#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/time.h>

#define BUF_SIZE 516
#define DATA_SIZE 512

#define OP_RRQ 1
#define OP_WRQ 2
#define OP_DATA 3
#define OP_ACK 4
#define OP_ERROR 5

void send_rrq(int sockfd, struct sockaddr_in *server_addr, socklen_t server_len, const char *filename, const char *mode)
{
    char buffer[BUF_SIZE];
    int msg_len = snprintf(buffer, BUF_SIZE, "%c%c%s%c%s%c", 0, OP_RRQ, filename, 0, mode, 0);
    sendto(sockfd, buffer, msg_len, 0, (struct sockaddr *)server_addr, server_len);
}

void send_wrq(int sockfd, struct sockaddr_in *server_addr, socklen_t server_len, const char *filename, const char *mode)
{
    char buffer[BUF_SIZE];
    int msg_len = snprintf(buffer, BUF_SIZE, "%c%c%s%c%s%c", 0, OP_WRQ, filename, 0, mode, 0);
    sendto(sockfd, buffer, msg_len, 0, (struct sockaddr *)server_addr, server_len);
}

void send_ack(int sockfd, struct sockaddr_in *server_addr, socklen_t server_len, int block_num)
{
    char ack_packet[4] = {0, OP_ACK, (block_num >> 8) & 0xFF, block_num & 0xFF};
    sendto(sockfd, ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)server_addr, server_len);
}

void receive_file(int sockfd, struct sockaddr_in *server_addr, socklen_t server_len, const char *filename)
{
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    char buffer[BUF_SIZE];
    int block_num = 0;
    ssize_t bytes_received;

    while (1)
    {
        bytes_received = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *)server_addr, &server_len);
        if (bytes_received < 0)
        {
            perror("recvfrom");
            close(fd);
            exit(EXIT_FAILURE);
        }

        int opcode = buffer[1];
        int recv_block_num = (buffer[2] << 8) | buffer[3];

        if (opcode == OP_DATA && recv_block_num == block_num + 1)
        {
            write(fd, buffer + 4, bytes_received - 4);
            send_ack(sockfd, server_addr, server_len, recv_block_num);
            block_num++;

            if (bytes_received < BUF_SIZE)
            {
                // Last packet received
                break;
            }
        }
        else if (opcode == OP_ERROR)
        {
            fprintf(stderr, "Error from server: %s\n", buffer + 4);
            close(fd);
            exit(EXIT_FAILURE);
        }
        else
        {
            fprintf(stderr, "Unexpected packet received\n");
            close(fd);
            exit(EXIT_FAILURE);
        }
    }

    close(fd);
}

void send_file(int sockfd, struct sockaddr_in *server_addr, socklen_t server_len, const char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    char buffer[BUF_SIZE];
    char data_block[DATA_SIZE];
    int block_num = 0;
    ssize_t bytes_read, bytes_sent, bytes_received;
    struct timeval timeout;
    timeout.tv_sec = 1;  // 1 second timeout
    timeout.tv_usec = 0;

    // Wait for initial ACK from server
    while (1)
    {
        bytes_received = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *)server_addr, &server_len);
        if (bytes_received < 0)
        {
            perror("recvfrom");
            close(fd);
            exit(EXIT_FAILURE);
        }

        int opcode = buffer[1];
        if (opcode == OP_ACK)
        {
            int recv_block_num = (buffer[2] << 8) | buffer[3];
            if (recv_block_num == block_num)
            {
                printf("Initial ACK received\n");
                break;
            }
        }
    }

    block_num = 1;
    do
    {
        bytes_read = read(fd, data_block, DATA_SIZE);
        if (bytes_read < 0)
        {
            perror("read");
            close(fd);
            exit(EXIT_FAILURE);
        }

        snprintf(buffer, BUF_SIZE, "%c%c%c%c", 0, OP_DATA, block_num >> 8, block_num & 0xFF);
        memcpy(buffer + 4, data_block, bytes_read);

        while (1)
        {
            bytes_sent = sendto(sockfd, buffer, bytes_read + 4, 0, (struct sockaddr *)server_addr, server_len);
            if (bytes_sent < 0)
            {
                perror("sendto");
                close(fd);
                exit(EXIT_FAILURE);
            }

            printf("Block %d sent, waiting for ACK\n", block_num);

            // Set socket timeout for ACK
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

            bytes_received = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *)server_addr, &server_len);
            if (bytes_received < 0)
            {
                perror("recvfrom");
                printf("Retrying block %d\n", block_num);
                continue;
            }

            int opcode = buffer[1];
            int recv_block_num = (buffer[2] << 8) | buffer[3];

            if (opcode == OP_ACK && recv_block_num == block_num)
            {
                printf("Valid ACK for block %d received\n", block_num);
                block_num++;
                break;
            }
            else if (opcode == OP_ERROR)
            {
                fprintf(stderr, "Error from server: %s\n", buffer + 4);
                close(fd);
                exit(EXIT_FAILURE);
            }
            else
            {
                fprintf(stderr, "Unexpected packet received\n");
                close(fd);
                exit(EXIT_FAILURE);
            }
        }
    } while (bytes_read == DATA_SIZE);

    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s <server_ip> <server_port> <filename> <mode>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    const char *filename = argv[3];
    const char *mode = argv[4];

    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Depending on the mode, perform RRQ or WRQ
    if (strcmp(mode, "r") == 0) // Read mode
    {
        send_rrq(sockfd, &server_addr, server_len, filename, "octet");
        receive_file(sockfd, &server_addr, server_len, filename);
    }
    else if (strcmp(mode, "w") == 0) // Write mode
    {
        send_wrq(sockfd, &server_addr, server_len, filename, "octet");
        send_file(sockfd, &server_addr, server_len, filename);
    }
    else
    {
        fprintf(stderr, "Invalid mode. Use 'r' for read or 'w' for write.\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    close(sockfd);
    return 0;
}
