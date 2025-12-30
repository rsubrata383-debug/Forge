#ifndef FORGE_SERVER_H
#define FORGE_SERVER_H

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <netinet/in.h>
#endif

typedef struct {
#ifdef _WIN32
    SOCKET socket_fd;
#else
    int socket_fd;
    struct sockaddr_in address;
#endif
    int port;
    int backlog;
} ForgeServer;

ForgeServer create_forge_server(int port, int backlog);
void launch_server(ForgeServer *server);

#ifdef _WIN32
void handle_client(SOCKET client_socket);
#else
void handle_client(int client_socket);
#endif

#endif
