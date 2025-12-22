#include "src/solver.h"
#include <endian.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

bool recv_all(int socket, char *buffer, size_t length) {
  size_t total_received = 0;
  while (total_received < length) {
    ssize_t received =
        recv(socket, buffer + total_received, length - total_received, 0);
    if (received <= 0)
      return false;
    total_received += received;
  }
  return true;
}

void handle_client(int client) {
  uint64_t file_size = 0;

  if (!recv_all(client, reinterpret_cast<char *>(&file_size),
                sizeof(file_size))) {
    std::cerr << "Failed to receive file size" << std::endl;
    close(client);
    return;
  }

  file_size = be64toh(file_size);
  std::cout << "Allocating " << file_size << " bytes in memory..." << std::endl;

  std::vector<char> memory_buffer;
  try {
    memory_buffer.resize(file_size);
  } catch (const std::bad_alloc &e) {
    std::cerr << "File too large for available RAM!" << std::endl;
    close(client);
    return;
  }

  if (recv_all(client, memory_buffer.data(), file_size)) {
    std::cout << "Successfully received " << memory_buffer.size()
              << " bytes into RAM." << std::endl;

    std::string data_str(memory_buffer.begin(), memory_buffer.end());
    std::istringstream iss(data_str);
    // --- Process data here ---
    solver::ProblemInstance instance = solver::readInstance(iss);

    solver::Solution bestSolution =
        solver::simulatedAnnealing(instance, 100000, 1000.0, 0.995);

    std::ostringstream oss;
    for (size_t i = 0; i < bestSolution.schedule.size(); ++i) {
      oss << bestSolution.schedule[i];
      if (i < bestSolution.schedule.size() - 1) {
        oss << ","; // Only add comma if it's not the last element
      }
    }
    std::string csv_data = oss.str();

    uint64_t msg_len = htobe64(csv_data.size());

    // 3. Send length header
    // send(client, &msg_len, sizeof(msg_len), 0);

    // 4. Send the actual string data
    send(client, csv_data.c_str(), csv_data.size(), 0);

  } else {
    std::cerr << "Connection lost while receiving data." << std::endl;
  }

  //   char msg[100] = "Data received successfully";
  //   send(client, msg, sizeof(msg), 0);

  close(client);
}

int main() {

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(8080);
  addr.sin_addr.s_addr = INADDR_ANY;

  auto fd = bind(sock, (sockaddr *)&addr, sizeof(addr));
  listen(sock, 1);

  while (true) {
    int client = accept(sock, nullptr, nullptr);
    handle_client(client);
  }

  close(sock);
  // solver::ProblemInstance instance = solver::readInstance(instancePath);

  // solver::Solution bestSolution = solver::simulatedAnnealing(instance,
  // 100000, 1000.0, 0.995); std::cout << "Best TWT found: " <<
  // bestSolution.objectiveValue << std::endl;

  // solver::Solution insertionSolution =
  // solver::TWT_Insertion_Heuristic(instance); std::cout << "Insertion
  // Heuristic TWT: " << insertionSolution.objectiveValue << std::endl;

  return 0;
}
