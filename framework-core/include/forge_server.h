#ifndef FORGE_SERVER_H
#define FORGE_SERVER_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

#include <stddef.h>
#include "forge_abi.h"
/* =========================================================
   C99 Static Assert
   ========================================================= */

#define STATIC_ASSERT(cond, msg) \
    typedef char static_assertion_##msg[(cond) ? 1 : -1]

/* =========================================================
   C99 Alignment Helper
   ========================================================= */

#define ALIGNOF(T) offsetof(struct { char c; T member; }, member)

STATIC_ASSERT(
    FORGE_ABI_VERSION == 1,
    forge_server_abi_mismatch);
/* =========================================================
   Forge Server Structure
   ========================================================= */

typedef struct
{
#ifdef _WIN32
    SOCKET socket_fd;
#else
    int socket_fd;
#endif
    int port;
    int backlog;
    struct sockaddr_in address;
} ForgeServer;

/* =========================================================
   Compile-time Safety Checks
   ========================================================= */

STATIC_ASSERT(sizeof(ForgeServer) > 0, forge_server_not_empty);
STATIC_ASSERT(sizeof(ForgeServer) <= 128, forge_server_size_reasonable);

/* Alignment must be safe */
STATIC_ASSERT(
    ALIGNOF(ForgeServer) >= ALIGNOF(
#ifdef _WIN32
                                SOCKET
#else
                                int
#endif
                                ),
    forge_server_alignment_valid);

/* =========================================================
   API
   ========================================================= */

ForgeServer create_forge_server(int port, int backlog);
void launch_server(ForgeServer *server);

#ifdef _WIN32
void handle_client(SOCKET client_socket);
#else
void handle_client(int client_socket);
#endif

void shutdown_server(void);

#endif /* FORGE_SERVER_H */
