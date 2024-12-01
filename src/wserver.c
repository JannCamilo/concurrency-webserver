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

#define DEFAULT_PORT 10000
#define DEFAULT_THREADS 1
#define DEFAULT_BUFFERS 1
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
        printf("Buffer full, waiting space...\n");
        pthread_cond_wait(&b->not_full, &b->lock);
    }
    b->buffer[b->end] = conn_fd;
    b->end = (b->end + 1) % b->size;
    printf("Connection %d added to the buffer (start: %d, end: %d)\n", conn_fd, b->start, b->end);
    pthread_cond_signal(&b->not_empty);
    pthread_mutex_unlock(&b->lock);
}

int buffer_pop(Buffer *b) {
    pthread_mutex_lock(&b->lock);
    while (b->start == b->end) {
        printf("Buffer empty, waiting for a new connection...\n");
        pthread_cond_wait(&b->not_empty, &b->lock);
    }
    int conn_fd = b->buffer[b->start];
    b->start = (b->start + 1) % b->size;
    printf("Connection %d removed from the buffer (start: %d, end: %d)\n", conn_fd, b->start, b->end);
    pthread_cond_signal(&b->not_full);
    pthread_mutex_unlock(&b->lock);
    return conn_fd;
}

void *worker(void *arg) {
    Buffer *b = (Buffer *) arg;
    printf("Worker thread %ld started.\n", pthread_self());

    while (1) {
        int conn_fd = buffer_pop(b);
        printf("Thread %ld processing connection %d.\n", pthread_self(), conn_fd);
        request_handle(conn_fd);
        printf("Thread %ld finished processing connection %d.\n", pthread_self(), conn_fd);
        close_or_die(conn_fd);
    }
}

int main(int argc, char *argv[]) {
    int c;
    char *root_dir = default_root;
    int port = DEFAULT_PORT;
    int thread_count = DEFAULT_THREADS;
    int buffer_size = DEFAULT_BUFFERS;

    while ((c = getopt(argc, argv, "d:p:t:b:")) != -1) {
        switch (c) {
        case 'd':
            root_dir = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            if (port <= 0) {
                fprintf(stderr, "Invalid port.\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 't':
            thread_count = atoi(optarg);
            if (thread_count <= 0) {
                fprintf(stderr, "Number of threads must be a positive integer.\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'b':
            buffer_size = atoi(optarg);
            if (buffer_size <= 0) {
                fprintf(stderr, "Buffer size must be a positive integer.\n");
                exit(EXIT_FAILURE);
            }
            break;
        default:
            fprintf(stderr, "Usage: wserver [-d basedir] [-p port] [-t threads] [-b buffer_size]\n");
            exit(EXIT_FAILURE);
        }
    }

    // Change working directory
    chdir_or_die(root_dir);

    // Create the buffer
    Buffer *buffer = buffer_init(buffer_size);

    // Create worker threads
    pthread_t threads[thread_count];
    for (int i = 0; i < thread_count; ++i) {
        if (pthread_create(&threads[i], NULL, worker, (void *) buffer) != 0) {
            fprintf(stderr, "Error creating thread %d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    // Open the socket to listen for connections
    int listen_fd = open_listen_fd_or_die(port);
    while (1) {
        printf("Server started on port %d with %d threads and buffer size %d\n", port, thread_count, buffer_size);
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        int conn_fd = accept_or_die(listen_fd, (sockaddr_t *) &client_addr, (socklen_t *) &client_len);

        buffer_push(buffer, conn_fd);
    }
    return 0;
}