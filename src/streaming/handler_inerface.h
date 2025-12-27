#pragma once

#include <string>
#include <stop_token>
#include <vector>


#include "../HTTPServer/ServerInterface/IServer.h"

class IHandler {
public:

    virtual ~IHandler() = default;
    virtual void handle(socket_t client, const std::string& path, std::stop_token st) = 0; //Función  a ejecutar

    // Opcional: para gestión de clientes
    virtual void addClient(socket_t client) { (void)client; }
    virtual void removeClient(socket_t client) { (void)client; }
    virtual void stopAll() = 0;
    virtual std::size_t getClientCount() = 0;
    
};