#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

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

static int uid = 10;

int main(int argc, char *argv[])
{
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
    // server_addr.sin_addr.s_addr = INADDR_ANY;
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
                send_message(buffer, cli->uid);
                buffer[receive] = '\0'; // Ensure buffer is null-terminated
                // printf("%s -> %s\n", buffer, cli->name);
                printf("%s\n", buffer);
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