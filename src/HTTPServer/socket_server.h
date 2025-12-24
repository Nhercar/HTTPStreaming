#pragma once

#ifdef _WIN32
    #include <winsock2.h>
    using socket_t = SOCKET;
    #define INVALID_SOCKET_T INVALID_SOCKET
    #define SOCKET_ERROR_T SOCKET_ERROR
    inline void closeSocket(socket_t sock) { closesocket(sock); }
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    using socket_t = int;
    #define INVALID_SOCKET_T -1
    #define SOCKET_ERROR_T -1
    inline void closeSocket(socket_t sock) { close(sock); }
#endif


#include <functional>
#include <string>
#include <atomic>
#include <vector>
#include <mutex>
#include <thread>
#include <future>
#include <unordered_map>
#include <stop_token>

// Callback para manejar requests HTTP
// Recibe el socket para permitir streaming continuo
using RequestHandlerStop = std::function<void(SOCKET, const std::string&, std::stop_token)>;

struct ClientRecord {
    std::jthread thread;                                   // Hilo del cliente
    std::shared_ptr<std::atomic<bool>> finished;            // Marca de finalización cooperativa
};


// Abstracción de servidor TCP con multihilo
class SocketServer {
public:
    SocketServer(int port);
    ~SocketServer();
    
    bool start();                 // Inicializa y escucha en el puerto
    void run(RequestHandlerStop handler);     // Variante con stop_token
    void stop();                  // Limpieza y detención

    // Control fino de clientes
    void shutdownClient(SOCKET clientSocket);   // Cerrar un cliente concreto desde el servidor
    void shutdownAllClients();                  // Cerrar todos los clientes desde el servidor

    size_t getActiveClients();

    void startMonitor();

    void stopMonitor();

private:
    static const int MAX_THREADS = 5;


    int port;
    socket_t serverSocket;
    std::atomic<bool> running{false};

    std::jthread monitorThread;

    // Mapa socket -> hilo/estado asociado
    std::unordered_map<socket_t, ClientRecord> clients;
    std::mutex clientsMutex;     // Protege 'clients'
    
    void cleanupFinishedThreads();
    void joinAllThreads();
    void closeAllClientSockets(); // Compatibilidad: llama a shutdownAllClients()

    // Worker del cliente con cancelación cooperativa
    void handleClient(std::stop_token st, SOCKET clientSocket, const RequestHandlerStop& handler);
};
