#pragma once
#include "socket_interface.h"
#include "socket_server.h"
#include "logger.h"
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

class SocketServerWin : public ISocketServer {
public:
    explicit SocketServerWin(int port) : impl(port) {}
    bool start() override {
#if defined(_WIN32) || defined(_WIN64)
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (result != 0) {
            Logger::getInstance().error("WSAStartup falló: " + std::to_string(result));
            return false;
        }
#endif
        return impl.start();
    }
    void run(RequestHandlerStop handler) override { impl.run(handler); }
    void stop() override { impl.stop();
#if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
#endif
    }
    void shutdownClient(socket_t clientSocket) override { impl.shutdownClient(clientSocket); }
    void shutdownAllClients() override { impl.shutdownAllClients(); }
    size_t getActiveClients() override { return impl.getActiveClients(); }
    void startMonitor() override { impl.startMonitor(); }
    void stopMonitor() override { impl.stopMonitor(); }

private:
    SocketServer impl;
};
