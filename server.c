#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

/**
 * Project 1 starter code
 * All parts needed to be changed/added are marked with TODO
 */

#define BUFFER_SIZE 1024
#define DEFAULT_SERVER_PORT 8081
#define DEFAULT_REMOTE_HOST "131.179.176.34"
#define DEFAULT_REMOTE_PORT 5001

struct server_app {
    // Parameters of the server
    // Local port of HTTP server
    uint16_t server_port;

    // Remote host and port of remote proxy
    char *remote_host;
    uint16_t remote_port;
};

// The following function is implemented for you and doesn't need
// to be change
void parse_args(int argc, char *argv[], struct server_app *app);

// The following functions need to be updated
void handle_request(struct server_app *app, int client_socket);
void serve_local_file(int client_socket, const char *path);
void proxy_remote_file(struct server_app *app, int client_socket, const char *path);

// The main function is provided and no change is needed
int main(int argc, char *argv[])
{
    struct server_app app;
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int ret;

    parse_args(argc, argv, &app);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(app.server_port);

    // The following allows the program to immediately bind to the port in case
    // previous run exits recently
    int optval = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) == -1) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", app.server_port);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("accept failed");
            continue;
        }
        
        printf("Accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        handle_request(&app, client_socket);
        close(client_socket);
    }

    close(server_socket);
    return 0;
}

void parse_args(int argc, char *argv[], struct server_app *app)
{
    int opt;

    app->server_port = DEFAULT_SERVER_PORT;
    app->remote_host = NULL;
    app->remote_port = DEFAULT_REMOTE_PORT;

    while ((opt = getopt(argc, argv, "b:r:p:")) != -1) {
        switch (opt) {
        case 'b':
            app->server_port = atoi(optarg);
            break;
        case 'r':
            app->remote_host = strdup(optarg);
            break;
        case 'p':
            app->remote_port = atoi(optarg);
            break;
        default: /* Unrecognized parameter or "-?" */
            fprintf(stderr, "Usage: server [-b local_port] [-r remote_host] [-p remote_port]\n");
            exit(-1);
            break;
        }
    }

    if (app->remote_host == NULL) {
        app->remote_host = strdup(DEFAULT_REMOTE_HOST);
    }
}

void handle_request(struct server_app *app, int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Read the request from HTTP client
    // Note: This code is not ideal in the real world because it
    // assumes that the request header is small enough and can be read
    // once as a whole.
    // However, the current version suffices for our testing.
    bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        return;  // Connection closed or error
    }

    buffer[bytes_read] = '\0';
    // copy buffer to a new string
    char *request = malloc(strlen(buffer) + 1);
    strcpy(request, buffer);

    // TODO: Parse the header and extract essential fields, e.g. file name
    char method[8], path[256], protocol[16];
    char *saveptr;
    char *line = strtok_r(request, "\r\n", &saveptr);

    sscanf(line, "%s %s %s", method, path, protocol);

    while ((line = strtok_r(NULL, "\r\n", &saveptr)) != NULL) {
        
    }

    if (strcasecmp(protocol, "HTTP/1.1") == 0) {
    if (strlen(path) > 3 && strcmp(path + strlen(path) - 3, ".ts") == 0) {
        proxy_remote_file(app, client_socket, request);
    } else {
        if (strcmp(path, "/") == 0) {
            // Redirect requests for "/" to "index.html"
            serve_local_file(client_socket, "/index.html");
            free(request);
            return;
        }
        serve_local_file(client_socket, path);
        }
    }

    free(request);
}

void serve_local_file(int client_socket, const char *path) {
    // Open the requested file
    printf("path: %s\n", path);
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    char relative_path[1024];
    snprintf(relative_path, sizeof(relative_path), "%s/%s", cwd, path);

    FILE *file_fd = fopen(relative_path, "r");
    if (file_fd == NULL) {
        char buff[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, buff, strlen(buff), 0);
        return;
    }

    // Get file size
    fseek(file_fd, 0, SEEK_END);
    long file_size = ftell(file_fd);
    fseek(file_fd, 0, SEEK_SET);

    char *content_type = "application/octet-stream";
    char *ext = strrchr(path, '.');
    if (ext != NULL) {
        if (strcmp(ext, ".html") == 0 || strcmp(ext, ".txt") == 0) {
            content_type = "text/html; charset=utf-8";
        } else if (strcmp(ext, ".jpg") == 0) {
            content_type = "image/jpeg";
        }
    }

    // Send HTTP headers with the correct content type and file size
    char buff[BUFFER_SIZE];
    snprintf(buff, sizeof(buff), "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", content_type, file_size);
    send(client_socket, buff, strlen(buff), 0);

    // Send file content
    ssize_t bytes_read;
    while ((bytes_read = fread(buff, 1, sizeof(buff), file_fd)) > 0) {
        send(client_socket, buff, bytes_read, 0);
    }

    fclose(file_fd);
}

void proxy_remote_file(struct server_app *app, int client_socket, const char *request) {
    // TODO: Implement proxy request and replace the following code
    // What's needed:
    // * Connect to remote server (app->remote_server/app->remote_port)
    // * Forward the original request to the remote server
    // * Pass the response from remote server back
    // Bonus:
    // * When connection to the remote server fail, properly generate
    // HTTP 502 "Bad Gateway" response
    char method[8], path[256], protocol[16];
    sscanf(request, "%s %s %s", method, path, protocol);

    int backend_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (backend_socket == -1) {
        perror("backend socket failed");
        char response[] = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
        return;
    }

    struct hostent *backend_host = gethostbyname(app->remote_host);
    if (backend_host == NULL) {
        perror("gethostbyname failed");
        char response[] = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
        close(backend_socket);
        return;
    }

    struct sockaddr_in backend_addr;
    backend_addr.sin_family = AF_INET;
    backend_addr.sin_port = htons(app->remote_port);
    memcpy(&backend_addr.sin_addr.s_addr, backend_host->h_addr, backend_host->h_length);
    //fix code HERE 
    if (connect(backend_socket, (struct sockaddr *)&backend_addr, sizeof(backend_addr)) == -1) {
        perror("backend connect failed");
        char response[] = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
        close(backend_socket);
        return;
    }

    send(backend_socket, request, strlen(request), 0);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = recv(backend_socket, buffer, sizeof(buffer), 0)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    close(backend_socket);
}
