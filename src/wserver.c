#include <stdio.h>
#include "request.h"
#include "io_helper.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>

#define DEFAULT_PORT 8111
#define DEFAULT_THREADS 1
#define DEFAULT_BUFFERS 2
#define BUFFER_SIZE 1024
char default_root[] = ".";

typedef struct {
    int *buffer;
    int size;
    int start;
    int end;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} Buffer;

Buffer *buffer_init(int size) {
    Buffer *b = malloc(sizeof(Buffer));
    b->buffer = malloc(sizeof(int) * size);
    b->size = size;
    b->start = 0;
    b->end = 0;
    pthread_mutex_init(&b->lock, NULL);
    pthread_cond_init(&b->not_empty, NULL);
    pthread_cond_init(&b->not_full, NULL);
    return b;
}

void buffer_push(Buffer *b, int conn_fd) {
    pthread_mutex_lock(&b->lock);
    while ((b->end + 1) % b->size == b->start) {
        printf("Buffer full, waiting space...\n");  // Mensaje cuando el buffer está lleno
        pthread_cond_wait(&b->not_full, &b->lock); // Esperar si el buffer está lleno
    }
    b->buffer[b->end] = conn_fd;
    b->end = (b->end + 1) % b->size;
    printf("Conection %d added to the buffer (start: %d, end: %d)\n", conn_fd, b->start, b->end);  // Mensaje cuando se agrega una conexión
    pthread_cond_signal(&b->not_empty); // Notificar que hay algo para procesar
    pthread_mutex_unlock(&b->lock);
}

int buffer_pop(Buffer *b) {
    pthread_mutex_lock(&b->lock);
    while (b->start == b->end) {
        printf("Buffer empty, waiting a new conecion...\n");  // Mensaje cuando el buffer está vacío
        pthread_cond_wait(&b->not_empty, &b->lock); // Esperar si el buffer está vacío
    }
    int conn_fd = b->buffer[b->start];
    b->start = (b->start + 1) % b->size;
    printf("Conection %d sorted of the buffer (start: %d, end: %d)\n", conn_fd, b->start, b->end);  // Mensaje cuando se retira una conexión
    pthread_cond_signal(&b->not_full); // Notificar que el buffer no está lleno
    pthread_mutex_unlock(&b->lock);
    return conn_fd;
}

void *worker(void *arg) {
    Buffer *b = (Buffer *) arg;
    printf("Thread of work %ld start.\n", pthread_self());  // Mensaje cuando un hilo de trabajo inicia

    while (1) {
        int conn_fd = buffer_pop(b); // Obtener una conexión del buffer
        printf("Thread %ld proceing the conection %d.\n", pthread_self(), conn_fd);  // Mensaje cuando un hilo empieza a procesar una conexión
        request_handle(conn_fd); // Procesar la solicitud del cliente
        printf("Thread %ld finish of procesing the conection %d.\n", pthread_self(), conn_fd);  // Mensaje cuando un hilo termina de procesar
        close_or_die(conn_fd); // Cerrar la conexión cuando termine
    }
}

//
// ./wserver [-d basedir] [-p port] [-t threads] [-b buffers]
// 
int main(int argc, char *argv[]) {
    int c;
    char *root_dir = default_root;
    int port = DEFAULT_PORT;
    int thread_count = DEFAULT_THREADS;
    int buffer_size = DEFAULT_BUFFERS;
    
    while ((c = getopt(argc, argv, "d:p:t:b")) != -1)
        switch (c) {
        case 'd':
            root_dir = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            if (port <= 0) {
                fprintf(stderr, "Port invalid.\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 't':
            thread_count = atoi(optarg);
            if (thread_count <= 0) {
                fprintf(stderr, "The number of threads should be a positive integer.\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'b':
            buffer_size = atoi(optarg);
            if (buffer_size <= 0) {
                fprintf(stderr, "The size of buffer shloud be a positive integer.\n");
                exit(EXIT_FAILURE);
            }
            break;
        default:
            fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t threads] [-b buffers]\n");
            exit(1);
        }
    
    // run out of this directory
    chdir_or_die(root_dir);

    // Crear el buffer
    Buffer *buffer = buffer_init(buffer_size);

    // Crear hilos de trabajo
    pthread_t threads[thread_count];
    for (int i = 0; i < thread_count; ++i) {
        if (pthread_create(&threads[i], NULL, worker, (void *)buffer) != 0) {
            fprintf(stderr, "Error al crear hilo %d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    // Abrir el socket de escucha
    int listen_fd = open_listen_fd_or_die(port);
    while (1) {
        printf("Started serverin the port %d whit %d threads and buffer of size %d\n", port, thread_count, buffer_size);
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        int conn_fd = accept_or_die(listen_fd, (sockaddr_t *) &client_addr, (socklen_t *) &client_len);
        
        buffer_push(buffer, conn_fd); // Enviar la conexión al buffer para que los hilos la manejen
    }

    // Unir los hilos (aunque este punto nunca se alcanzará en este diseño)
    for (int i = 0; i < thread_count; ++i) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
