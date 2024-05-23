#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>

#define PORT 8888
#define IP   "127.0.0.1"
#define BUFSIZE 100

static int fd;

void fatal(const char *message) {
    perror(message);
    exit(1);
}

void handler(int signal) {
    if (fd != -1) {
        close(fd);
    }
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    struct sockaddr_in addr;

    // Configure signal handling for SIGTERM
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Create the socket
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        fatal("socket creation failed");
    }

    // Configure the address to bind to
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    if (argc == 3) {
        addr.sin_port = htons((uint16_t) atoi(argv[2]));
        if (inet_aton(argv[1], &(addr.sin_addr)) == 0) {
            fprintf(stderr, "Invalid IP address: %s\n", argv[1]);
            exit(EXIT_FAILURE);
        }
    } else {
        addr.sin_port = htons(PORT);
        if (inet_aton(IP, &(addr.sin_addr)) == 0) {
            perror("inet_aton");
            exit(EXIT_FAILURE);
        }
    }

    // Set socket options
    int optval = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Bind the socket
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("Listening on %s:%d ...\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    char buf[BUFSIZE];
    struct sockaddr_in src_addr;
    socklen_t src_addr_len;

    while (1) {
        memset(&src_addr, 0, sizeof(struct sockaddr_in));
        src_addr_len = sizeof(struct sockaddr_in);

        // Receive a message
        ssize_t n = recvfrom(fd, buf, BUFSIZE - 1, 0, (struct sockaddr*)&src_addr, &src_addr_len);
        if (n == -1) {
            perror("recvfrom");
            exit(EXIT_FAILURE);
        }

        buf[n] = '\0'; // Ensure null-termination

        // Print sender's address and received message
        printf("[%s:%d] %s\n", inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), buf);

        // Send the received message back to the client
        ssize_t sent_bytes = sendto(fd, buf, n, 0, (struct sockaddr*)&src_addr, src_addr_len);
        if (sent_bytes == -1) {
            perror("sendto");
            exit(EXIT_FAILURE);
        }
    }

    // Close the socket
    close(fd);
    return 0;
}
