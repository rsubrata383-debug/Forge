#include "forge_http.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif

/* =========================================================
   HTTP Request Parser
   ========================================================= */

int forge_parse_http_request(const char *raw, ForgeHttpRequest *req)
{
  if (!raw || !req)
    return -1;

  const char *line_end = strstr(raw, "\r\n");
  if (!line_end)
    return -1;

  const char *sp1 = strchr(raw, ' ');
  if (!sp1 || sp1 > line_end)
    return -1;

  const char *sp2 = strchr(sp1 + 1, ' ');
  if (!sp2 || sp2 > line_end)
    return -1;

  size_t method_len = sp1 - raw;
  size_t path_len = sp2 - (sp1 + 1);
  size_t ver_len = line_end - (sp2 + 1);

  if (method_len == 0 || path_len == 0 || ver_len == 0)
    return -1;

  if (method_len >= FORGE_MAX_METHOD ||
      path_len >= FORGE_MAX_PATH ||
      ver_len >= FORGE_MAX_VER)
    return -1;

  memcpy(req->method, raw, method_len);
  req->method[method_len] = '\0';

  memcpy(req->path, sp1 + 1, path_len);
  req->path[path_len] = '\0';

  memcpy(req->version, sp2 + 1, ver_len);
  req->version[ver_len] = '\0';

  /* Validate method */
  for (size_t i = 0; i < method_len; i++)
  {
    if (!isupper((unsigned char)req->method[i]))
      return -1;
  }

  /* Origin-form only */
  if (req->path[0] != '/')
    return -1;

  /* HTTP version */
  if (strcmp(req->version, "HTTP/1.1") != 0 &&
      strcmp(req->version, "HTTP/1.0") != 0)
    return -1;

  return 0;
}

/* =========================================================
   HTTP Response Helpers
   ========================================================= */

void forge_send_text(int client_socket,
                     const char *status,
                     const char *body)
{
  char resp[512];

  snprintf(resp, sizeof(resp),
           "HTTP/1.1 %s\r\n"
           "Content-Type: text/plain; charset=utf-8\r\n"
           "Content-Length: %zu\r\n"
           "Connection: close\r\n"
           "\r\n"
           "%s",
           status,
           strlen(body),
           body);

#ifdef _WIN32
  send(client_socket, resp, (int)strlen(resp), 0);
#else
  write(client_socket, resp, strlen(resp));
#endif
}

void forge_send_json(int client_socket,
                     const char *status,
                     const char *body)
{
  char resp[512];

  snprintf(resp, sizeof(resp),
           "HTTP/1.1 %s\r\n"
           "Content-Type: application/json; charset=utf-8\r\n"
           "Content-Length: %zu\r\n"
           "Connection: close\r\n"
           "\r\n"
           "%s",
           status,
           strlen(body),
           body);

#ifdef _WIN32
  send(client_socket, resp, (int)strlen(resp), 0);
#else
  write(client_socket, resp, strlen(resp));
#endif
}
