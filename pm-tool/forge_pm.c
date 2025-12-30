#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>

#ifndef PATH_MAX
#ifdef _WIN32
#define PATH_MAX 260
#else
#define PATH_MAX 4096
#endif
#endif

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#define PATH_SEP '\\'
#define mkdir(path, mode) CreateDirectoryA(path, NULL)
#define rmdir(path) RemoveDirectoryA(path)
#define unlink(path) DeleteFileA(path)
#define access(path, mode) _access(path, 0)
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#define PATH_SEP '/'
#endif

#include "sha256.h"
#include "extractor.h"
#include "downloader.h"
#include "forge_manifest.h"
#include "forge_lock.h"
#include "forge_pm.h"
/* ---------------- Constants ---------------- */
#define VENDOR_ROOT "user-app/vendor/forge"
#define REGISTRY_PATH "user-app/vendor/forge/installed.txt"

#define MANIFEST_URL_FMT "http://localhost:8080/%s/%s/forge.json"
#define ZIP_URL_FMT "http://localhost:8080/%s/%s/%s.zip"

#define MAX_STACK 64
#define MAX_PKG 256
#define MAX_URL 512
#define MAX_EXTRACT_SIZE (512ULL * 1024 * 1024)
#define REG_LINE_BUF 4096
#define MAX_RECURSION_DEPTH 100 // Prevent stack overflow

/* ---------------- Forward Declarations ---------------- */
static int cmd_install(const char *pkg, const char *zip_url, const char *expected_sha);
static void cmd_remove(const char *pkg);
static void cmd_list(void);
static int install_with_deps(const char *pkg);
static void ensure_dirs(const char *dir);
static int remove_directory(const char *path, int depth);
static int is_installed(const char *pkg);
static int fetch_and_parse_manifest(const char *name, const char *version, ForgeDep deps[], int max);
static int cmd_update_package(const char *pkg);
static int fetch_latest_version(const char *name, char *out, size_t sz);

/* ---------------- Dependency Stack ---------------- */
static int stack_top = 0;
static int depth = 0;
static int lock_enforced = 0;
static char stack[MAX_STACK][MAX_PKG];

static int stack_contains(const char *pkg)
{
    for (int i = 0; i < stack_top; i++)
        if (strcmp(stack[i], pkg) == 0)
            return 1;
    return 0;
}

static int stack_push(const char *pkg)
{
    if (stack_top >= MAX_STACK)
        return 0;
    strncpy(stack[stack_top], pkg, MAX_PKG - 1);
    stack[stack_top][MAX_PKG - 1] = '\0';
    stack_top++;
    return 1;
}

static void stack_pop(void)
{
    if (stack_top > 0)
        stack_top--;
}

#ifndef _WIN32
static void fsync_parent_dir(const char *path)
{
    char *path_copy = strdup(path);
    if (!path_copy)
        return;
    char *parent = dirname(path_copy);
    int fd = open(parent, O_RDONLY);
    if (fd != -1)
    {
        fsync(fd);
        close(fd);
    }
    free(path_copy);
}
#endif

static int safe_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return (n >= 0 && (size_t)n < size);
}

static void get_forge_home(char *out, size_t sz)
{
#ifdef _WIN32
    const char *home = getenv("USERPROFILE");
#else
    const char *home = getenv("HOME");
#endif
    if (!home)
        home = ".";
    snprintf(out, sz, "%s/.forge", home);
}

static void get_cache_dir(char *out, size_t sz)
{
    char base[PATH_MAX];
    get_forge_home(base, sizeof(base));
    snprintf(out, sz, "%s/cache", base);
}

static int cache_get_zip(const char *sha, char *out, size_t sz)
{
    char cache_dir[PATH_MAX];
    get_cache_dir(cache_dir, sizeof(cache_dir));
    snprintf(out, sz, "%s/%s.zip", cache_dir, sha);
    return access(out, 0) == 0;
}

static int cache_store_zip(const char *sha, const char *src_zip)
{
    char cache_dir[PATH_MAX], final[PATH_MAX], tmp[PATH_MAX];
    get_cache_dir(cache_dir, sizeof(cache_dir));
    ensure_dirs(cache_dir);

    snprintf(final, sizeof(final), "%s/%s.zip", cache_dir, sha);
    snprintf(tmp, sizeof(tmp), "%s.tmp", final);

    if (access(final, 0) == 0)
        return 1; // already cached

#ifdef _WIN32
    if (!CopyFileA(src_zip, tmp, FALSE))
        return 0;
    return MoveFileExA(tmp, final, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    FILE *in = fopen(src_zip, "rb");
    FILE *outf = fopen(tmp, "wb");
    if (!in || !outf)
    {
        if (in)
            fclose(in);
        if (outf)
            fclose(outf);
        return 0;
    }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, outf);

    fclose(in);
    fflush(outf);
    fsync(fileno(outf));
    fclose(outf);
    int result = rename(tmp, final);
    fsync_parent_dir(final);
    return result == 0;
#endif
}

/* ---------------- Path Utilities ---------------- */
static void ensure_dirs(const char *path)
{
    char tmp[PATH_MAX];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

#ifdef _WIN32
    if (strlen(tmp) >= 2 && tmp[1] == ':')
        memmove(tmp, tmp + 2, strlen(tmp) + 1); // FIXED: +1 for null terminator
#endif

    for (char *p = tmp; *p; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            char c = *p;
            *p = '\0';
#ifdef _WIN32
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = c;
        }
    }
#ifdef _WIN32
    _mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif
}

/* ---------------- Validation ---------------- */
static int sanitize_pkg(const char *pkg, char *name, char *version)
{
    if (sscanf(pkg, "%63[^@]@%31[^@]", name, version) != 2)
        return 0;
    if (strstr(name, "..") || strchr(name, '/') || strchr(name, '\\'))
        return 0;
    if (strstr(version, "..") || strchr(version, '/') || strchr(version, '\\'))
        return 0;
    return 1;
}

/* ---------------- Manifest ---------------- */
static int fetch_and_parse_manifest(const char *name, const char *version, ForgeDep deps[], int max)
{
    char url[MAX_URL];
    snprintf(url, sizeof(url), MANIFEST_URL_FMT, name, version);

    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "tmp_manifest_%s_%s.json", name, version);

    if (!download_file(url, tmp))
        return -1;

    // FIXED: Parse before fclose
    int n = load_dependencies(tmp, deps, max);

    FILE *f = fopen(tmp, "rb");
    if (!f)
    {
        unlink(tmp);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);

    if (sz <= 0 || sz > 1024 * 1024)
    {
        unlink(tmp);
        return -1;
    }

    unlink(tmp);
    return n;
}

/* ---------------- Dependency Resolver ---------------- */
static int install_with_deps(const char *pkg)
{
    int result = 0;

    if (depth > MAX_RECURSION_DEPTH)
    {
        fprintf(stderr, "❌ Recursion depth exceeded\n");
        return 0;
    }

    if (stack_contains(pkg))
    {
        fprintf(stderr, "❌ Circular dependency: %s\n", pkg);
        goto cleanup;
    }

    if (!stack_push(pkg))
    {
        fprintf(stderr, "❌ Dependency stack overflow\n");
        goto cleanup;
    }

    if (is_installed(pkg))
    {
        printf("✔ %s already installed\n", pkg);
        result = 1;
        goto cleanup;
    }

    char name[64], version[32];
    if (!sanitize_pkg(pkg, name, version))
    {
        goto cleanup;
    }

    ForgeDep deps[32];
    int dep_count = fetch_and_parse_manifest(name, version, deps, 32);
    if (dep_count < 0)
    {
        fprintf(stderr, "❌ Failed to fetch manifest for %s@%s\n", name, version);
        goto cleanup;
    }

    for (int i = 0; i < dep_count; i++)
    {
        char dep[MAX_PKG];
        snprintf(dep, sizeof(dep), "%.63s@%.31s", deps[i].name, deps[i].version);
        printf("%*s↳ Resolving %s\n", depth * 2, "", dep);
        depth++;
        if (!install_with_deps(dep))
        {
            depth--;
            goto cleanup;
        }
        depth--;
    }

    char zip_url[MAX_URL];
    char sha[65] = {0};
    int locked = lock_enforced;

    if (locked)
    {
        if (!lock_get(pkg, zip_url, sizeof(zip_url), sha, sizeof(sha)))
        {
            fprintf(stderr, "❌ %s not in forge.lock\n", pkg);
            goto cleanup;
        }
    }
    else
    {
        snprintf(zip_url, sizeof(zip_url), ZIP_URL_FMT, name, version, name);
    }

    printf("%*s↳ Installing %s\n", depth * 2, "", pkg);
    depth++;
    result = cmd_install(pkg, zip_url, locked ? sha : NULL);
    depth--;

cleanup:
    stack_pop();
    return result;
}

/* ---------------- Registry ---------------- */
static int is_installed(const char *pkg)
{
    FILE *f = fopen(REGISTRY_PATH, "r");
    if (!f)
        return 0;

    char line[REG_LINE_BUF];
    while (fgets(line, sizeof(line), f))
    {
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, pkg) == 0)
        {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

/* ---------------- Filesystem ---------------- */
static int remove_directory(const char *path, int rdepth)
{
    if (rdepth > MAX_RECURSION_DEPTH)
        return 0; // Prevent stack overflow

#ifdef _WIN32
    char spec[PATH_MAX];
    WIN32_FIND_DATAA ffd;
    if (!safe_snprintf(spec, sizeof(spec), "%s\\*", path))
        return 0;

    HANDLE hFind = FindFirstFileA(spec, &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0;

    do
    {
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
            continue;

        char sub[PATH_MAX];
        if (!safe_snprintf(sub, sizeof(sub), "%s\\%s", path, ffd.cFileName))
            continue;

        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            remove_directory(sub, rdepth + 1);
        else
            DeleteFileA(sub);
    } while (FindNextFileA(hFind, &ffd) != 0);

    FindClose(hFind);
    return RemoveDirectoryA(path);
#else
    DIR *d = opendir(path);
    if (!d)
        return 0;

    struct dirent *p;
    while ((p = readdir(d)))
    {
        if (strcmp(p->d_name, ".") == 0 || strcmp(p->d_name, "..") == 0)
            continue;

        char sub[PATH_MAX];
        if (!safe_snprintf(sub, sizeof(sub), "%s/%s", path, p->d_name))
            continue;

        struct stat st;
        if (lstat(sub, &st) < 0)
            continue;

        if (S_ISDIR(st.st_mode))
            remove_directory(sub, rdepth + 1);
        else
            unlink(sub);
    }
    closedir(d);
    return rmdir(path) == 0;
#endif
}

static void cmd_remove(const char *pkg)
{
    char name[64], version[32];
    if (!sanitize_pkg(pkg, name, version))
        return;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s/%s", VENDOR_ROOT, name, version);
    remove_directory(path, 0);

    FILE *in = fopen(REGISTRY_PATH, "r");
    if (!in)
        return;

    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s.tmp", REGISTRY_PATH);
    FILE *out = fopen(tmp, "w");
    if (!out)
    {
        fclose(in);
        return;
    }

    char line[REG_LINE_BUF];
    while (fgets(line, sizeof(line), in))
    {
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, pkg) != 0)
            fprintf(out, "%s\n", line);
    }

    fclose(in);
    fflush(out);
#ifndef _WIN32
    fsync(fileno(out));
#endif
    fclose(out);

    // FIXED: Check rename result
    if (rename(tmp, REGISTRY_PATH) != 0)
    {
        fprintf(stderr, "❌ Failed to update registry\n");
        unlink(tmp);
    }

    lock_remove(pkg);
}

/* ---------------- Commands ---------------- */
static int cmd_install(const char *pkg, const char *zip_url, const char *expected_sha)
{
    char name[64], version[32], zip[PATH_MAX], final_dir[PATH_MAX], temp_dir[PATH_MAX], backup[PATH_MAX];
    ensure_dirs(VENDOR_ROOT);

    if (is_installed(pkg))
        return 1;
    if (!sanitize_pkg(pkg, name, version))
        return 0;

    if (!safe_snprintf(zip, sizeof(zip), "tmp_%s_%s.zip", name, version) ||
        !safe_snprintf(final_dir, sizeof(final_dir), "%s/%s/%s", VENDOR_ROOT, name, version) ||
        !safe_snprintf(temp_dir, sizeof(temp_dir), "%s.tmp", final_dir) ||
        !safe_snprintf(backup, sizeof(backup), "%s.old", final_dir))
        return 0;

    remove_directory(temp_dir, 0);
    remove_directory(backup, 0);

    char cached_zip[PATH_MAX] = {0};
    int have_cache = expected_sha && cache_get_zip(expected_sha, cached_zip, sizeof(cached_zip));

    if (have_cache)
    {
        // FIXED: Safe copy with null termination
        strncpy(zip, cached_zip, PATH_MAX - 1);
        zip[PATH_MAX - 1] = '\0';
    }
    else
    {
        if (!download_file(zip_url, zip))
            goto fail;

        unsigned char hash[32];
        char actual[65];
        if (sha256_file(zip, hash) != 0)
            goto fail;
        sha256_hex(hash, actual);

        if (expected_sha && strcmp(actual, expected_sha) != 0)
        {
            fprintf(stderr, "❌ Hash mismatch for %s (expected %s, got %s)\n", pkg, expected_sha, actual);
            goto fail;
        }

        cache_store_zip(actual, zip);
    }

    ensure_dirs(temp_dir);
    if (!extract_zip(zip, temp_dir))
        goto fail;

#ifdef _WIN32
    if (access(final_dir, 0) == 0)
    {
        if (!MoveFileExA(final_dir, backup, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            goto fail;
    }
    if (!MoveFileExA(temp_dir, final_dir, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        goto rollback;
#else
    if (rename(final_dir, backup) != 0 && errno != ENOENT)
        goto fail;
    if (rename(temp_dir, final_dir) != 0)
        goto rollback;
    fsync_parent_dir(final_dir);
#endif

    // Atomic Registry Update
    char reg_tmp[PATH_MAX];
    snprintf(reg_tmp, sizeof(reg_tmp), "%s.tmp", REGISTRY_PATH);
    FILE *in = fopen(REGISTRY_PATH, "r");
    FILE *out = fopen(reg_tmp, "w");
    if (!out)
    {
        if (in)
            fclose(in);
        goto rollback;
    }

    char line[REG_LINE_BUF];
    int found = 0;
    if (in)
    {
        while (fgets(line, sizeof(line), in))
        {
            line[strcspn(line, "\r\n")] = '\0';
            if (strcmp(line, pkg) == 0)
                found = 1;
            fprintf(out, "%s\n", line);
        }
        fclose(in);
    }
    if (!found)
        fprintf(out, "%s\n", pkg);

    fflush(out);
#ifndef _WIN32
    fsync(fileno(out));
#endif
    fclose(out);

#ifdef _WIN32
    if (!MoveFileExA(reg_tmp, REGISTRY_PATH, MOVEFILE_REPLACE_EXISTING))
        goto rollback;
#else
    if (rename(reg_tmp, REGISTRY_PATH) != 0)
        goto rollback;
    fsync_parent_dir(REGISTRY_PATH);
#endif

    remove_directory(backup, 0);
    unlink(zip);
    return 1;

rollback:
#ifdef _WIN32
    MoveFileExA(backup, final_dir, MOVEFILE_REPLACE_EXISTING);
#else
    if (access(backup, 0) == 0)
        rename(backup, final_dir);
#endif
fail:
    unlink(zip);
    remove_directory(temp_dir, 0);
    return 0;
}

static void cmd_list(void)
{
    ensure_dirs(VENDOR_ROOT);
    FILE *f = fopen(REGISTRY_PATH, "r");
    if (!f)
        return;
    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        line[strcspn(line, "\r\n")] = '\0';
        printf("  • %s\n", line);
    }
    fclose(f);
}

int version_cmp(const char *a, const char *b)
{
    int ma = 0, mi = 0, pa = 0;
    int mb = 0, mj = 0, pb = 0;

    // FIXED: Verify sscanf parsed all 3 fields
    if (sscanf(a, "%d.%d.%d", &ma, &mi, &pa) != 3)
        return -1;
    if (sscanf(b, "%d.%d.%d", &mb, &mj, &pb) != 3)
        return 1;

    if (ma != mb)
        return ma - mb;
    if (mi != mj)
        return mi - mj;
    return pa - pb;
}

static void cmd_update_all(void)
{
    FILE *f = fopen(REGISTRY_PATH, "r");
    if (!f)
        return;

    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        line[strcspn(line, "\r\n")] = '\0';
        // FIXED: Safe check for valid package line
        if (line[0] && strlen(line) > 1 && strchr(line, '@'))
            cmd_update_package(line);
    }
    fclose(f);
}

static int cmd_update_package(const char *pkg)
{
    char name[64], old_ver[32];
    if (!sanitize_pkg(pkg, name, old_ver))
        return 0;

    char latest[32];
    if (!fetch_latest_version(name, latest, sizeof(latest)))
        return 0;

    if (version_cmp(latest, old_ver) <= 0)
    {
        printf("✔ %s is up to date\n", pkg);
        return 1;
    }

    char new_pkg[128];
    snprintf(new_pkg, sizeof(new_pkg), "%s@%s", name, latest);

    printf("⬆ Updating %s → %s\n", pkg, new_pkg);
    if (!install_with_deps(new_pkg))
    {
        printf("❌ Update failed, keeping old version\n");
        return 0;
    }
    cmd_remove(pkg);
    lock_remove(pkg);
    return 1;
}

static int fetch_latest_version(const char *name, char *out, size_t sz)
{
    char url[512];
    snprintf(url, sizeof(url), "https://forge-packages.example.com/%s/latest.txt", name);

    char tmp[256];
    snprintf(tmp, sizeof(tmp), "tmp_%s_latest.txt", name);

    if (!download_file(url, tmp))
        return 0;

    FILE *f = fopen(tmp, "r");
    if (!f)
    {
        unlink(tmp);
        return 0;
    }

    fgets(out, sz, f);
    out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
    unlink(tmp);
    return 1;
}

/* ---------------- Lockfile Callback ---------------- */
static void install_locked_cb(const char *pkg, const char *url, const char *sha)
{
    if (!cmd_install(pkg, url, sha))
        fprintf(stderr, "❌ Failed to install locked package: %s\n", pkg);
}

/* ---------------- Main ---------------- */
int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Forge Package Manager v1.0\n");
        printf("Usage: forge <list|install|remove|update>\n");
        return 0;
    }

    if (!strcmp(argv[1], "list"))
    {
        cmd_list();
    }
    else if (!strcmp(argv[1], "install"))
    {
        if (argc == 2)
        {
            if (!lock_exists())
            {
                fprintf(stderr, "❌ forge.lock not found\n");
                return 1;
            }
            int saved_lock = lock_enforced; // FIXED: Local state
            lock_enforced = 1;
            lock_read_all(install_locked_cb);
            lock_enforced = saved_lock;
        }
        else if (argc == 3)
        {
            install_with_deps(argv[2]);
        }
        else
        {
            printf("Usage: forge install [pkg@version]\n");
            return 1;
        }
    }
    else if (!strcmp(argv[1], "remove") && argc == 3)
    {
        cmd_remove(argv[2]);
    }
    else if (!strcmp(argv[1], "update"))
    {
        if (argc == 2)
            cmd_update_all();
        else if (argc == 3)
            cmd_update_package(argv[2]);
        else
            printf("Usage: forge update [pkg@version]\n");
    }
    else
    {
        printf("Unknown command: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
