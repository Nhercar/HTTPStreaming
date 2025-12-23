#pragma once

#include <cstdint>
#include <string>

#include "socket_server.h"

#ifdef _WIN32
    #define INVALID_SOCKET_T INVALID_SOCKET
    #define SOCKET_ERROR_T SOCKET_ERROR
    inline void closeSocket(socket_t sock) { closesocket(sock); }
#else
    #define INVALID_SOCKET_T -1
    #define SOCKET_ERROR_T -1
    inline void closeSocket(socket_t sock) { close(sock); }
#endif

bool sendAll(socket_t sock, const uint8_t* data, size_t len);
bool receiveAll(socket_t sock, uint8_t* data, size_t len);