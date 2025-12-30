#include "sha256.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ===== SHA256 CORE ===== */

typedef struct {
    uint32_t h[8];
    uint8_t  block[64];
    uint64_t len;
    size_t   curlen;
} SHA256_CTX;

static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
    0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
    0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
    0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
    0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
    0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208
};

#define ROTR(x,n) ((x>>n)|(x<<(32-n)))

static void sha256_transform(SHA256_CTX *ctx) {
    uint32_t m[64], a,b,c,d,e,f,g,h,t1,t2;
    for (int i=0;i<16;i++)
        m[i] = (ctx->block[i*4]<<24)|(ctx->block[i*4+1]<<16)|(ctx->block[i*4+2]<<8)|(ctx->block[i*4+3]);
    for (int i=16;i<64;i++)
        m[i] = (ROTR(m[i-2],17)^ROTR(m[i-2],19)^(m[i-2]>>10)) + m[i-7] +
               (ROTR(m[i-15],7)^ROTR(m[i-15],18)^(m[i-15]>>3)) + m[i-16];

    a=ctx->h[0]; b=ctx->h[1]; c=ctx->h[2]; d=ctx->h[3];
    e=ctx->h[4]; f=ctx->h[5]; g=ctx->h[6]; h=ctx->h[7];

    for (int i=0;i<64;i++) {
        t1 = h + (ROTR(e,6)^ROTR(e,11)^ROTR(e,25)) + ((e&f)^((~e)&g)) + K[i] + m[i];
        t2 = (ROTR(a,2)^ROTR(a,13)^ROTR(a,22)) + ((a&b)^(a&c)^(b&c));
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }

    ctx->h[0]+=a; ctx->h[1]+=b; ctx->h[2]+=c; ctx->h[3]+=d;
    ctx->h[4]+=e; ctx->h[5]+=f; ctx->h[6]+=g; ctx->h[7]+=h;
}

static void sha256_init(SHA256_CTX *ctx) {
    ctx->h[0]=0x6a09e667; ctx->h[1]=0xbb67ae85;
    ctx->h[2]=0x3c6ef372; ctx->h[3]=0xa54ff53a;
    ctx->h[4]=0x510e527f; ctx->h[5]=0x9b05688c;
    ctx->h[6]=0x1f83d9ab; ctx->h[7]=0x5be0cd19;
    ctx->len=0; ctx->curlen=0;
}

static void sha256_update(SHA256_CTX *ctx, const unsigned char *data, size_t len) {
    while (len--) {
        ctx->block[ctx->curlen++] = *data++;
        if (ctx->curlen == 64) {
            sha256_transform(ctx);
            ctx->len += 512;
            ctx->curlen = 0;
        }
    }
}

static void sha256_final(SHA256_CTX *ctx, unsigned char hash[32]) {
    ctx->len += ctx->curlen * 8;
    ctx->block[ctx->curlen++] = 0x80;

    while (ctx->curlen < 56)
        ctx->block[ctx->curlen++] = 0;

    for (int i=7;i>=0;i--)
        ctx->block[ctx->curlen++] = (ctx->len >> (i*8)) & 0xff;

    sha256_transform(ctx);

    for (int i=0;i<8;i++) {
        hash[i*4]   = (ctx->h[i]>>24)&0xff;
        hash[i*4+1] = (ctx->h[i]>>16)&0xff;
        hash[i*4+2] = (ctx->h[i]>>8)&0xff;
        hash[i*4+3] = ctx->h[i]&0xff;
    }
}

/* ===== PUBLIC API ===== */

int sha256_file(const char *path, unsigned char hash[32]) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return -1;  // file open failed
    }

    SHA256_CTX ctx;
    sha256_init(&ctx);

    unsigned char buf[4096];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        sha256_update(&ctx, buf, n);
    }

    if (ferror(f)) {
        fclose(f);
        return -1;  // read error
    }

    fclose(f);
    sha256_final(&ctx, hash);
    return 0;  // success
}


void sha256_hex(const unsigned char hash[32], char out[65]) {
    for (int i=0;i<32;i++)
        sprintf(out+i*2,"%02x",hash[i]);
    out[64]=0;
}
