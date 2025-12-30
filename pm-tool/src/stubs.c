// pm-tool/stubs.c - FIXED signatures + string.h
#include <string.h>
#include "sha256.h"
#include "extractor.h"
#include "downloader.h"
#include "../include/forge_manifest.h"
#include "../include/forge_lock.h"

// Match EXACT signatures from headers
bool extract_zip_safe(const char *zip_path, const char *dest_dir, uint64_t max_size)
{
  (void)zip_path;
  (void)dest_dir;
  (void)max_size;
  return true;
}

int download_file(const char *url, const char *output_path)
{
  (void)url;
  (void)output_path;
  return 1;
}

int sha256_file(const char *path, unsigned char hash[32])
{
  (void)path;
  (void)hash;
  return 0;
}

void sha256_hex(const unsigned char hash[32], char out[65])
{
  (void)hash;
  strcpy(out, "deadbeefdeadbeefdeadbeefdeadbeefdeadbeef");
}

int lock_exists(void)
{
  return 1;
}

int lock_get(const char *pkg, char *url, int urlsz, char *sha, int shasz)
{
  (void)pkg;
  (void)url;
  (void)urlsz;
  (void)sha;
  (void)shasz;
  return 1;
}

void lock_read_all(void (*cb)(const char *pkg, const char *url, const char *sha))
{
  (void)cb;
}

void lock_remove(const char *pkg)
{
  (void)pkg;
}

int load_dependencies(const char *path, ForgeDep *deps, int max)
{
  (void)path;
  (void)deps;
  (void)max;
  return 0;
}
