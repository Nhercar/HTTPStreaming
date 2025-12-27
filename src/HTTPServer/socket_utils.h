#pragma once

#include <cstdint>
#include <string>
#include "../HTTPServer/ServerInterface/IServer.h"


bool sendAll(socket_t sock, const uint8_t* data, size_t len);
bool receiveAll(socket_t sock, uint8_t* data, size_t len);