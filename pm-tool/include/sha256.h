// pm-tool/include/sha256.h
#ifndef SHA256_H
#define SHA256_H

#include <stddef.h>
//TODO
int sha256_file(const char *path, unsigned char hash[32]);
void sha256_hex(const unsigned char hash[32], char out[65]);

#endif
