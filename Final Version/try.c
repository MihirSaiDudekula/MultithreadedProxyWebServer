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

void parse_http_request(char* buffer, struct http_request* req) {
    memset(req, 0, sizeof(struct http_request));

    char* space = strchr(buffer, ' ');
    if (space) {
        strncpy(req->method, buffer, MIN(space - buffer, 15));
    }

    char* url_start = space + 1;
    char* url_end = strchr(url_start, ' ');
    if (url_end) {
        strncpy(req->url, url_start, MIN(url_end - url_start, 2047));
    }

    char* host_start = strstr(buffer, "Host: ");
    if (host_start) {
        host_start += 6;
        char* host_end = strstr(host_start, "\r\n");
        if (host_end) {
            strncpy(req->host, TARGET_HOST, sizeof(req->host) - 1);
        }
    } else {
        strncpy(req->host, TARGET_HOST, sizeof(req->host) - 1);
    }

    char* content_type = strstr(buffer, "Content-Type: ");
    if (content_type) {
        content_type += 14;
        char* content_type_end = strstr(content_type, "\r\n");
        if (content_type_end) {
            strncpy(req->content_type, content_type,
                   MIN(content_type_end - content_type, 127));
        }
    }

    char* content_length = strstr(buffer, "Content-Length: ");
    if (content_length) {
        req->content_length = atoi(content_length + 16);
    }

    char* body = strstr(buffer, "\r\n\r\n");
    if (body) {
        req->body = body + 4;
    }
}

int connect_to_server(char* host, int port) {
    struct hostent *server;
    struct sockaddr_in server_addr;

    // Always use TARGET_HOST and TARGET_PORT
    server = gethostbyname(TARGET_HOST);
    if (server == NULL) {
        fprintf(stderr, "Error: Could not resolve hostname %s\n", TARGET_HOST);
        return -1;
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating socket for server connection");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TARGET_PORT);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting to destination server");
        close(server_socket);
        return -1;
    }

    return server_socket;
}

void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    free(arg);

    sem_wait(&connection_semaphore);

    char buffer[MAX_BYTES];
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received <= 0) {
        close(client_socket);
        sem_post(&connection_semaphore);
        return NULL;
    }

    buffer[bytes_received] = '\0';

    struct http_request req;
    parse_http_request(buffer, &req);

    if (strcmp(req.method, "GET") != 0) {
        send_error_response(client_socket, 405);
        close(client_socket);
        sem_post(&connection_semaphore);
        return NULL;
    }

    int server_socket = connect_to_server(req.host, TARGET_PORT);
    if (server_socket < 0) {
        send_error_response(client_socket, 502);
        close(client_socket);
        sem_post(&connection_semaphore);
        return NULL;
    }

    if (send(server_socket, buffer, bytes_received, 0) < 0) {
        send_error_response(client_socket, 500);
        close(server_socket);
        close(client_socket);
        sem_post(&connection_semaphore);
        return NULL;
    }

    while ((bytes_received = recv(server_socket, buffer, sizeof(buffer), 0)) > 0) {
        if (send(client_socket, buffer, bytes_received, 0) < 0) {
            break;
        }
    }

    if (bytes_received < 0) {
        send_error_response(client_socket, 500);
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
