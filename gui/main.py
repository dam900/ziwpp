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
    QSizePolicy,
)


class GanttCanvas(FigureCanvas):
    """Klasa do renderowania wykresu Gantta wewnątrz PyQt5."""

    def __init__(self, parent=None):
        self.fig, self.ax = plt.subplots(figsize=(10, 5))
        super().__init__(self.fig)
        self.setParent(parent)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.updateGeometry()
        # Paleta kolorów dla zadań
        self.colors = list(mcolors.TABLEAU_COLORS.values())

    def plot_gantt(self, sequence, process_times, setup_times):
        self.ax.clear()

        current_time = 0
        prev_job = -1  # -1 oznacza stan początkowy (start)

        y_pos = 10
        bar_height = 8

        for i, job in enumerate(sequence):
            # Zabezpieczenie na wypadek błędnego ID zadania z serwera
            if job < 0 or job >= len(process_times):
                print(f"Ostrzeżenie: Otrzymano nieprawidłowe ID zadania: {job}")
                continue

            # 1. Pobierz i narysuj czas przezbrojenia (setup time)
            setup_time = setup_times.get((prev_job, job), 0)

            if setup_time > 0:
                self.ax.broken_barh(
                    [(current_time, setup_time)],
                    (y_pos, bar_height),
                    facecolors="lightgrey",
                    hatch="///",
                    alpha=0.6,
                    label="Przezbrojenie" if i == 0 else "",
                )
                current_time += setup_time

            # 2. Pobierz czas trwania i narysuj zadanie
            duration = process_times[job]
            task_color = self.colors[
                job % len(self.colors)
            ]  # Przypisz kolor cyklicznie

            self.ax.broken_barh(
                [(current_time, duration)],
                (y_pos, bar_height),
                facecolors=task_color,
                edgecolors="black",
                alpha=0.9,
            )

            # 3. Dodaj etykietę (nad paskiem, obrócona, żeby nie nachodziła)
            label_x = current_time + duration / 2
            label_y = y_pos + bar_height + 0.5  # Trochę nad paskiem

            self.ax.text(
                label_x,
                label_y,
                f"Zad {job}",
                ha="center",
                va="bottom",
                rotation=45,
                fontsize=9,
            )

            current_time += duration
            prev_job = job

        # Ustawienia osi
        self.ax.set_xlabel("Czas [jednostki]", fontsize=12)
        self.ax.set_yticks([])  # Ukryj oś Y
        self.ax.set_title(
            "Harmonogram Zadań (Wykres Gantta)", fontsize=14, fontweight="bold"
        )
        self.ax.grid(True, axis="x", linestyle="--", alpha=0.7)

        # Ustawienie limitów, żeby wykres ładnie wyglądał
        self.ax.set_ylim(5, 25)
        self.ax.set_xlim(
            left=0, right=max(current_time, 1) * 1.05
        )  # Lekki margines z prawej

        # Dodanie legendy (tylko dla setupu, bo zadania są opisane)
        handles, labels = self.ax.get_legend_handles_labels()
        if handles:
            self.ax.legend(handles[:1], labels[:1], loc="upper right")

        self.fig.tight_layout()
        self.draw()


class SolverApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Harmonogramowanie Zadań - GUI Klient")
        self.resize(1200, 700)

        # Dane instancji
        self.file_content = ""
        self.process_times = []
        self.setup_times = {}

        self.init_ui()

    def init_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)

        # --- Panel kontrolny (Góra) ---
        control_panel = QWidget()
        control_panel.setStyleSheet(
            "background-color: #f0f0f0; border-radius: 5px; padding: 5px;"
        )
        top_layout = QHBoxLayout(control_panel)

        self.btn_load = QPushButton("1. Wczytaj Plik Instancji")
        self.btn_load.setStyleSheet("padding: 5px; font-weight: bold;")
        self.btn_load.clicked.connect(self.load_file)

        self.label_file = QLabel("Brak pliku")
        self.label_file.setStyleSheet("color: gray; margin-left: 10px;")

        top_layout.addWidget(self.btn_load)
        top_layout.addWidget(self.label_file)
        top_layout.addStretch()

        top_layout.addWidget(QLabel("IP Serwera:"))
        self.input_host = QLineEdit("127.0.0.1")
        self.input_host.setMaximumWidth(100)
        top_layout.addWidget(self.input_host)

        top_layout.addWidget(QLabel("Port:"))
        self.input_port = QLineEdit("8080")
        self.input_port.setMaximumWidth(60)
        top_layout.addWidget(self.input_port)

        self.btn_solve = QPushButton("2. Wyślij i Rysuj")
        self.btn_solve.setStyleSheet(
            "padding: 5px; font-weight: bold; background-color: #d4edda; color: #155724;"
        )
        self.btn_solve.clicked.connect(self.send_to_server)
        self.btn_solve.setEnabled(False)
        top_layout.addWidget(self.btn_solve)

        layout.addWidget(control_panel)

        # --- Wykres i Nawigacja (Środek) ---
        self.canvas = GanttCanvas(self)
        # DODANO: Pasek narzędzi nawigacyjnych Matplotlib
        self.toolbar = NavigationToolbar(self.canvas, self)

        layout.addWidget(self.toolbar)
        layout.addWidget(self.canvas, stretch=1)

    def load_file(self):
        path, _ = QFileDialog.getOpenFileName(
            self,
            "Wybierz plik instancji",
            "",
            "Instance Files (*.instance);;Text Files (*.txt);;All Files (*)",
        )
        if path:
            try:
                with open(path, "r") as f:
                    self.file_content = f.read()

                if self.parse_instance(self.file_content):
                    self.label_file.setText(
                        f"Wczytano: {path.split('/')[-1]} (Zadań: {len(self.process_times)})"
                    )
                    self.label_file.setStyleSheet("color: green; margin-left: 10px;")
                    self.btn_solve.setEnabled(True)
                    QMessageBox.information(
                        self,
                        "Sukces",
                        f"Pomyślnie wczytano dane dla {len(self.process_times)} zadań.",
                    )
                else:
                    raise ValueError(
                        "Plik nie zawiera wymaganych sekcji (Process Times/Setup Times)."
                    )

            except Exception as e:
                QMessageBox.critical(
                    self,
                    "Błąd parsowania",
                    f"Nie udało się przetworzyć pliku:\n{str(e)}",
                )
                self.label_file.setText("Błąd wczytywania")
                self.label_file.setStyleSheet("color: red; margin-left: 10px;")
                self.btn_solve.setEnabled(False)

    def parse_instance(self, content):
        """Parsuje czasy procesów i przezbrojeń."""
        self.process_times = []
        self.setup_times = {}
        try:
            # Parsowanie czasów trwania (Process Times)
            pt_match = re.search(r"Process Times:\s*([\d\s]+)", content, re.DOTALL)
            if pt_match:
                self.process_times = [int(x) for x in pt_match.group(1).split()]
            else:
                print("Brak sekcji Process Times")
                return False

            # Parsowanie czasów przezbrojeń (Setup Times) - opcjonalne, ale zalecane
            st_match = re.search(r"Setup Times:\s*(.*)", content, re.DOTALL)
            if st_match:
                lines = st_match.group(1).strip().split("\n")
                for line in lines:
                    parts = line.split()
                    if len(parts) >= 3:  # Może być więcej kolumn, bierzemy pierwsze 3
                        try:
                            u, v, t = map(int, parts[:3])
                            self.setup_times[(u, v)] = t
                        except ValueError:
                            continue
            return True
        except Exception as e:
            print(f"Wyjątek podczas parsowania: {e}")
            return False

    def send_to_server(self):
        host = self.input_host.text()
        try:
            port = int(self.input_port.text())
        except ValueError:
            QMessageBox.warning(self, "Błąd", "Port musi być liczbą.")
            return

        if not self.process_times:
            QMessageBox.warning(
                self, "Błąd", "Najpierw wczytaj poprawny plik instancji."
            )
            return

        # Prosty stan ładowania
        self.btn_solve.setText("Oczekiwanie na serwer...")
        self.btn_solve.setEnabled(False)
        QApplication.processEvents()

        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(30)  # Większy timeout na obliczenia
                print(f"Łączenie z {host}:{port}...")
                s.connect((host, port))

                # Przygotowanie danych (rozmiar big-endian + treść)
                data_bytes = self.file_content.encode("utf-8")
                file_size = len(data_bytes)
                header = struct.pack(">Q", file_size)

                print(f"Wysyłanie {file_size} bajtów...")
                s.sendall(header)
                s.sendall(data_bytes)

                print("Oczekiwanie na odpowiedź...")
                # Odbieranie wyniku (zakładamy, że wynik to string z ID po przecinku)
                response = s.recv(8192).decode("utf-8").strip()
                print(f"Otrzymano: {response}")

                if not response:
                    raise ValueError(
                        "Serwer zwrócił pustą odpowiedź lub rozłączył się."
                    )

                # Konwersja wyniku "0,1,2,3..." na listę int
                try:
                    sequence = [
                        int(x.strip()) for x in response.split(",") if x.strip()
                    ]
                except ValueError:
                    raise ValueError(
                        f"Serwer zwrócił dane w złym formacie: '{response}'. Oczekiwano np. '0, 2, 1'."
                    )

                if len(sequence) != len(self.process_times):
                    QMessageBox.warning(
                        self,
                        "Ostrzeżenie",
                        f"Liczba zadań w instancji ({len(self.process_times)}) różni się od wyniku z serwera ({len(sequence)}). Wykres może być niepełny.",
                    )

                # Rysowanie wykresu
                self.canvas.plot_gantt(sequence, self.process_times, self.setup_times)

        except socket.timeout:
            QMessageBox.critical(
                self,
                "Błąd Połączenia",
                "Serwer nie odpowiedział w określonym czasie (timeout).",
            )
        except socket.error as e:
            QMessageBox.critical(
                self, "Błąd Połączenia", f"Nie można połączyć się z serwerem: {e}"
            )
        except Exception as e:
            QMessageBox.critical(self, "Błąd", f"Wystąpił problem: {str(e)}")
        finally:
            self.btn_solve.setText("2. Wyślij i Rysuj")
            self.btn_solve.setEnabled(True)


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = SolverApp()
    window.show()
    sys.exit(app.exec_())
