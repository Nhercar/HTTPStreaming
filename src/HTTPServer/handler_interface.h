#pragma once
#include <stop_token>
#include "socket_types.h"
#include <string>
#include <cstddef>

class IHandler {
public:
    virtual ~IHandler() = default;
    virtual void operator()(socket_t socket, const std::string& rawRequest, std::stop_token st) = 0;
    virtual void stopAll() = 0;
    virtual std::size_t getClientCount() = 0;
};
