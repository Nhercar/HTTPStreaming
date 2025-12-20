#pragma once
#include <functional>
#include <string>
#include <stop_token>
#include "socket_types.h"

using RequestHandlerStop = std::function<void(socket_t, const std::string&, std::stop_token)>;

class ISocketServer {
public:
    virtual ~ISocketServer() = default;
    virtual bool start() = 0;
    virtual void run(RequestHandlerStop handler) = 0;
    virtual void stop() = 0;
    virtual void shutdownClient(socket_t clientSocket) = 0;
    virtual void shutdownAllClients() = 0;
    virtual size_t getActiveClients() = 0;
    virtual void startMonitor() = 0;
    virtual void stopMonitor() = 0;
};
