#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define DELIMITER '#'

typedef struct
{
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[32];
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void add_client(client_t *cl);
void remove_client(int uid);
void send_message(char *s, int uid);
void *handle_client(void *arg);
void receive_file(int sockfd, const char *filename);
void send_file(const char *filename, int uid);

static int uid = 10;

void handler(int signal)
{
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    signal(SIGTERM, handler);
    // Give the IP as a parameter
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    const char *server_ip = argv[1];

    int sockfd, newsockfd;
    struct sockaddr_in server_addr, client_addr;
    pthread_t tid;

    // Socket settings
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(PORT);

    // Bind
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("ERROR: Bind failed");
        return EXIT_FAILURE;
    }

    // Listen
    if (listen(sockfd, 10) < 0)
    {
        perror("ERROR: Socket listen");
        return EXIT_FAILURE;
    }

    printf("Listening on: %s:%d\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

    while (1)
    {
        socklen_t clilen = sizeof(client_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &clilen);

        // Client settings
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = client_addr;
        cli->sockfd = newsockfd;
        cli->uid = uid++;

        // Add client to the queue and fork thread
        add_client(cli);
        pthread_create(&tid, NULL, &handle_client, (void *)cli);

        // Reduce CPU usage
        sleep(1);
    }

    return EXIT_SUCCESS;
}

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void add_client(client_t *cl)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (!clients[i])
        {
            clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int uid)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid == uid)
            {
                clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void send_message(char *s, int uid)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid != uid)
            {
                if (write(clients[i]->sockfd, s, strlen(s)) < 0)
                {
                    perror("ERROR: Write to descriptor failed");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void send_file(const char *filename, int uid)
{
    char ready_msg[6];
    off_t offset = 0;
    ssize_t sent_bytes = 0;
    size_t total_sent = 0;

    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        perror("open failed");
    }

    struct stat file_stat;
    off_t file_size;
    if (stat(filename, &file_stat) == 0)
    {
        file_size = file_stat.st_size;
    }
    else
    {
        perror("stat");
    }
    char file_size_str[20]; // Adjust size based on your integer range
    sprintf(file_size_str, "%c%ld", DELIMITER, file_size);

    size_t filename_length = strlen(filename);

    pthread_mutex_lock(&clients_mutex);

    printf("Trying to send: %s %ld\n", filename, file_size);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid != uid)
            {
                if (send(clients[i]->sockfd, "SENDING_FILE", strlen("SENDING_FILE"), 0) == -1)
                {
                    perror("ERROR: Write to descriptor failed");
                    break;
                }

                // Send file name
                if (send(clients[i]->sockfd, filename, filename_length, 0) == -1)
                {
                    perror("send");
                }

                // Send file size
                if (send(clients[i]->sockfd, file_size_str, strlen(file_size_str), 0) == -1)
                {
                    perror("send");
                    break;
                }

                // Receive the signal to start sending the file
                recv(clients[i]->sockfd, ready_msg, 5, 0);
                ready_msg[5] = '\0';
                // Send the file IF the signal arrived
                if (strcmp(ready_msg, "ready") == 0)
                {
                    printf("Ready to send the file.\n");
                    // Send the file
                    while (total_sent < file_size)
                    {
                        sent_bytes = sendfile(clients[i]->sockfd, fd, &offset, file_size - total_sent);
                        if (sent_bytes == -1)
                        {
                            perror("sendfile");
                            close(fd);
                            exit(EXIT_FAILURE);
                        }
                        total_sent += sent_bytes;
                    }
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void receive_file(int sockfd, const char *filename)
{
    char buffer[BUFFER_SIZE];
    int n;

    // Create file
    int file_fd = open(filename, O_WRONLY | O_CREAT, 0666);
    if (file_fd < 0)
    {
        perror("ERROR: File open");
        return;
    }

    // Receive file size
    bzero(buffer, BUFFER_SIZE);
    n = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if (n <= 0)
    {
        perror("ERROR: Receive file size");
        close(file_fd);
        return;
    }
    long file_size = atol(buffer);
    long total_received = 0;

    // Send acknowledgment
    send(sockfd, "SIZE RECEIVED", strlen("SIZE RECEIVED"), 0);

    // Receive file data
    while (total_received < file_size)
    {
        n = recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (n <= 0)
        {
            break;
        }
        if (write(file_fd, buffer, n) < 0)
        {
            perror("ERROR: Write to file");
            break;
        }
        total_received += n;
    }

    if (total_received < file_size)
    {
        perror("ERROR: Incomplete file received");
    }
    printf("File received successfully\n");
    close(file_fd);
}

void *handle_client(void *arg)
{
    char buffer[BUFFER_SIZE];
    char name[32];
    int leave_flag = 0;

    client_t *cli = (client_t *)arg;

    // Name
    if (recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32 - 1)
    {
        printf("Enter the name correctly\n");
        leave_flag = 1;
    }
    else
    {
        strcpy(cli->name, name);
        sprintf(buffer, "%s has joined\n", cli->name);
        printf("%s", buffer);
        send_message(buffer, cli->uid);
    }

    bzero(buffer, BUFFER_SIZE);

    while (1)
    {
        if (leave_flag)
        {
            break;
        }

        int receive = recv(cli->sockfd, buffer, BUFFER_SIZE, 0);
        if (receive > 0)
        {
            if (strlen(buffer) > 0)
            {
                if (strncmp(buffer, "file: ", 6) == 0)
                {
                    char *filename = buffer + 6;
                    printf("Receiving file: %s\n", filename);
                    receive_file(cli->sockfd, filename);
                    send_file(filename, cli->uid);
                }
                else
                {
                    send_message(buffer, cli->uid);
                    buffer[receive] = '\0'; // Ensure buffer is null-terminated
                    printf("%s\n", buffer);
                }
            }
        }
        else if (receive == 0 || strcmp(buffer, "exit") == 0)
        {
            sprintf(buffer, "%s has left\n", cli->name);
            printf("%s", buffer);
            send_message(buffer, cli->uid);
            leave_flag = 1;
        }
        else
        {
            printf("ERROR: -1\n");
            leave_flag = 1;
        }

        bzero(buffer, BUFFER_SIZE);
    }

    close(cli->sockfd);
    remove_client(cli->uid);
    free(cli);
    pthread_detach(pthread_self());

    return NULL;
}
