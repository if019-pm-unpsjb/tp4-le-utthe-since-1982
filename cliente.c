#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUF_SIZE 4096

void fatal(const char *message) {
    perror(message);
    exit(1);
}

int main(int argc, char **argv) {
    int c, s, bytes;
    char buf[BUF_SIZE];
    struct hostent *h;
    struct sockaddr_in channel;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_name> <server_port> <file_name>\n", argv[0]);
        exit(1);
    }

    int server_port = atoi(argv[2]);
    if (server_port <= 0) {
        fprintf(stderr, "Invalid port number: %s\n", argv[2]);
        exit(1);
    }

    h = gethostbyname(argv[1]);
    if (!h) {
        fatal("gethostbyname failed");
    }

    s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        fatal("socket creation failed");
    }

    memset(&channel, 0, sizeof(channel));
    channel.sin_family = AF_INET;
    memcpy(&channel.sin_addr.s_addr, h->h_addr_list[0], h->h_length);
    channel.sin_port = htons(server_port);

    c = connect(s, (struct sockaddr *)&channel, sizeof(channel));
    if (c < 0) {
        fatal("connection failed");
    }

    write(s, argv[3], strlen(argv[3]) + 1);

    while ((bytes = read(s, buf, BUF_SIZE)) > 0) {
        write(1, buf, bytes);
    }

    close(s);
    return 0;
}
