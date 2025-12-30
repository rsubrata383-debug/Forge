#include "forge_server.h"
#include "forge_abi.h"
#include "forge_router.h"
#include "forge_http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

/* =========================================================
   ABI
   ========================================================= */

/* Embed ABI version into the binary */
const int forge_abi_version = FORGE_ABI_VERSION;

/* =========================================================
   Route Handlers (forward declarations)
   ========================================================= */

static void handle_root(const ForgeHttpRequest *req, int client_socket);
static void handle_health(const ForgeHttpRequest *req, int client_socket);
static void handle_version(const ForgeHttpRequest *req, int client_socket);
static void send_404(int client_socket);

/* =========================================================
   Route Table (FILE SCOPE)
   ========================================================= */

static const ForgeRoute routes[] = {
    {"GET", "/", handle_root},
    {"GET", "/health", handle_health},
    {"GET", "/api/version", handle_version},
};

/* =========================================================
   Route Handlers
   ========================================================= */

static void handle_root(const ForgeHttpRequest *req, int client_socket)
{
    (void)req;
    forge_send_text(client_socket, "200 OK", "Hello from Forge!\n");
}

static void handle_health(const ForgeHttpRequest *req, int client_socket)
{
    (void)req;
    forge_send_text(client_socket, "200 OK", "OK\n");
}

static void handle_version(const ForgeHttpRequest *req, int client_socket)
{
    (void)req;
    forge_send_json(client_socket, "200 OK",
                    "{ \"name\": \"forge\", \"version\": \"1.0\" }\n");
}

/* =========================================================
   Server Creation
   ========================================================= */

ForgeServer create_forge_server(int port, int backlog)
{
    ForgeServer server;
    memset(&server, 0, sizeof(server));

    server.port = port;
    server.backlog = backlog;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
#endif

    server.socket_fd = socket(AF_INET, SOCK_STREAM, 0);

#ifdef _WIN32
    if (server.socket_fd == INVALID_SOCKET)
    {
        printf("socket failed: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
#else
    if (server.socket_fd < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
#endif

#ifdef _WIN32
    BOOL opt = TRUE;
    if (setsockopt(server.socket_fd, SOL_SOCKET, SO_REUSEADDR,
                   (const char *)&opt, sizeof(opt)) < 0)
    {
        printf("setsockopt failed: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
#else
    int opt = 1;
    if (setsockopt(server.socket_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
#endif

    server.address.sin_family = AF_INET;
    server.address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
    server.address.sin_port = htons(port);

#ifdef _WIN32
    if (bind(server.socket_fd,
             (struct sockaddr *)&server.address,
             sizeof(server.address)) == SOCKET_ERROR)
    {
        printf("bind failed: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
#else
    if (bind(server.socket_fd,
             (struct sockaddr *)&server.address,
             sizeof(server.address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
#endif

    if (listen(server.socket_fd, backlog) < 0)
    {
#ifdef _WIN32
        printf("listen failed: %d\n", WSAGetLastError());
#else
        perror("listen failed");
#endif
        exit(EXIT_FAILURE);
    }
    printf("🔒 Bound to 127.0.0.1:%d\n", port);
    printf("✅ Forge server running on port %d\n", port);
    return server;
}

/* =========================================================
   Server Loop
   ========================================================= */

void launch_server(ForgeServer *server)
{
    while (1)
    {
        printf("🔌 Client connected\n");
        fflush(stdout);

#ifdef _WIN32
        SOCKET client_socket = accept(server->socket_fd, NULL, NULL);
        if (client_socket == INVALID_SOCKET)
        {
            printf("accept failed: %d\n", WSAGetLastError());
            continue;
        }
#else
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_socket = accept(
            server->socket_fd,
            (struct sockaddr *)&client_addr,
            &addr_len);

        if (client_socket < 0)
        {
            perror("accept failed");
            continue;
        }
#endif

        handle_client(client_socket);
    }
}

/* =========================================================
   Client Handler
   ========================================================= */

#ifdef _WIN32
void handle_client(SOCKET client_socket)
#else
void handle_client(int client_socket)
#endif
{
    char buffer[30000];
    int bytes_read;

#ifdef _WIN32
    bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
#else
    bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
#endif

    if (bytes_read <= 0)
    {
#ifdef _WIN32
        closesocket(client_socket);
#else
        close(client_socket);
#endif
        return;
    }

    buffer[bytes_read] = '\0';

    ForgeHttpRequest req;
    memset(&req, 0, sizeof(req));

    if (forge_parse_http_request(buffer, &req) != 0)
    {
        send_404(client_socket);
        goto cleanup;
    }

    const ForgeHttpRequest *creq = &req;

    const ForgeRoute *route =
        forge_match_route(
            routes,
            (int)(sizeof(routes) / sizeof(routes[0])),
            creq);

    if (route)
    {
        route->handler(creq, client_socket);
    }
    else
    {
        send_404(client_socket);
    }

cleanup:
#ifdef _WIN32
    closesocket(client_socket);
#else
    close(client_socket);
#endif
}

/* =========================================================
   404 Helper
   ========================================================= */

static void send_404(int client_socket)
{
    forge_send_text(client_socket, "404 Not Found", "Not Found\n");
}

/* =========================================================
   Shutdown
   ========================================================= */

void shutdown_server(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}
