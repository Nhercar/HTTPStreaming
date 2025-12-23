#include "socket_server.h"
#include "logger.h"
#include "socket_types.h"

#include <thread>
#include <sstream>
#include <future>
#include <unordered_map>
#include <algorithm>
#include <chrono>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

SocketServer::SocketServer(int port)
    : port(port), running(false) {
#if defined(_WIN32) || defined(_WIN64)
    serverSocket = INVALID_SOCKET;
#else
    serverSocket = -1;
#endif
}

SocketServer::~SocketServer() {
    stop();
}

bool SocketServer::start() {
    // Socket initialization is OS-implementation responsibility
    int result = 0;
    // Crear socket TCP
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#if defined(_WIN32) || defined(_WIN64)
    if (serverSocket == INVALID_SOCKET) {
        Logger::getInstance().error("Error creando socket: " + std::to_string(WSAGetLastError()));
        WSACleanup();
        return false;
    }
#else
    if (serverSocket < 0) {
        Logger::getInstance().error("Error creando socket");
        return false;
    }
#endif
    Logger::getInstance().info("Socket creado correctamente");

    // Configurar dirección
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind
    result = bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
#if defined(_WIN32) || defined(_WIN64)
    if (result == SOCKET_ERROR) {
        Logger::getInstance().error("Error en bind: " + std::to_string(WSAGetLastError()));
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }
#else
    if (result < 0) {
        Logger::getInstance().error("Error en bind");
        close(serverSocket);
        return false;
    }
#endif
    Logger::getInstance().info("Socket enlazado al puerto " + std::to_string(port));

    // Listen
    result = listen(serverSocket, SOMAXCONN);
#if defined(_WIN32) || defined(_WIN64)
    if (result == SOCKET_ERROR) {
        Logger::getInstance().error("Error en listen: " + std::to_string(WSAGetLastError()));
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }
#else
    if (result < 0) {
        Logger::getInstance().error("Error en listen");
        close(serverSocket);
        return false;
    }
#endif
    
    std::ostringstream oss;
    oss << "Servidor escuchando en http://localhost:" << port;
    Logger::getInstance().info(oss.str());

    startMonitor();
    
    running = true;
    return true;
}


void SocketServer::run(RequestHandlerStop handler) {
    while (running) {
        // Limpiar tareas terminadas antes de aceptar nuevo cliente
        cleanupFinishedThreads();
        // Aceptar conexión
        socket_t clientSocket = accept(serverSocket, nullptr, nullptr);
#if defined(_WIN32) || defined(_WIN64)
        if (clientSocket == INVALID_SOCKET) {
            if (running) {
                Logger::getInstance().error("Error al aceptar cliente: " + std::to_string(WSAGetLastError()));
            }
            continue;
        }
#else
        if (clientSocket < 0) {
            if (running) {
                Logger::getInstance().error("Error al aceptar cliente");
            }
            continue;
        }
#endif
        if (static_cast<int>(getActiveClients ()) >= MAX_THREADS) {
            Logger::getInstance().info("Límite de clientes superado.");
            
#if defined(_WIN32) || defined(_WIN64)
            closesocket(clientSocket);
#else
            close(clientSocket);
#endif
            continue;
        }
        
        Logger::getInstance().info("Cliente conectado");

        // Crear estado de finalización y lanzar jthread asociado al socket
        auto finished = std::make_shared<std::atomic<bool>>(false);
        std::jthread th([this, clientSocket, handler, finished](std::stop_token st) {
            handleClient(st, clientSocket, handler);
            finished->store(true, std::memory_order_relaxed);
        });

        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.emplace(clientSocket, ClientRecord{ std::move(th), finished });
            
        }

    }
}

void SocketServer::handleClient(std::stop_token st, socket_t clientSocket, const RequestHandlerStop& handler) {
    bool socketClosed = false;
    char buffer[4096];
    if (st.stop_requested()) {
#if defined(_WIN32) || defined(_WIN64)
    shutdown(clientSocket, SD_BOTH);
    closesocket(clientSocket);
#else
    shutdown(clientSocket, SHUT_RDWR);
    close(clientSocket);
#endif
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
            Logger::getInstance().info("Cliente cerró la conexión sin enviar datos");
        } else {
    #if defined(_WIN32) || defined(_WIN64)
            Logger::getInstance().error("Error en recv: " + std::to_string(WSAGetLastError()));
    #else
            Logger::getInstance().error("Error en recv");
    #endif
        }

    // ... (maneja bytesReceived como antes)
    } else if (sel == 0) {
        // Timeout: no llegaron datos en 2 segundos
        Logger::getInstance().info("Timeout esperando datos del cliente en socket " + std::to_string(clientSocket));
         if (!socketClosed) {
#if defined(_WIN32) || defined(_WIN64)
            closesocket(clientSocket);
#else
            close(clientSocket);
#endif
            Logger::getInstance().info("Conexion cerrada");
        }
    } else {
        // Error en select
    #if defined(_WIN32) || defined(_WIN64)
        Logger::getInstance().error("Error en select: " + std::to_string(WSAGetLastError()) + " en socket " + std::to_string(clientSocket));
    #else
        Logger::getInstance().error("Error en select en socket " + std::to_string(clientSocket));
    #endif
        if (!socketClosed) {
#if defined(_WIN32) || defined(_WIN64)
            closesocket(clientSocket);
#else
            close(clientSocket);
#endif
            Logger::getInstance().info("Conexion cerrada");
        }
    }

    if (!socketClosed) {
#if defined(_WIN32) || defined(_WIN64)
        closesocket(clientSocket);
#else
        close(clientSocket);
#endif
        Logger::getInstance().info("Conexion cerrada");
    }

 
}

void SocketServer::stop() {
    if(!running) return;
    running = false;

    if(serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }

    stopMonitor();
    // Forzar cierre de todas las conexiones activas
    shutdownAllClients();

    cleanupFinishedThreads();

    joinAllThreads();

#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    Logger::getInstance().info("Servidor detenido");
}

void SocketServer::joinAllThreads() {
    std::vector<std::jthread> toJoin;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        toJoin.reserve(clients.size());
        for (auto &kv : clients) {
            toJoin.push_back(std::move(kv.second.thread));
        }
        clients.clear();
    }
    // Al salir del scope, los jthread se unen automáticamente (RAII)
}

void SocketServer::cleanupFinishedThreads() {
    std::vector<socket_t> toErase;
    std::vector<std::jthread> toJoin;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (auto &kv : clients) {
            if (kv.second.finished && kv.second.finished->load(std::memory_order_relaxed)) {
                toErase.push_back(kv.first);
            }
        }
        for (socket_t s : toErase) {
            auto it = clients.find(s);
            if (it != clients.end()) {
                toJoin.push_back(std::move(it->second.thread));
                clients.erase(it);
            }
        }
    }
    // Al destruirse los jthread, se sincroniza con los hilos ya finalizados
}
 
void SocketServer::closeAllClientSockets() {
    shutdownAllClients();
}

void SocketServer::shutdownClient(socket_t clientSocket) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = clients.find(clientSocket);
    if (it != clients.end()) {
        // Pedir parada cooperativa e interrumpir el socket
        it->second.thread.request_stop();
#if defined(_WIN32) || defined(_WIN64)
        shutdown(clientSocket, SD_BOTH);
        closesocket(clientSocket);
#else
        shutdown(clientSocket, SHUT_RDWR);
        close(clientSocket);
#endif
        Logger::getInstance().info("Cliente " + std::to_string((uintptr_t)clientSocket) + " marcado para cierre");
        // El hilo saldrá y el recolector lo limpiará
    }
}

void SocketServer::shutdownAllClients() {
    size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (auto &kv : clients) {
            socket_t sock = kv.first;
            kv.second.thread.request_stop();
#if defined(_WIN32) || defined(_WIN64)
            shutdown(sock, SD_BOTH);
            closesocket(sock);
#else
            shutdown(sock, SHUT_RDWR);
            close(sock);
#endif
            ++count;
        }
    }
    Logger::getInstance().info("Cerradas " + std::to_string(count) + " conexiones activas");
}

size_t SocketServer::getActiveClients(){  
    std::scoped_lock lock(clientsMutex);    
    return clients.size();;
}


void SocketServer::startMonitor() {
    // jthread member in .h: std::jthread monitorThread;
    monitorThread = std::jthread([this](std::stop_token st){
        using namespace std::chrono_literals;
        while (!st.stop_requested()) {
            cleanupFinishedThreads();
            Logger::getInstance().info("Monitor: " + std::to_string(getActiveClients()) + " clientes activos");
            std::this_thread::sleep_for(1s);
        }
    });
}

void SocketServer::stopMonitor() {
    if (monitorThread.joinable()) monitorThread.request_stop();
}