#pragma once

#include <cstdint>
#include <string>

// Utility functions for reliable socket I/O
#include "socket_types.h"

bool sendAll(socket_t sock, const uint8_t* data, size_t len);
bool receiveAll(socket_t sock, uint8_t* data, size_t len);