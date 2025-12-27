#include "windows_server.h"
#include "../logger.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include <chrono>


#include "../ServerInterface/IServer.h"
#include "../streaming/stateless_handlers.h"

#pragma comment(lib, "ws2_32.lib")

WinSocketServer::WinSocketServer(int port)
: port_(port), serverSocket_(INVALID_SOCKET) {}

WinSocketServer::~WinSocketServer() {
    stop();
}

bool WinSocketServer::start() {
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (res != 0) {
        Logger::getInstance().error("WSAStartup failed: " + std::to_string(res));
        return false;
    }

    serverSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket_ == INVALID_SOCKET) {
        Logger::getInstance().error("socket() failed: " + std::to_string(WSAGetLastError()));
        WSACleanup();
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port_));
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        Logger::getInstance().error("bind() failed: " + std::to_string(WSAGetLastError()));
        closesocket(serverSocket_);
        WSACleanup();
        return false;
    }

    if (listen(serverSocket_, SOMAXCONN) == SOCKET_ERROR) {
        Logger::getInstance().error("listen() failed: " + std::to_string(WSAGetLastError()));
        closesocket(serverSocket_);
        WSACleanup();
        return false;
    }

    startMonitor();
    running_.store(true);
    Logger::getInstance().info("WinSocketServer listening on port " + std::to_string(port_));
    return true;
}

void WinSocketServer::run(RequestHandlerStop handler) {
    while (running_) {
        cleanupFinishedThreads();

        socket_t client = accept(serverSocket_, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            if (running_) Logger::getInstance().error("accept() failed: " + std::to_string(WSAGetLastError()));
            continue;
        }
        if (getActiveClients() >= MAX_CLIENTS) {
            MaxClientsReached maxClientesHandler;
            maxClientesHandler.handle(client, "", std::stop_token{});
            continue;
        }

        auto finished = std::make_shared<std::atomic<bool>>(false);
        std::jthread th([this, client, handler, finished](std::stop_token st){
            handleClient(st, client, handler);
            finished->store(true, std::memory_order_relaxed);
        });

        {
            std::lock_guard<std::mutex> lg(clientsMutex_);
            clients_.emplace(client, ClientRecord{ std::move(th), finished });
        }
    }
}

void WinSocketServer::handleClient(std::stop_token st, socket_t clientSocket, const RequestHandlerStop& handler) {
    bool socketClosed = false;
    char buffer[4096];
    if (st.stop_requested()) {
        shutdown(clientSocket, SD_BOTH);
        closesocket(clientSocket);
        socketClosed = true;
        return;
    }

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(clientSocket, &readfds);

    timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    int sel = select(0, &readfds, nullptr, nullptr, &timeout);
    if (sel > 0 && FD_ISSET(clientSocket, &readfds)) {
    // Hay datos listos para leer
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
       Logger::getInstance().info("handleClient: bytesReceived = " + std::to_string(bytesReceived) + " en socket " + std::to_string(clientSocket));
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string request(buffer, bytesReceived);
            handler(clientSocket, request, st);
        } else if (bytesReceived == 0) {
            Logger::getInstance().info("Cliente cerro la conexion sin enviar datos");
        } else {
            Logger::getInstance().error("Error en recv: " + std::to_string(WSAGetLastError()));
        }

    // ... (maneja bytesReceived como antes)
    } else if (sel == 0) {
        // Timeout: no llegaron datos en 2 segundos
        Logger::getInstance().info("Timeout esperando datos del cliente en socket " + std::to_string(clientSocket));

    } else {
        // Error en select
        Logger::getInstance().error("Error en select: " + std::to_string(WSAGetLastError()) + " en socket " + std::to_string(clientSocket));
    }

    if (!socketClosed) {
        closesocket(clientSocket);
        Logger::getInstance().info("Conexion cerrada en " + std::to_string(clientSocket));
    }
}

void WinSocketServer::stop() {
    if (!running_) return;
    running_.store(false);
    if (serverSocket_ != INVALID_SOCKET) {
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
    }
    stopMonitor();
    shutdownAllClients();
    cleanupFinishedThreads();
    joinAllThreads();
    WSACleanup();
    Logger::getInstance().info("WinSocketServer stopped");
}

size_t WinSocketServer::getActiveClients() {
    std::scoped_lock lk(clientsMutex_);
    return clients_.size();
}



void WinSocketServer::joinAllThreads() {
    std::vector<std::jthread> toJoin;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        toJoin.reserve(clients_.size());
        for (auto &kv : clients_) {
            toJoin.push_back(std::move(kv.second.thread));
        }
        clients_.clear();
    }
    // Al salir del scope, los jthread se unen automáticamente (RAII)
}

void WinSocketServer::cleanupFinishedThreads() {
    std::vector<socket_t> toErase;
    std::vector<std::jthread> toJoin;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto &kv : clients_) {
            if (kv.second.finished && kv.second.finished->load(std::memory_order_relaxed)) {
                toErase.push_back(kv.first);
            }
        }
        for (socket_t s : toErase) {
            auto it = clients_.find(s);
            if (it != clients_.end()) {
                toJoin.push_back(std::move(it->second.thread));
                clients_.erase(it);
            }
        }
    }
    // Al destruirse los jthread, se sincroniza con los hilos ya finalizados
}
 

void WinSocketServer::shutdownClient(socket_t clientSocket) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto it = clients_.find(clientSocket);
    if (it != clients_.end()) {
        // Pedir parada cooperativa e interrumpir el socket
        it->second.thread.request_stop();
        shutdown(clientSocket, SD_BOTH);
        closesocket(clientSocket);
        Logger::getInstance().info("Cliente " + std::to_string((uintptr_t)clientSocket) + " marcado para cierre");
        // El hilo saldrá y el recolector lo limpiará
    }
}

void WinSocketServer::shutdownAllClients() {
    size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto &kv : clients_) {
            socket_t sock = kv.first;
            kv.second.thread.request_stop();
            shutdown(sock, SD_BOTH);
            closesocket(sock);
            ++count;
        }
    }
    Logger::getInstance().info("Cerradas " + std::to_string(count) + " conexiones activas");
}


void WinSocketServer::startMonitor() {
    // jthread member in .h: std::jthread monitorThread_;
    monitorThread_ = std::jthread([this](std::stop_token st){
        using namespace std::chrono_literals;
        while (!st.stop_requested()) {
            cleanupFinishedThreads();
            std::this_thread::sleep_for(1s);
            Logger::getInstance().debug("Hay " + std::to_string(getActiveClients()) + " Clientes conectados");
        }
    });
}

void WinSocketServer::stopMonitor() {
    if (monitorThread_.joinable()) monitorThread_.request_stop();
}
