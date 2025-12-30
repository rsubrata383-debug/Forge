#include "forge_manifest.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static char *trim(char *s)
{
    while (isspace((unsigned char)*s))
        s++;
    if (*s == 0)
        return s;

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
    {
        *end-- = '\0';
    }
    return s;
}

int load_dependencies(const char *path, ForgeDep deps[], int max)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        return -1; // distinguish "file missing" from "no deps"
    }

    char line[512];
    int count = 0;
    int in_deps = 0;
    int brace_depth = 0;

    while (fgets(line, sizeof(line), f))
    {
        char *p = trim(line);

        /* detect dependencies start */
        if (!in_deps)
        {
            if (strncmp(p, "\"dependencies\"", 14) == 0 &&
                strchr(p, '{'))
            {
                in_deps = 1;
                brace_depth = 1;
            }
            continue;
        }

        /* track braces */
        for (char *c = p; *c; c++)
        {
            if (*c == '{')
                brace_depth++;
            if (*c == '}')
                brace_depth--;
        }

        if (brace_depth <= 0)
        {
            break; // end of dependencies object
        }

        /* parse: "name": "version" */
        char name[64], version[32];
        if (sscanf(p, "\"%63[^\"]\"%*[: ]\"%31[^\"]\"", name, version) == 2)
        {

            /* security checks */
            if (strstr(name, "..") || strchr(name, '/') || strchr(name, '\\'))
                continue;
            if (strstr(version, "..") || strchr(version, '/') || strchr(version, '\\'))
                continue;

            if (count >= max)
            {
                fprintf(stderr, "⚠ Too many dependencies (max %d)\n", max);
                break;
            }

            strncpy(deps[count].name, name, sizeof(deps[count].name) - 1);
            deps[count].name[sizeof(deps[count].name) - 1] = '\0';

            strncpy(deps[count].version, version, sizeof(deps[count].version) - 1);
            deps[count].version[sizeof(deps[count].version) - 1] = '\0';

            count++;
        }
    }

    fclose(f);
    return count;
}
