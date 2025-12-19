#ifndef SOCKET_SERVER_H
#define SOCKET_SERVER_H

#include <WinSock2.h>
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
using RequestHandler = std::function<void(SOCKET, const std::string&)>;
using RequestHandlerStop = std::function<void(SOCKET, const std::string&, std::stop_token)>;

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
    SOCKET serverSocket;
    std::atomic<bool> running{false};

    struct ClientRecord {
        std::jthread thread;                                   // Hilo del cliente
        std::shared_ptr<std::atomic<bool>> finished;            // Marca de finalización cooperativa
    };

    std::jthread monitorThread;

    // Mapa socket -> hilo/estado asociado
    std::unordered_map<SOCKET, ClientRecord> clients;
    std::mutex clientsMutex;     // Protege 'clients'
    
    void cleanupFinishedThreads();
    void joinAllThreads();
    void closeAllClientSockets(); // Compatibilidad: llama a shutdownAllClients()

    // Worker del cliente con cancelación cooperativa
    void handleClient(std::stop_token st, SOCKET clientSocket, const RequestHandlerStop& handler);
};

#endif // SOCKET_SERVER_H
