#pragma once

#include <winsock2.h>
#include <cstdint>
#include <string>

// Utility functions for reliable socket I/O
bool sendAll(SOCKET sock, const uint8_t* data, size_t len);
bool receiveAll(SOCKET sock, uint8_t* data, size_t len);