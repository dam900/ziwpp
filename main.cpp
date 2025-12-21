#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

void handle_client(int client) {
  char buffer[1024] = {0};
  int bytes_received = recv(client, buffer, sizeof(buffer) - 1, 0);

  if (bytes_received > 0) {
    buffer[bytes_received] = '\0';
    std::cout << "Received: " << buffer << std::endl;
    send(client, buffer, bytes_received, 0);
  }

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
