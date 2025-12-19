#include "socket_server.h"
#include "logger.h"
#include <WS2tcpip.h>
#include <thread>
#include <sstream>
#include <future>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

SocketServer::SocketServer(int port) 
    : port(port), serverSocket(INVALID_SOCKET), running(false) {
}

SocketServer::~SocketServer() {
    stop();
}

bool SocketServer::start() {
    // Inicializar Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        Logger::getInstance().error("WSAStartup falló: " + std::to_string(result));
        return false;
    }
    Logger::getInstance().info("Winsock inicializado correctamente");

    // Crear socket TCP
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        Logger::getInstance().error("Error creando socket: " + std::to_string(WSAGetLastError()));
        WSACleanup();
        return false;
    }
    Logger::getInstance().info("Socket creado correctamente");

    // Configurar dirección
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind
    result = bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result == SOCKET_ERROR) {
        Logger::getInstance().error("Error en bind: " + std::to_string(WSAGetLastError()));
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }
    Logger::getInstance().info("Socket enlazado al puerto " + std::to_string(port));

    // Listen
    result = listen(serverSocket, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        Logger::getInstance().error("Error en listen: " + std::to_string(WSAGetLastError()));
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }
    
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
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            if (running) {
                Logger::getInstance().error("Error al aceptar cliente: " + std::to_string(WSAGetLastError()));
            }
            continue;
        }
        if (static_cast<int>(getActiveClients ()) >= MAX_THREADS) {
            Logger::getInstance().info("Límite de clientes superado.");
            closesocket(clientSocket);
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

void SocketServer::handleClient(std::stop_token st, SOCKET clientSocket, const RequestHandlerStop& handler) {
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
            Logger::getInstance().info("Cliente cerró la conexión sin enviar datos");
        } else {
            Logger::getInstance().error("Error en recv: " + std::to_string(WSAGetLastError()));
        }

    // ... (maneja bytesReceived como antes)
    } else if (sel == 0) {
        // Timeout: no llegaron datos en 2 segundos
        Logger::getInstance().info("Timeout esperando datos del cliente en socket " + std::to_string(clientSocket));
         if (!socketClosed) {
            closesocket(clientSocket);
            Logger::getInstance().info("Conexion cerrada");
        }
    } else {
        // Error en select
        Logger::getInstance().error("Error en select: " + std::to_string(WSAGetLastError()) + " en socket " + std::to_string(clientSocket));
        if (!socketClosed) {
            closesocket(clientSocket);
            Logger::getInstance().info("Conexion cerrada");
        }
    }

    if (!socketClosed) {
        closesocket(clientSocket);
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

    WSACleanup();
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
    std::vector<SOCKET> toErase;
    std::vector<std::jthread> toJoin;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (auto &kv : clients) {
            if (kv.second.finished && kv.second.finished->load(std::memory_order_relaxed)) {
                toErase.push_back(kv.first);
            }
        }
        for (SOCKET s : toErase) {
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

void SocketServer::shutdownClient(SOCKET clientSocket) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = clients.find(clientSocket);
    if (it != clients.end()) {
        // Pedir parada cooperativa e interrumpir el socket
        it->second.thread.request_stop();
        shutdown(clientSocket, SD_BOTH);
        closesocket(clientSocket);
        Logger::getInstance().info("Cliente " + std::to_string((uintptr_t)clientSocket) + " marcado para cierre");
        // El hilo saldrá y el recolector lo limpiará
    }
}

void SocketServer::shutdownAllClients() {
    size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (auto &kv : clients) {
            SOCKET sock = kv.first;
            kv.second.thread.request_stop();
            shutdown(sock, SD_BOTH);
            closesocket(sock);
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