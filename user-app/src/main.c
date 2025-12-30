#include <stdio.h>
#include "forge_abi.h"
#include "forge_server.h"

// ✅ C99 ABI check (compile-time failure if wrong version)
#if FORGE_ABI_VERSION != 1
#error "Forge ABI v1.0 required (got " #FORGE_ABI_VERSION ")"
#endif

int main()
{
    setbuf(stdout, NULL);
    printf("🚀 Forge User App v1.0\n");
    printf("🌐 Starting HTTP server: http://localhost:8080\n");

    ForgeServer server = create_forge_server(8080, 10);
    launch_server(&server);

    return 0;
}
