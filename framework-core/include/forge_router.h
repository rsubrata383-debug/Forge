#ifndef FORGE_ROUTER_H
#define FORGE_ROUTER_H

#include "forge_http.h"

/* =========================================================
   Router Types
   ========================================================= */

typedef void (*ForgeRouteHandler)(
    const ForgeHttpRequest *req,
    int client_socket);

typedef struct
{
  const char *method;
  const char *path;
  ForgeRouteHandler handler;
} ForgeRoute;

/* =========================================================
   Router API
   ========================================================= */

const ForgeRoute *forge_match_route(
    const ForgeRoute *routes,
    int count,
    const ForgeHttpRequest *req);

#endif /* FORGE_ROUTER_H */
