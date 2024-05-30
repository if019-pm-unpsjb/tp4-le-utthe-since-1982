#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define SERVER_PORT 8888
#define BUF_SIZE 516
#define DATA_SIZE 512

#define OP_RRQ 1
#define OP_WRQ 2
#define OP_DATA 3
#define OP_ACK 4
#define OP_ERROR 5

#define ERR_NOT_FOUND 1
#define ERR_ACCESS_VIOLATION 2
#define ERR_ILLEGAL_OPERATION 4

void send_error(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len, int error_code, const char *error_msg)
{
    char buffer[BUF_SIZE];
    int msg_len = snprintf(buffer, BUF_SIZE, "%c%c%c%c%s%c", 0, OP_ERROR, 0, error_code, error_msg, 0);
    sendto(sockfd, buffer, msg_len, 0, (struct sockaddr *)client_addr, client_len);
}

void handle_rrq(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len, char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        send_error(sockfd, client_addr, client_len, ERR_NOT_FOUND, "File not found");
        return;
    }

    char buffer[BUF_SIZE];
    char data_block[DATA_SIZE];
    int block_num = 1;
    ssize_t bytes_read, bytes_sent;

    do
    {
        bytes_read = read(fd, data_block, DATA_SIZE);
        if (bytes_read < 0)
        {
            send_error(sockfd, client_addr, client_len, ERR_ACCESS_VIOLATION, "Access violation");
            close(fd);
            return;
        }

        snprintf(buffer, BUF_SIZE, "%c%c%c%c", 0, OP_DATA, block_num >> 8, block_num & 0xFF);
        memcpy(buffer + 4, data_block, bytes_read);

        bytes_sent = sendto(sockfd, buffer, bytes_read + 4, 0, (struct sockaddr *)client_addr, client_len);
        if (bytes_sent < 0)
        {
            perror("sendto");
            close(fd);
            return;
        }

        block_num++;

        // Wait for ACK
        char ack_buffer[BUF_SIZE];
        recvfrom(sockfd, ack_buffer, BUF_SIZE, 0, (struct sockaddr *)client_addr, &client_len);
        // Here we should validate the ACK, but for simplicity, we assume it’s correct.

    } while (bytes_read == DATA_SIZE);

    close(fd);
}

void handle_wrq(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len, char *filename)
{
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        send_error(sockfd, client_addr, client_len, ERR_ACCESS_VIOLATION, "Access violation");
        return;
    }

    char buffer[BUF_SIZE];
    char ack_buffer[4] = {0, OP_ACK, 0, 0};
    int block_num = 0;
    ssize_t bytes_received, bytes_written;

    // Send initial ACK for WRQ
    sendto(sockfd, ack_buffer, 4, 0, (struct sockaddr *)client_addr, client_len);

    while (1)
    {
        bytes_received = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *)client_addr, &client_len);
        if (bytes_received < 0)
        {
            perror("recvfrom");
            close(fd);
            return;
        }

        int opcode = buffer[1];
        // int recv_block_num = (unsigned int)(buffer[2] << 8) | (unsigned int)buffer[3];
        int recv_block_num = ((buffer[2] << 8) | (buffer[3] & 0x0FF));
        if (opcode == OP_DATA && recv_block_num == block_num + 1)
        {
            bytes_written = write(fd, buffer + 4, bytes_received - 4);
            if (bytes_written < 0)
            {
                perror("write");
                close(fd);
                return;
            }

            block_num++;
            ack_buffer[2] = buffer[2];
            ack_buffer[3] = buffer[3];
            sendto(sockfd, ack_buffer, 4, 0, (struct sockaddr *)client_addr, client_len);

            if (bytes_received < BUF_SIZE)
            { // Last packet
                break;
            }
        }
        else if (opcode == OP_ERROR)
        {
            fprintf(stderr, "Error from client: %s\n", buffer + 4);
            close(fd);
            return;
        }
        else
        {
            printf("rec block num: %d\n", recv_block_num);
        }
    }

    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];

    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUF_SIZE];
    socklen_t client_len = sizeof(client_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Listening on %s:%d ...\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

    while (1)
    {
        ssize_t n = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);
        if (n < 0)
        {
            perror("recvfrom");
            continue;
        }

        buffer[n] = '\0';

        int opcode = buffer[1];
        char *filename = buffer + 2;
        char *mode = filename + strlen(filename) + 1;

        if (opcode == OP_RRQ)
        {
            printf("RRQ from %s: %s (%s)\n", inet_ntoa(client_addr.sin_addr), filename, mode);
            handle_rrq(sockfd, &client_addr, client_len, filename);
        }
        else if (opcode == OP_WRQ)
        {
            printf("WRQ from %s: %s (%s)\n", inet_ntoa(client_addr.sin_addr), filename, mode);
            handle_wrq(sockfd, &client_addr, client_len, filename);
        }
        else
        {
            send_error(sockfd, &client_addr, client_len, ERR_ILLEGAL_OPERATION, "Illegal TFTP operation");
        }
    }

    close(sockfd);
    return 0;
}
