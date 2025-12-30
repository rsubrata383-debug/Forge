#ifndef FORGE_PM_H
#define FORGE_PM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Remove ForgeDep - already in forge_manifest.h

bool extract_zip_safe(const char *zip_path, const char *dest_dir, uint64_t max_size);
#endif
