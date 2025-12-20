#pragma once
#include "socket_interface.h"
#include "socket_server.h"

class SocketServerPosix : public ISocketServer {
public:
    explicit SocketServerPosix(int port) : impl(port) {}
    bool start() override { return impl.start(); }
    void run(RequestHandlerStop handler) override { impl.run(handler); }
    void stop() override { impl.stop(); }
    void shutdownClient(socket_t clientSocket) override { impl.shutdownClient(clientSocket); }
    void shutdownAllClients() override { impl.shutdownAllClients(); }
    size_t getActiveClients() override { return impl.getActiveClients(); }
    void startMonitor() override { impl.startMonitor(); }
    void stopMonitor() override { impl.stopMonitor(); }

private:
    SocketServer impl;
};
