#include "../include/forge_router.h"
#include <string.h>

const ForgeRoute *forge_match_route(
    const ForgeRoute *routes,
    int count,
    const ForgeHttpRequest *req)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(routes[i].method, req->method) == 0 &&
            strcmp(routes[i].path, req->path) == 0)
        {
            return &routes[i];
        }
    }
    return NULL;
}
