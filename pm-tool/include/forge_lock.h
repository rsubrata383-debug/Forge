#ifndef FORGE_LOCK_H
#define FORGE_LOCK_H

#ifdef __cplusplus
extern "C"
{
#endif

    int lock_exists(void);

    int lock_has(const char *pkg);

    int lock_get(
        const char *pkg,
        char *url_out, int url_sz,
        char *sha_out, int sha_sz);

    int lock_add(
        const char *pkg,
        const char *url,
        const char *sha);

    void lock_remove(const char *pkg);

    void lock_read_all(
        void (*cb)(const char *pkg,
                   const char *url,
                   const char *sha));

#ifdef __cplusplus
}
#endif

#endif
