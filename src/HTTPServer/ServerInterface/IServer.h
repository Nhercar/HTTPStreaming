#pragma once
#include <string>
#include <functional>
#include <stop_token>
#include <memory>

#pragma once

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h> // <--- Añadido para funcionalidades extra

    using socket_t = SOCKET;
    #define INVALID_SOCKET_T INVALID_SOCKET
    #define SOCKET_ERROR_T SOCKET_ERROR
    // En Windows no existen estas banderas de Linux. Las definimos a 0.
    #ifndef MSG_NOSIGNAL
        #define MSG_NOSIGNAL 0
    #endif
    #ifndef MSG_DONTWAIT
        #define MSG_DONTWAIT 0
    #endif

    inline void closeSocket(socket_t sock) { closesocket(sock); }
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>  // <--- Necesario para IPPROTO_TCP
    #include <netinet/tcp.h> // <--- Necesario para TCP_NODELAY
    #include <unistd.h>      // Necesario para close()
    
    using socket_t = int;
    #define INVALID_SOCKET_T -1
    #define SOCKET_ERROR_T -1
    inline void closeSocket(socket_t sock) { close(sock); }
#endif

using RequestHandlerStop = std::function<void(socket_t, const std::string&, std::stop_token)>;

class IServer {
public:
    virtual ~IServer() = default;
    virtual bool start() = 0;
    virtual void run(RequestHandlerStop handler) = 0;
    virtual void stop() = 0;
    virtual size_t getActiveClients() = 0;
};

// The Factory: declaration only. Implementation is in server_factory.cpp
class ServerFactory {
public:
    static std::unique_ptr<IServer> create(int port);
};