#ifndef FORGE_ABI_H
#define FORGE_ABI_H

// Forge ABI v0.1.0 - Application Binary Interface
#define FORGE_ABI_VERSION 1
#define FORGE_VERSION "0.1.0"
#define FORGE_API __declspec(dllexport)

// Cross-platform compatibility
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#endif
