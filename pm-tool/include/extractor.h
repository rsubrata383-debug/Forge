// pm-tool/extractor.h - ADD this function declaration
#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include <stdbool.h>
#include <stdint.h>

// OLD function (if exists)
bool extract_zip(const char *zip_path, const char *dest_dir);

// ✅ NEW function - ADD THIS LINE
bool extract_zip_safe(const char *zip_path, const char *dest_dir, uint64_t max_size);

#endif
