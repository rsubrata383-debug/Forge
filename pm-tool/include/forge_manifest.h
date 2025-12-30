#ifndef FORGE_MANIFEST_H
#define FORGE_MANIFEST_H

typedef struct {
    char name[64];
    char version[32];
} ForgeDep;

int load_dependencies(
    const char *manifest_path,
    ForgeDep deps[],
    int max_deps
);

#endif
