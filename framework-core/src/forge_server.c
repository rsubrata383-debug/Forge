#include "forge_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#else
    #include <unistd.h>
    #include <arpa/inet.h>
#endif


ForgeServer create_forge_server(int port, int backlog) {
    ForgeServer server;
    server.port = port;
    server.backlog = backlog;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        exit(EXIT_FAILURE);
    }
#endif

    server.socket_fd = socket(AF_INET, SOCK_STREAM, 0);

#ifdef _WIN32
    if (server.socket_fd == INVALID_SOCKET)
#else
    if (server.socket_fd < 0)
#endif
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(
        server.socket_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        (const char *)&opt,
        sizeof(opt)
    );

#ifdef _WIN32
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server.socket_fd,
             (struct sockaddr *)&addr,
             sizeof(addr)) == SOCKET_ERROR) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
#else
    server.address.sin_family = AF_INET;
    server.address.sin_addr.s_addr = INADDR_ANY;
    server.address.sin_port = htons(port);

    if (bind(server.socket_fd,
             (struct sockaddr *)&server.address,
             sizeof(server.address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
#endif

    if (listen(server.socket_fd, backlog) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("✅ Server running on port %d\n", port);
    return server;
}


void launch_server(ForgeServer *server) {
    while (1) {
        printf("নতুন কানেকশনের জন্য অপেক্ষা করছি...\n");

#ifdef _WIN32
        SOCKET client_socket = accept(server->socket_fd, NULL, NULL);
        if (client_socket == INVALID_SOCKET) {
            perror("Accept failed");
            continue;
        }
#else
        socklen_t addr_len = sizeof(server->address);
        int client_socket = accept(
            server->socket_fd,
            (struct sockaddr *)&server->address,
            &addr_len
        );
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }
#endif

        handle_client(client_socket);
    }
}


#ifdef _WIN32
void handle_client(SOCKET client_socket)
#else
void handle_client(int client_socket)
#endif
{
    char buffer[30000] = {0};

#ifdef _WIN32
    recv(client_socket, buffer, sizeof(buffer), 0);
#else
    read(client_socket, buffer, sizeof(buffer));
#endif

    printf("📩 Request:\n%s\n", buffer);

    const char *response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 12\r\n"
        "\r\n"
        "Hello World!";

#ifdef _WIN32
    send(client_socket, response, (int)strlen(response), 0);
    closesocket(client_socket);
#else
    write(client_socket, response, strlen(response));
    close(client_socket);
#endif
}
