#pragma once
#if defined(_WIN32) || defined(_WIN64)
#include <WinSock2.h>
using socket_t = SOCKET;
#else
using socket_t = int;
#endif
