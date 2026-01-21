import sys
import socket
import struct
import re
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.backends.backend_qt5agg import NavigationToolbar2QT as NavigationToolbar
from PyQt5.QtWidgets import (
    QApplication,
    QMainWindow,
    QPushButton,
    QVBoxLayout,
    QHBoxLayout,
    QWidget,
    QFileDialog,
    QLabel,
    QLineEdit,
    QMessageBox,
    QTextEdit,
    QFrame,
)
from PyQt5.QtCore import QThread, pyqtSignal, pyqtSlot, Qt, QProcess


class SolverWorker(QThread):
    log_received = pyqtSignal(str)
    result_ready = pyqtSignal(list, float)
    error_occurred = pyqtSignal(str)
    finished_signal = pyqtSignal()

    def __init__(self, host, port, file_content):
        super().__init__()
        self.host = host
        self.port = port
        self.file_content = file_content

    def run(self):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(60)
                self.log_received.emit(f"Łączenie z {self.host}:{self.port}...")
                s.connect((self.host, self.port))

                data_bytes = self.file_content.encode("utf-8")
                s.sendall(struct.pack(">Q", len(data_bytes)))
                s.sendall(data_bytes)
                self.log_received.emit("Instancja wysłana. Rozpoczynanie obliczeń...")

                last_sequence = []
                last_objective = 0.0

                while True:
                    header = self.recv_exact(s, 8)
                    if not header:
                        break

                    if b"DONE" in header:
                        self.recv_exact(s, 42)
                        self.log_received.emit("--- ALGORYTM ZAKOŃCZONY (DONE) ---")
                        break

                    msg_len = struct.unpack(">Q", header)[0]
                    body_bytes = self.recv_exact(s, msg_len)
                    if not body_bytes:
                        break

                    body = body_bytes.decode("utf-8").strip()

                    parts = [p.strip() for p in body.split(",") if p.strip()]
                    if len(parts) >= 2:
                        last_objective = float(parts[-1])
                        last_sequence = [int(x) for x in parts[:-1]]

                        self.log_received.emit(
                            f"[Aktualizacja] Znaleziono TWT: {last_objective}"
                        )

                if last_sequence:
                    self.result_ready.emit(last_sequence, last_objective)

        except Exception as e:
            self.error_occurred.emit(f"Błąd: {str(e)}")
        finally:
            self.finished_signal.emit()

    def recv_exact(self, sock, n):
        data = b""
        while len(data) < n:
            packet = sock.recv(n - len(data))
            if not packet:
                return None
            data += packet
        return data


class GanttCanvas(FigureCanvas):
    def __init__(self, parent=None):
        self.fig, self.ax = plt.subplots(figsize=(10, 5))
        super().__init__(self.fig)

    def plot_gantt(self, sequence, process_times, setup_times):
        self.ax.clear()
        current_time = 0
        prev_job = -1
        y_pos, bar_height = 10, 8
        cmap = plt.get_cmap("tab20")

        for i, job in enumerate(sequence):
            st = setup_times.get((prev_job, job), 0)
            if st > 0:
                self.ax.broken_barh(
                    [(current_time, st)],
                    (y_pos, bar_height),
                    facecolors="lightgrey",
                    hatch="///",
                    alpha=0.4,
                )
                current_time += st

            if job < len(process_times):
                duration = process_times[job]
                color = cmap(job % 20)
                self.ax.broken_barh(
                    [(current_time, duration)],
                    (y_pos, bar_height),
                    facecolors=color,
                    edgecolors="black",
                    linewidth=0.5,
                )

                self.ax.text(
                    current_time + duration / 2,
                    y_pos + bar_height + 0.5,
                    f"J{job}",
                    ha="center",
                    va="bottom",
                    rotation=45,
                    fontsize=8,
                )

                current_time += duration
                prev_job = job

        self.ax.set_ylim(0, 32)
        self.ax.set_title(
            "Ostateczny Harmonogram (Gantt)", pad=30, fontsize=14, fontweight="bold"
        )
        self.ax.set_xlabel("Czas [jednostki]")
        self.ax.set_yticks([])
        self.ax.grid(True, axis="x", linestyle=":", alpha=0.6)
        self.fig.tight_layout()
        self.draw()


class SolverApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("TWT Solver Client")
        self.resize(1300, 800)
        self.file_content = ""
        self.process_times = []
        self.setup_times = {}
        self.init_ui()

        self.server_process = QProcess(self)

        executable_path = "/home/dam900/studia/drugi_stopien/ziwpp/build/app"

        self.server_process.start(executable_path)

        self.server_process.errorOccurred.connect(self.handle_error)

    def init_ui(self):
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        layout = QVBoxLayout(main_widget)

        # Panel Górny sterowania
        top_panel = QFrame()
        top_panel.setFrameShape(QFrame.StyledPanel)
        top_layout = QHBoxLayout(top_panel)

        self.btn_load = QPushButton("1. Wczytaj Instancję")
        self.btn_load.clicked.connect(self.load_file)

        self.host_input = QLineEdit("127.0.0.1")
        self.port_input = QLineEdit("8080")
        self.btn_solve = QPushButton("2. Uruchom Solver")
        self.btn_solve.setEnabled(False)
        self.btn_solve.clicked.connect(self.start_solving)
        self.btn_solve.setStyleSheet("background-color: #d4edda; font-weight: bold;")

        top_layout.addWidget(self.btn_load)
        top_layout.addStretch()
        top_layout.addWidget(QLabel("IP:"))
        top_layout.addWidget(self.host_input)
        top_layout.addWidget(QLabel("Port:"))
        top_layout.addWidget(self.port_input)
        top_layout.addWidget(self.btn_solve)
        layout.addWidget(top_panel)

        # Panel Środkowy (Wykres po lewej, Logi po prawej)
        mid_layout = QHBoxLayout()

        # Wykres
        plot_box = QVBoxLayout()
        self.canvas = GanttCanvas(self)
        self.toolbar = NavigationToolbar(self.canvas, self)
        self.obj_label = QLabel("Objective Value (TWT): ---")
        self.obj_label.setStyleSheet(
            "font-size: 20px; font-weight: bold; color: #155724; padding: 10px;"
        )
        self.obj_label.setAlignment(Qt.AlignCenter)

        plot_box.addWidget(self.toolbar)
        plot_box.addWidget(self.canvas, stretch=1)
        plot_box.addWidget(self.obj_label)
        mid_layout.addLayout(plot_box, stretch=3)

        log_box = QVBoxLayout()
        log_box.addWidget(QLabel("Przebieg optymalizacji:"))
        self.log_display = QTextEdit()
        self.log_display.setReadOnly(True)
        self.log_display.setStyleSheet(
            "background-color: #1e1e1e; color: #00ff00; font-family: 'Consolas'; font-size: 10pt;"
        )
        self.log_display.setFixedWidth(350)
        log_box.addWidget(self.log_display)
        mid_layout.addLayout(log_box, stretch=1)

        layout.addLayout(mid_layout)

    def load_file(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Wybierz plik", "", "Instance (*.instance)"
        )
        if path:
            with open(path, "r") as f:
                self.file_content = f.read()
            if self.parse_instance(self.file_content):
                self.log_display.append(f"Wczytano: {path.split('/')[-1]}")
                self.btn_solve.setEnabled(True)

    def parse_instance(self, content):
        try:
            pt = re.search(r"Process Times:\s*([\d\s]+)", content, re.DOTALL)
            self.process_times = [int(x) for x in pt.group(1).split()]
            self.setup_times = {}
            st = re.search(r"Setup Times:\s*(.*)", content, re.DOTALL)
            if st:
                for line in st.group(1).strip().split("\n"):
                    parts = line.split()
                    if len(parts) >= 3:
                        u, v, t = map(int, parts[:3])
                        self.setup_times[(u, v)] = t
            return True
        except:
            return False

    def start_solving(self):
        self.log_display.clear()
        self.btn_solve.setEnabled(False)
        self.worker = SolverWorker(
            self.host_input.text(), int(self.port_input.text()), self.file_content
        )
        self.worker.log_received.connect(lambda m: self.log_display.append(m))
        self.worker.result_ready.connect(self.handle_final_result)
        self.worker.error_occurred.connect(
            lambda e: QMessageBox.critical(self, "Błąd", e)
        )
        self.worker.finished_signal.connect(lambda: self.btn_solve.setEnabled(True))
        self.worker.start()

    @pyqtSlot(list, float)
    def handle_final_result(self, sequence, objective):
        self.obj_label.setText(f"Final TWT: {objective}")
        self.canvas.plot_gantt(sequence, self.process_times, self.setup_times)

    def handle_error(self, error):
        print(f"Process error: {error}")

    def closeEvent(self, event):
        self.server_process.terminate()
        self.server_process.waitForFinished()
        event.accept()


if __name__ == "__main__":

    socket_path = "/home/dam900/studia/drugi_stopien/ziwpp/build/app"
    app = QApplication(sys.argv)
    window = SolverApp()
    window.show()
    sys.exit(app.exec_())
