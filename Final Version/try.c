/*
 * Multithreaded Proxy Server Implementation
 * 
 * This proxy server acts as an intermediary between clients and a backend server,
 * providing caching capabilities to improve performance and reduce load on the
 * backend server.
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
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

// Error logging macro - provides consistent error reporting
#define LOG_ERROR(msg) fprintf(stderr, "ERROR: %s: %s\n", msg, strerror(errno))

// Configuration structure for proxy server settings
/**
 * @brief Configuration structure for the proxy server
 * Contains all configurable parameters that control the server's behavior
 */
typedef struct {
    int port;                // Port to listen on
    char* target_host;       // Backend server hostname
    int target_port;         // Backend server port
    size_t max_cache_size;   // Maximum cache size in bytes
    size_t max_element_size; // Maximum size of a single cached item
    int max_clients;         // Maximum concurrent clients
    bool debug_mode;         // Enable debug logging
} ProxyConfig;

/**
 * @brief Global configuration for the proxy server
 * Default values can be overridden through configuration file or command line
 */
ProxyConfig config = {
    .port = 8080,           // Default proxy port
    .target_host = "localhost", // Default backend host
    .target_port = 3000,    // Default backend port
    .max_cache_size = 200 * (1<<20), // 200MB cache
    .max_element_size = 10 * (1<<10), // 10KB max element
    .max_clients = 10,      // Max concurrent clients
    .debug_mode = false     // Debug logging disabled by default
};

/**
 * @brief Statistics tracking for cache performance
 * Maintains counters for cache hits, misses, and current size
 */
struct CacheStats {
    size_t total_hits;       // Number of cache hits
    size_t total_misses;     // Number of cache misses
    size_t current_size;     // Current cache size in bytes
    size_t max_size;         // Maximum allowed cache size
} cache_stats = {0};

/**
 * @brief Thread-safe logging function
 * Ensures that log messages are not interleaved when multiple threads are logging
 */
void safe_log(const char* format, ...) {
    static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER; // Static mutex for logging
    pthread_mutex_lock(&log_mutex); // Lock mutex to prevent concurrent writes
    
    va_list args;
    va_start(args, format);
    vprintf(format, args); // Print formatted message
    printf("\n");
    va_end(args);
    
    pthread_mutex_unlock(&log_mutex); // Release mutex
}

#define MAX_CLIENTS 10
#define MAX_BYTES 4096
#define MAX_ELEMENT_SIZE 10 * (1<<10)
#define MAX_CACHE_SIZE 200 * (1<<20)
#define DEFAULT_PORT 8080
#define MIN(a,b) ((a) < (b) ? (a) : (b))

// Define target server details
#define TARGET_HOST "localhost"
#define TARGET_PORT 3000

struct http_request {
    char method[16];
    char url[2048];
    char host[256];
    char* body;
    int content_length;
    char content_type[128];
};

typedef struct cache_element {
    char* data;
    int len;
    char* url;
    time_t lru_time_track;
    struct cache_element* next;
} cache_element;

pthread_t tid[MAX_CLIENTS];
pthread_mutex_t cache_lock;
sem_t connection_semaphore;
cache_element* cache_head = NULL;
int total_cache_size = 0;

bool validate_url(const char* url) {
    // Basic URL validation
    if (!url || strlen(url) > 2048) return false;
    if (strchr(url, '\0') == NULL) return false;
    
    // Check for potential proxy chaining
    if (strchr(url, ':') != NULL) return false;
    if (strstr(url, "//") != NULL) return false;
    
    return true;
}

bool validate_headers(const char* headers) {
    // Check for malicious headers
    if (strstr(headers, "Proxy-Connection") != NULL) return false;
    if (strstr(headers, "X-Forwarded-For") != NULL) return false;
    if (strstr(headers, "X-Proxy") != NULL) return false;
    
    return true;
}

void parse_http_request(char* buffer, struct http_request* req) {
    memset(req, 0, sizeof(struct http_request));
    
    // Validate input length
    if (strlen(buffer) > MAX_BYTES - 1) {
        LOG_ERROR("Request too large");
        return;
    }

    // Parse method
    char* space = strchr(buffer, ' ');
    if (!space || space - buffer > sizeof(req->method)) {
        LOG_ERROR("Invalid request method");
        return;
    }
    strncpy(req->method, buffer, space - buffer);
    req->method[space - buffer] = '\0';

    // Parse URL
    char* url_start = space + 1;
    char* url_end = strchr(url_start, ' ');
    if (!url_end || url_end - url_start > sizeof(req->url)) {
        LOG_ERROR("Invalid URL");
        return;
    }
    strncpy(req->url, url_start, url_end - url_start);
    req->url[url_end - url_start] = '\0';
    
    if (!validate_url(req->url)) {
        LOG_ERROR("Invalid URL format");
        return;
    }

    // Parse headers
    char* header_start = strstr(buffer, "\r\n");
    if (!header_start) {
        LOG_ERROR("Invalid request format");
        return;
    }
    
    if (!validate_headers(header_start)) {
        LOG_ERROR("Invalid headers");
        return;
    }

    // Parse content type
    char* content_type = strstr(buffer, "Content-Type: ");
    if (content_type) {
        content_type += 14;
        char* content_type_end = strstr(content_type, "\r\n");
        if (content_type_end) {
            size_t len = MIN(content_type_end - content_type, sizeof(req->content_type) - 1);
            strncpy(req->content_type, content_type, len);
            req->content_type[len] = '\0';
        }
    }

    // Parse content length
    char* content_length = strstr(buffer, "Content-Length: ");
    if (content_length) {
        content_length += 16;
        char* endptr;
        req->content_length = strtoul(content_length, &endptr, 10);
        if (endptr == content_length || req->content_length > MAX_ELEMENT_SIZE) {
            LOG_ERROR("Invalid content length");
            return;
        }
    }

    // Parse body
    char* body = strstr(buffer, "\r\n\r\n");
    if (body) {
        req->body = body + 4;
    }
}

int connect_to_server(const char* host, int port, int timeout_ms) {
    struct hostent *server;
    struct sockaddr_in server_addr;
    struct timeval timeout;
    int server_socket;
    
    // Set socket timeout
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    // Resolve hostname
    server = gethostbyname(host);
    if (!server) {
        LOG_ERROR("Could not resolve hostname");
        return -1;
    }

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        LOG_ERROR("Failed to create socket");
        return -1;
    }

    // Set socket options
    if (setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, 
                  (const char*)&timeout, sizeof(timeout)) < 0) {
        LOG_ERROR("Failed to set receive timeout");
        close(server_socket);
        return -1;
    }

    if (setsockopt(server_socket, SOL_SOCKET, SO_SNDTIMEO, 
                  (const char*)&timeout, sizeof(timeout)) < 0) {
        LOG_ERROR("Failed to set send timeout");
        close(server_socket);
        return -1;
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    // Connect with timeout
    if (connect(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("Failed to connect to server");
        close(server_socket);
        return -1;
    }

    return server_socket;
}

// Cache management functions
bool cache_lookup(const char* url, cache_element** element) {
    pthread_mutex_lock(&cache_lock);
    
    cache_element* current = cache_head;
    while (current) {
        if (strcmp(current->url, url) == 0) {
            *element = current;
            pthread_mutex_unlock(&cache_lock);
            cache_stats.total_hits++;
            return true;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&cache_lock);
    cache_stats.total_misses++;
    return false;
}

void cache_add(const char* url, const char* data, size_t len) {
    pthread_mutex_lock(&cache_lock);
    
    // Check if URL already exists
    cache_element* element;
    if (cache_lookup(url, &element)) {
        // Update existing element
        free(element->data);
        element->data = malloc(len);
        if (!element->data) {
            pthread_mutex_unlock(&cache_lock);
            return;
        }
        memcpy(element->data, data, len);
        element->len = len;
        element->lru_time_track = time(NULL);
        pthread_mutex_unlock(&cache_lock);
        return;
    }

    // Create new element
    cache_element* new_element = malloc(sizeof(cache_element));
    if (!new_element) {
        pthread_mutex_unlock(&cache_lock);
        return;
    }

    new_element->url = strdup(url);
    new_element->data = malloc(len);
    if (!new_element->data) {
        free(new_element);
        pthread_mutex_unlock(&cache_lock);
        return;
    }

    memcpy(new_element->data, data, len);
    new_element->len = len;
    new_element->lru_time_track = time(NULL);
    new_element->next = cache_head;
    cache_head = new_element;

    // Update cache size
    cache_stats.current_size += len;
    
    // Evict if cache is full
    while (cache_stats.current_size > config.max_cache_size) {
        cache_element* to_evict = NULL;
        time_t oldest_time = time(NULL);
        
        cache_element* current = cache_head;
        while (current) {
            if (current->lru_time_track < oldest_time) {
                oldest_time = current->lru_time_track;
                to_evict = current;
            }
            current = current->next;
        }

        if (to_evict) {
            cache_stats.current_size -= to_evict->len;
            free(to_evict->url);
            free(to_evict->data);
            free(to_evict);
        }
    }

    pthread_mutex_unlock(&cache_lock);
}

void cache_cleanup() {
    pthread_mutex_lock(&cache_lock);
    
    cache_element* current = cache_head;
    while (current) {
        cache_element* next = current->next;
        free(current->url);
        free(current->data);
        free(current);
        current = next;
    }
    cache_head = NULL;
    cache_stats.current_size = 0;

    pthread_mutex_unlock(&cache_lock);
}
}

void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    free(arg);

    sem_wait(&connection_semaphore);

    // Set socket timeout
    struct timeval timeout = {30, 0}; // 30 seconds timeout
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, 
                  (const char*)&timeout, sizeof(timeout)) < 0) {
        LOG_ERROR("Failed to set client socket timeout");
        close(client_socket);
        sem_post(&connection_semaphore);
        return NULL;
    }

    char buffer[MAX_BYTES];
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received <= 0) {
        LOG_ERROR("Failed to receive data from client");
        close(client_socket);
        sem_post(&connection_semaphore);
        return NULL;
    }

    buffer[bytes_received] = '\0';

    struct http_request req;
    parse_http_request(buffer, &req);

    if (req.content_length > config.max_element_size) {
        LOG_ERROR("Request too large");
        send_error_response(client_socket, 413);
        close(client_socket);
        sem_post(&connection_semaphore);
        return NULL;
    }

    // Check cache first
    cache_element* cache_element;
    if (cache_lookup(req.url, &cache_element)) {
        // Cache hit - send cached response
        safe_log("Cache hit for URL: %s", req.url);
        send(client_socket, cache_element->data, cache_element->len, 0);
        close(client_socket);
        sem_post(&connection_semaphore);
        return NULL;
    }

    // Cache miss - forward to target server
    int server_socket = connect_to_server(config.target_host, config.target_port, 5000);
    if (server_socket < 0) {
        LOG_ERROR("Failed to connect to target server");
        send_error_response(client_socket, 502);
        close(client_socket);
        sem_post(&connection_semaphore);
        return NULL;
    }

    // Forward request
    if (send(server_socket, buffer, bytes_received, 0) < 0) {
        LOG_ERROR("Failed to send request to target server");
        send_error_response(client_socket, 500);
        close(server_socket);
        close(client_socket);
        sem_post(&connection_semaphore);
        return NULL;
    }

    // Buffer for response
    char response_buffer[MAX_BYTES];
    size_t total_received = 0;
    
    // Receive response in chunks
    while (true) {
        ssize_t bytes = recv(server_socket, response_buffer, sizeof(response_buffer), 0);
        if (bytes <= 0) break;
        
        // Send to client
        if (send(client_socket, response_buffer, bytes, 0) < 0) {
            LOG_ERROR("Failed to send to client");
            break;
        }
        
        total_received += bytes;
        
        // Cache response if it's not too large
        if (total_received <= config.max_element_size) {
            static char* full_response = NULL;
            static size_t allocated_size = 0;
            
            // Resize buffer if needed
            if (total_received + bytes > allocated_size) {
                allocated_size = total_received + bytes + MAX_BYTES;
                full_response = realloc(full_response, allocated_size);
                if (!full_response) {
                    LOG_ERROR("Memory allocation failed");
                    break;
                }
            }
            
            // Copy received data
            memcpy(full_response + total_received - bytes, response_buffer, bytes);
        }
    }

    // Cache the complete response if we have it
    if (total_received > 0 && total_received <= config.max_element_size && full_response) {
        cache_add(req.url, full_response, total_received);
        free(full_response);
    }

    close(server_socket);
    close(client_socket);
    sem_post(&connection_semaphore);
    return NULL;
}

int send_error_response(int socket, int status_code) {
    char response[512];
    char *status_text;

    switch(status_code) {
        case 405:
            status_text = "Method Not Allowed";
            break;
        case 502:
            status_text = "Bad Gateway";
            break;
        case 500:
            status_text = "Internal Server Error";
            break;
        default:
            status_text = "Internal Server Error";
            status_code = 500;
    }

    snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"error\": \"%s\"}\r\n",
        status_code, status_text, status_text);

    return send(socket, response, strlen(response), 0);
}

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

    sem_init(&connection_semaphore, 0, MAX_CLIENTS);
    pthread_mutex_init(&cache_lock, NULL);

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Proxy server listening on port %d...\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int* client_socket = malloc(sizeof(int));
        *client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);

        if (*client_socket < 0) {
            perror("Accept failed");
            free(client_socket);
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("New connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void*)client_socket) != 0) {
            perror("Thread creation failed");
            close(*client_socket);
            free(client_socket);
            continue;
        }
        pthread_detach(thread_id);
    }

    close(server_socket);
    sem_destroy(&connection_semaphore);
    pthread_mutex_destroy(&cache_lock);
    return 0;
}
