/*
 * Improved Multithreaded HTTP Proxy Server
 * 
 * This proxy server handles HTTP GET requests, implementing:
 * - Multithreading for concurrent client connections
 * - LRU caching for improved performance
 * - Thread synchronization using mutexes and semaphores
 * - Error handling and HTTP status responses
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <errno.h>

#define MAX_CLIENTS 10
#define MAX_BYTES 4096        // 4KB buffer size
#define MAX_ELEMENT_SIZE 10 * (1<<10)  // Max size of single cache element
#define MAX_CACHE_SIZE 200 * (1<<20)   // Max total cache size (200MB)
#define DEFAULT_PORT 8080

// Cache element structure for LRU implementation
typedef struct cache_element {
    char* data;              // Cached response data
    int len;                 // Length of cached data
    char* url;               // URL as cache key
    time_t lru_time_track;   // Timestamp for LRU tracking
    struct cache_element* next;  // Next element in linked list
} cache_element;

// Global variables
pthread_t tid[MAX_CLIENTS];         // Thread IDs
pthread_mutex_t cache_lock;         // Cache access lock
sem_t connection_semaphore;         // Connection limit semaphore
cache_element* cache_head = NULL;   // Cache linked list head
int total_cache_size = 0;          // Current cache size

// Function declarations
cache_element* find_in_cache(char* url);
int add_to_cache(char* data, int size, char* url);
void remove_from_cache(void);
int send_error_response(int socket, int status_code);
int connect_to_server(char* host, int port);
void* handle_client(void* client_socket);


// Add these function implementations before the main() function

// Function to establish connection with destination server
int connect_to_server(char* host, int port) {
    struct hostent *server;
    struct sockaddr_in server_addr;
    
    // Get host information
    server = gethostbyname(host);
    if (server == NULL) {
        fprintf(stderr, "Error: Could not resolve hostname %s\n", host);
        return -1;
    }
    
    // Create socket for server connection
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating socket for server connection");
        return -1;
    }
    
    // Configure server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    // Establish connection to server
    if (connect(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting to destination server");
        close(server_socket);
        return -1;
    }
    
    return server_socket;
}

// Function to send HTTP error responses back to client
int send_error_response(int socket, int status_code) {
    char *response;
    switch(status_code) {
        case 400:
            response = "HTTP/1.1 400 Bad Request\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: 35\r\n\r\n"
                      "<html>400 - Bad Request</html>\r\n";
            break;
        case 403:
            response = "HTTP/1.1 403 Forbidden\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: 32\r\n\r\n"
                      "<html>403 - Forbidden</html>\r\n";
            break;
        case 404:
            response = "HTTP/1.1 404 Not Found\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: 32\r\n\r\n"
                      "<html>404 - Not Found</html>\r\n";
            break;
        case 500:
            response = "HTTP/1.1 500 Internal Server Error\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: 45\r\n\r\n"
                      "<html>500 - Internal Server Error</html>\r\n";
            break;
        case 502:
            response = "HTTP/1.1 502 Bad Gateway\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: 35\r\n\r\n"
                      "<html>502 - Bad Gateway</html>\r\n";
            break;
        default:
            response = "HTTP/1.1 500 Internal Server Error\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: 45\r\n\r\n"
                      "<html>500 - Internal Server Error</html>\r\n";
    }
    
    return send(socket, response, strlen(response), 0);
}

// Main server implementation
int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port_number>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        printf("Invalid port number. Use 1-65535\n");
        exit(EXIT_FAILURE);
    }

    // Initialize synchronization primitives
    sem_init(&connection_semaphore, 0, MAX_CLIENTS);
    pthread_mutex_init(&cache_lock, NULL);

    // Create server socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Enable socket address reuse
    int reuse = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // Configure server address
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket to address
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Proxy server listening on port %d...\n", port);

    // Main server loop
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Accept new connection
        int* client_socket = malloc(sizeof(int));
        *client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        
        if (*client_socket < 0) {
            perror("Accept failed");
            free(client_socket);
            continue;
        }

        // Log connection
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("New connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        // Create thread to handle client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void*)client_socket) != 0) {
            perror("Thread creation failed");
            close(*client_socket);
            free(client_socket);
            continue;
        }
        pthread_detach(thread_id);
    }

    // Cleanup (though we never reach here in this implementation)
    close(server_socket);
    sem_destroy(&connection_semaphore);
    pthread_mutex_destroy(&cache_lock);
    return 0;
}

// Thread function to handle client requests
void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    free(arg);

    // Wait for available connection slot
    sem_wait(&connection_semaphore);

    char buffer[MAX_BYTES];
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) {
        close(client_socket);
        sem_post(&connection_semaphore);
        return NULL;
    }

    buffer[bytes_received] = '\0';

    // Check cache first
    pthread_mutex_lock(&cache_lock);
    cache_element* cached = find_in_cache(buffer);
    if (cached != NULL) {
        send(client_socket, cached->data, cached->len, 0);
        pthread_mutex_unlock(&cache_lock);
        printf("Cache hit - serving from cache\n");
    } else {
        pthread_mutex_unlock(&cache_lock);
        
        // Parse request and forward to destination server
        // Note: Simplified for example - you should properly parse HTTP headers
        char* host_start = strstr(buffer, "Host: ") + 6;
        char* host_end = strstr(host_start, "\r\n");
        char host[256] = {0};
        strncpy(host, host_start, host_end - host_start);

        int server_socket = connect_to_server(host, 80);
        if (server_socket < 0) {
            send_error_response(client_socket, 502);
            close(client_socket);
            sem_post(&connection_semaphore);
            return NULL;
        }

        // Forward request and get response
        send(server_socket, buffer, bytes_received, 0);
        
        char response[MAX_BYTES];
        bytes_received = recv(server_socket, response, sizeof(response), 0);
        
        if (bytes_received > 0) {
            send(client_socket, response, bytes_received, 0);
            
            // Cache the response
            pthread_mutex_lock(&cache_lock);
            add_to_cache(response, bytes_received, buffer);
            pthread_mutex_unlock(&cache_lock);
        }

        close(server_socket);
    }

    close(client_socket);
    sem_post(&connection_semaphore);
    return NULL;
}

// Cache management functions
cache_element* find_in_cache(char* url) {
    cache_element* current = cache_head;
    while (current != NULL) {
        if (strcmp(current->url, url) == 0) {
            current->lru_time_track = time(NULL);
            return current;
        }
        current = current->next;
    }
    return NULL;
}

int add_to_cache(char* data, int size, char* url) {
    if (size > MAX_ELEMENT_SIZE) {
        return 0;
    }

    while (total_cache_size + size > MAX_CACHE_SIZE) {
        remove_from_cache();
    }

    cache_element* element = malloc(sizeof(cache_element));
    element->data = malloc(size);
    memcpy(element->data, data, size);
    element->url = strdup(url);
    element->len = size;
    element->lru_time_track = time(NULL);
    element->next = cache_head;
    cache_head = element;

    total_cache_size += size + sizeof(cache_element) + strlen(url) + 1;
    return 1;
}

void remove_from_cache(void) {
    if (cache_head == NULL) {
        return;
    }

    cache_element *oldest = cache_head;
    cache_element *prev = NULL;
    cache_element *current = cache_head;

    // Find oldest element
    while (current->next != NULL) {
        if (current->next->lru_time_track < oldest->lru_time_track) {
            oldest = current->next;
            prev = current;
        }
        current = current->next;
    }

    // Remove oldest element
    if (prev == NULL) {
        cache_head = oldest->next;
    } else {
        prev->next = oldest->next;
    }

    total_cache_size -= oldest->len + sizeof(cache_element) + strlen(oldest->url) + 1;
    free(oldest->data);
    free(oldest->url);
    free(oldest);
}
