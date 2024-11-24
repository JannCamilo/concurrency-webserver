#include "io_helper.h"
#include <pthread.h>
#include <time.h>  // For measuring execution time

#define MAXBUF (8192)

// Structure to encapsulate thread parameters
typedef struct {
    char *host;     // Hostname or IP address of the server
    int port;       // Port number for the connection
    char *filename; // Name of the file to request
} thread_data_t;

// Function to send an HTTP GET request
void client_send(int fd, char *filename) {
    char buf[MAXBUF];
    char hostname[MAXBUF];
    
    // Get the hostname of the current machine
    gethostname_or_die(hostname, MAXBUF);
    
    // Construct and send the HTTP GET request
    sprintf(buf, "GET %s HTTP/1.1\n", filename); // Request line
    sprintf(buf, "%shost: %s\n\r\n", buf, hostname); // Host header
    write_or_die(fd, buf, strlen(buf)); // Send the request to the server
}

// Function to print the HTTP response
void client_print(int fd) {
    char buf[MAXBUF];  
    int n;
    
    // Read and print HTTP headers
    n = readline_or_die(fd, buf, MAXBUF); // Read the first line
    while (strcmp(buf, "\r\n") && (n > 0)) { // Stop at an empty line
        printf("Header: %s", buf); // Print the header
        n = readline_or_die(fd, buf, MAXBUF); // Read the next line
    }
    
    // Read and print the body of the HTTP response
    n = readline_or_die(fd, buf, MAXBUF);
    while (n > 0) {
        printf("%s", buf); // Print the response body
        n = readline_or_die(fd, buf, MAXBUF); // Read the next line
    }
}

// Function executed by each thread
void *client_thread(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    int clientfd;
    
    // Establish a connection to the server
    clientfd = open_client_fd_or_die(data->host, data->port);
    
    // Send the HTTP GET request and print the response
    client_send(clientfd, data->filename);
    client_print(clientfd);
    
    // Close the connection
    close_or_die(clientfd);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    char *host, *filename;
    int port;
    int num_threads = 10;  // Default number of threads for concurrency
        
    // Validate command-line arguments
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <host> <port> <filename> <threads>\n", argv[0]);
        exit(1);
    }
    
    // Parse command-line arguments
    host = argv[1];
    port = atoi(argv[2]);
    filename = argv[3];
    num_threads = atoi(argv[4]);

    pthread_t threads[num_threads];
    thread_data_t data[num_threads];
    struct timespec start, end;  // For measuring execution time
    
    // Record the start time
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Create multiple threads to simulate concurrent connections
    for (int i = 0; i < num_threads; i++) {
        data[i].host = host;
        data[i].port = port;
        data[i].filename = filename;
        
        // Create a thread to handle the connection
        if (pthread_create(&threads[i], NULL, client_thread, (void *)&data[i]) != 0) {
            fprintf(stderr, "Error creating thread %d\n", i);
            exit(1);
        }
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Record the end time
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // Calculate the total execution time in milliseconds
    double total_duration = (end.tv_sec - start.tv_sec) * 1000.0 + 
                            (end.tv_nsec - start.tv_nsec) / 1e6;
    
    // Display the total execution time and thread count
    printf("%d threads\n", num_threads);
    printf("Total time: %.2f ms\n", total_duration);

    exit(0);
}