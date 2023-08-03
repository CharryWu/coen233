#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "log.h"

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char *message = "Hello, server!";
    char buffer[BUFFER_SIZE] = {0};
    int valread, addrlen = sizeof(serv_addr);

    // Create socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        exit(EXIT_FAILURE);
    }

    // Send data to the server
    sendto(sock, message, strlen(message), 0, (struct sockaddr *)&serv_addr, addrlen);
    printf("Message sent to the server: %s\n", message);

    // Receive acknowledgment from the server
    valread = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&serv_addr, (socklen_t*)&addrlen);
    if (valread <= 0) {
        perror("Receive failed");
        exit(EXIT_FAILURE);
    }

    printf("Server response: %s\n", buffer);

    close(sock);

    return 0;
}
