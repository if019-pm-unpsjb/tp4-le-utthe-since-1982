#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define BUFSIZE 100

void fatal(const char *message) {
    perror(message);
    exit(1);
}

int main(int argc, char *argv[]) {
    int fd;
    struct sockaddr_in server_addr;
    char buffer[BUFSIZE];

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port> <message>\n", argv[0]);
        exit(1);
    }

    // Create socket
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        fatal("socket creation failed");
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    if (inet_aton(argv[1], &server_addr.sin_addr) == 0) {
        fatal("invalid server IP address");
    }

    // Send message to the server
    strncpy(buffer, argv[3], BUFSIZE);
    buffer[BUFSIZE - 1] = '\0'; // Ensure null-termination
    ssize_t sent_bytes = sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (sent_bytes == -1) {
        fatal("sendto failed");
    }

    printf("Message sent to %s:%s\n", argv[1], argv[2]);

    // Wait for the echoed message from the server
    struct sockaddr_in src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    ssize_t recv_bytes = recvfrom(fd, buffer, BUFSIZE - 1, 0, (struct sockaddr*)&src_addr, &src_addr_len);
    if (recv_bytes == -1) {
        fatal("recvfrom failed");
    }

    buffer[recv_bytes] = '\0'; // Ensure null-termination
    printf("Received message: %s\n", buffer);

    // Close socket
    close(fd);
    return 0;
}
