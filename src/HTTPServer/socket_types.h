#pragma once
#if defined(_WIN32) || defined(_WIN64)
#include <WinSock2.h>
using socket_t = SOCKET;
#else
// POSIX socket includes and compatibility definitions
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

using socket_t = int;

// Compatibility macros used in Windows code
#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif

// Provide closesocket alias for POSIX
inline void closesocket(int s) { ::close(s); }
#endif
