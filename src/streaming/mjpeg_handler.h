#pragma once

#include <stop_token>
#include <WinSock2.h>
#include <string>
#include <chrono>
#include <thread>
#include <sstream>

#include "logger.h"
#include "http_parser.h"
#include "http_response.h"


void homeHandler(SOCKET socket, const std::string&, std::stop_token st);

void notFoundHandler(SOCKET socket, const std::string&, std::stop_token st);

void streamHandler(SOCKET socket, const std::string&, std::stop_token st);


