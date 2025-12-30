#ifndef FORGE_HTTP_H
#define FORGE_HTTP_H

#include <stddef.h>

/* =========================================================
   HTTP LIMITS (ABI-STABLE)
   ========================================================= */

#define FORGE_MAX_METHOD 8
#define FORGE_MAX_PATH 256
#define FORGE_MAX_VER 16

/* =========================================================
   HTTP Request Structure
   ========================================================= */

typedef struct
{
   char method[FORGE_MAX_METHOD];
   char path[FORGE_MAX_PATH];
   char version[FORGE_MAX_VER];
} ForgeHttpRequest;

/* =========================================================
   HTTP Parsing
   ========================================================= */

int forge_parse_http_request(const char *raw,
                             ForgeHttpRequest *req);

/* =========================================================
   HTTP Response Helpers (PUBLIC API)
   ========================================================= */

void forge_send_text(int client_socket,
                     const char *status,
                     const char *body);

void forge_send_json(int client_socket,
                     const char *status,
                     const char *body);

#endif /* FORGE_HTTP_H */
