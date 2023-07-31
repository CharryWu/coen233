#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int server_fd;
    struct sockaddr_in address, client_addr;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    char *acknowledgment = "Message received by the server.";

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) == 0) {
        log_error("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to a specific IP and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        log_error("Binding failed");
        exit(EXIT_FAILURE);
    }

    log_info("Server listening on port %d...\n", PORT);

    // Receive data from the client
    int received_bytes = recvfrom(server_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, (socklen_t*)&addrlen);
    if (received_bytes <= 0) {
        log_error("Receive failed");
        exit(EXIT_FAILURE);
    }

    log_info("Received message from client: %s\n", buffer);

    // Send acknowledgment back to the client
    sendto(server_fd, acknowledgment, strlen(acknowledgment), 0, (struct sockaddr *)&client_addr, addrlen);

    log_info("Acknowledgment sent to the client.\n");

    close(server_fd);

    return 0;
}
