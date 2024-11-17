#include "io_helper.h"
#include <pthread.h>

#define MAXBUF (8192)

// Estructura para pasar parámetros a los hilos
typedef struct {
    char *host;
    int port;
    char *filename;
} thread_data_t;

// Función para enviar la solicitud HTTP GET
void client_send(int fd, char *filename) {
    char buf[MAXBUF];
    char hostname[MAXBUF];
    
    gethostname_or_die(hostname, MAXBUF);
    
    // Formar y enviar la solicitud HTTP
    sprintf(buf, "GET %s HTTP/1.1\n", filename);
    sprintf(buf, "%shost: %s\n\r\n", buf, hostname);
    write_or_die(fd, buf, strlen(buf));
}

// Función para leer y mostrar la respuesta HTTP
void client_print(int fd) {
    char buf[MAXBUF];  
    int n;
    
    // Leer y mostrar los encabezados HTTP
    n = readline_or_die(fd, buf, MAXBUF);
    while (strcmp(buf, "\r\n") && (n > 0)) {
        printf("Header: %s", buf);
        n = readline_or_die(fd, buf, MAXBUF);
    }
    
    // Leer y mostrar el cuerpo de la respuesta HTTP
    n = readline_or_die(fd, buf, MAXBUF);
    while (n > 0) {
        printf("%s", buf);
        n = readline_or_die(fd, buf, MAXBUF);
    }
}

// Función que será ejecutada por cada hilo
void *client_thread(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    int clientfd;
    
    // Abrir la conexión con el servidor
    clientfd = open_client_fd_or_die(data->host, data->port);
    
    // Enviar la solicitud y mostrar la respuesta
    client_send(clientfd, data->filename);
    client_print(clientfd);
    
    // Cerrar la conexión
    close_or_die(clientfd);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    char *host, *filename;
    int port;
    int num_threads = 10;  // Número de hilos para la concurrencia
    pthread_t threads[num_threads];
    thread_data_t data[num_threads];
    
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <host> <port> <filename>\n", argv[0]);
        exit(1);
    }
    
    host = argv[1];
    port = atoi(argv[2]);
    filename = argv[3];
    
    // Crear varios hilos para simular múltiples conexiones
    for (int i = 0; i < num_threads; i++) {
        data[i].host = host;
        data[i].port = port;
        data[i].filename = filename;
        
        // Crear el hilo
        if (pthread_create(&threads[i], NULL, client_thread, (void *)&data[i]) != 0) {
            fprintf(stderr, "Error to created the thread %d\n", i);
            exit(1);
        }
    }
    
    // Esperar que todos los hilos terminen
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    exit(0);
}
