#include <sys/types.h> /* Éste es el código del servidor */
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#define SERVER_PORT 12345 /* arbitrario, pero el cliente y el servidor deben coincidir */
#define BUF_SIZE 4096     /* tamaño de bloque para la transferencia */
#define QUEUE_SIZE 10

void fatal(const char *message) {
    herror(message);
    exit(1);
}

int main(int argc, char *argv[])
{
    int s, b, l, fd, sa, bytes, on = 1;
    char buf[BUF_SIZE];         /* búfer para el archivo saliente */
    struct sockaddr_in channel; /* contiene la dirección IP */
    /* Construye la estructura de la dirección para enlazar el socket. */
    memset(&channel, 0, sizeof(channel)); /* canal cero */
    channel.sin_family = AF_INET;
    channel.sin_addr.s_addr = htonl(INADDR_ANY);
    channel.sin_port = htons(SERVER_PORT);
    /* Apertura pasiva. Espera una conexión. */
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); /* crea el socket */
    if (s < 0)
        fatal("falla en socket ");
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
    b = bind(s, (struct sockaddr *)&channel, sizeof(channel));
    if (b < 0)
        fatal("falla en bind ");
    l = listen(s, QUEUE_SIZE); /* especifica el tamaño de la cola */
    if (l < 0)
        fatal("falla en listen");
    /* El socket ahora está configurado y enlazado. Espera una conexión y la procesa. */
    while (1)
    {
        sa = accept(s, 0, 0); /* se bloquea para la solicitud de conexión */
        if (sa < 0)
            fatal("falla en accept");
        read(sa, buf, BUF_SIZE); /* lee el nombre del archivo desde el socket */
        /* Obtiene y regresa el archivo.*/
        fd = open(buf, O_RDONLY); /* abre el archivo para regresarlo */
        if (fd < 0)
            fatal("falla en open");
        while (1)
        {
            bytes = read(fd, buf, BUF_SIZE); /* lee del archivo */
            if (bytes <= 0)
                break;             /* verifica el final del archivo */
            write(sa, buf, bytes); /* escribe bytes en el socket */
        }
        close(fd); /* cierra el archivo */
        close(sa); /* cierra la conexión */
    }
}