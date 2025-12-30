#include "forge_lock.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define LOCK_FILE "forge.lock"

int lock_exists(void)
{
    FILE *f = fopen(LOCK_FILE, "r");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

int lock_has(const char *pkg)
{
    FILE *f = fopen(LOCK_FILE, "r");
    if (!f)
        return 0;

    char p[128], u[512], s[65];
    while (fscanf(f, "%127s %511s %64s", p, u, s) == 3)
    {
        if (strcmp(p, pkg) == 0)
        {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

int lock_get(const char *pkg,
             char *url_out, int url_sz,
             char *sha_out, int sha_sz)
{
    FILE *f = fopen(LOCK_FILE, "r");
    if (!f)
        return 0;

    char p[128], u[512], s[65];
    while (fscanf(f, "%127s %511s %64s", p, u, s) == 3)
    {
        if (strcmp(p, pkg) == 0)
        {
            // Safety: Ensure we don't overflow the provided output buffers
            snprintf(url_out, (size_t)url_sz, "%s", u);
            snprintf(sha_out, (size_t)sha_sz, "%s", s);
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

int lock_add(const char *pkg, const char *url, const char *sha)
{
    FILE *f = fopen(LOCK_FILE, "a");
    if (!f)
        return 0;
    fprintf(f, "%s %s %s\n", pkg, url, sha);
    fclose(f);
    return 1;
}

void lock_remove(const char *pkg)
{
    FILE *in = fopen(LOCK_FILE, "r");
    if (!in)
        return;

    FILE *out = fopen("forge.lock.tmp", "w");
    if (!out)
    {
        fclose(in);
        return;
    }

    char p[128], u[512], s[65];
    while (fscanf(in, "%127s %511s %64s", p, u, s) == 3)
    {
        if (strcmp(p, pkg) != 0)
            fprintf(out, "%s %s %s\n", p, u, s);
    }

    fclose(in);
    fclose(out);

#ifdef _WIN32
    // Windows requires the old file to be gone before rename
    remove(LOCK_FILE);
#endif
    rename("forge.lock.tmp", LOCK_FILE);
}

/* FIX: Only one definition of lock_read_all allowed */
void lock_read_all(void (*cb)(const char *pkg, const char *url, const char *sha))
{
    if (!cb)
        return; // Guard against NULL callback

    FILE *f = fopen(LOCK_FILE, "r");
    if (!f)
        return;

    char p[128], u[512], s[65];
    while (fscanf(f, "%127s %511s %64s", p, u, s) == 3)
    {
        cb(p, u, s);
    }

    fclose(f);
}
