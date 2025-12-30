#include "downloader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#endif

int download_file(const char *url, const char *output_path)
{

#ifdef _WIN32
    int success = 0;

    HINTERNET hInternet = InternetOpenA(
        "ForgePM",
        INTERNET_OPEN_TYPE_PRECONFIG,
        NULL, NULL, 0);
    if (!hInternet)
    {
        printf("❌ InternetOpen failed\n");
        return 0;
    }

    HINTERNET hFile = InternetOpenUrlA(
        hInternet,
        url,
        NULL, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
        0);
    if (!hFile)
    {
        printf("❌ Failed to open URL: %s\n", url);
        InternetCloseHandle(hInternet);
        return 0;
    }

    /* ---- Check HTTP status ---- */
    DWORD status = 0;
    DWORD status_len = sizeof(status);
    if (!HttpQueryInfoA(
            hFile,
            HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
            &status,
            &status_len,
            NULL) ||
        status != 200)
    {

        printf("❌ HTTP error %lu while downloading %s\n", status, url);
        goto cleanup;
    }

    FILE *out = fopen(output_path, "wb");
    if (!out)
    {
        printf("❌ Failed to open output file: %s\n", output_path);
        goto cleanup;
    }

    char buffer[8192];
    DWORD bytesRead = 0;

    while (InternetReadFile(hFile, buffer, sizeof(buffer), &bytesRead))
    {
        if (bytesRead == 0)
            break;

        size_t written = fwrite(buffer, 1, bytesRead, out);
        if (written != bytesRead)
        {
            printf("❌ Disk write failed\n");
            fclose(out);
            goto cleanup;
        }
    }

    if (GetLastError() != ERROR_SUCCESS)
    {
        printf("❌ Network error during download\n");
        fclose(out);
        goto cleanup;
    }

    fclose(out);
    success = 1;

cleanup:
    InternetCloseHandle(hFile);
    InternetCloseHandle(hInternet);

    if (!success)
    {
        remove(output_path); // prevent partial file
    }

    return success;

#else
    /* POSIX fallback (requires curl) */
    char cmd[1024];
    if (snprintf(cmd, sizeof(cmd),
                 "curl -fL \"%s\" -o \"%s\"",
                 url, output_path) >= (int)sizeof(cmd))
    {
        return 0;
    }

    int rc = system(cmd);
    if (rc != 0)
    {
        remove(output_path);
        return 0;
    }
    return 1;
#endif
}
