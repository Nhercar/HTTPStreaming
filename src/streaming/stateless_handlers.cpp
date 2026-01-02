

#include "stateless_handlers.h"
#include "socket_utils.h"
#include "http_parser.h"
#include "logger.h"
#include <fstream>
#include "../HTTPServer/ServerInterface/IServer.h"

void HomeHandler::handle(socket_t client, const std::string&, std::stop_token) {
    HTTPResponse resp;
    
    // Intentamos abrir el archivo desde la carpeta assets
    // NOTA: La ruta es relativa a donde ejecutas el programa. 
    // En Docker (WORKDIR /app), "assets/homepage.htm" buscará en /app/assets/homepage.htm
    std::ifstream file("../assets/html/homepage.html");

    if (file.is_open()) {
        std::ostringstream ss;
        ss << file.rdbuf(); // Leemos todo el contenido del archivo
        resp.body = ss.str();
        
        resp.statusCode = 200;
        resp.statusMessage = "OK";
        resp.headers["Content-Type"] = "text/html; charset=UTF-8";
    } else {
        // Si no encuentra el archivo, mostramos un error y logueamos
        Logger::getInstance().error("No se pudo abrir assets/html/homepage.htm. Verifique la ruta.");
        resp.statusCode = 404;
        resp.statusMessage = "Not Found";
        resp.headers["Content-Type"] = "text/html; charset=UTF-8";
        resp.body = "<h1>Error: No se encuentra la interfaz (assets/html/homepage.html)</h1>";
    }
    
    std::string payload = buildHttpResponse(resp);
    sendAll(client, reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    closeSocket(client);
}

void NotFoundHandler::handle(socket_t client, const std::string&, std::stop_token) {
    std::ostringstream oss;
    oss << "ruta no encontrada";
    Logger::getInstance().debug(oss.str());
    HTTPResponse resp;
    resp.statusCode = 404;
    resp.statusMessage = "Not Found";
    resp.headers["Content-Type"] = "text/html; charset=UTF-8";
    resp.headers["Connection"] = "close";
    resp.body = 
        "<html>"
        "<head><title>404 - Not Found</title></head>"
        "<body>"
        "<h1>404 - Página no encontrada</h1>"
        "<p><a href='/'>Volver al inicio</a></p>"
        "</body>"
        "</html>";
    
    std::string payload = buildHttpResponse(resp);
    sendAll(client, reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    closeSocket(client);
}

void MaxClientsReached::handle(socket_t client, const std::string&, std::stop_token) {

    Logger::getInstance().info("Número máximo de clientes alcanzado. Devolviendo info");
    HTTPResponse resp;
    resp.statusCode = 404;
    resp.statusMessage = "Not Found";
    resp.headers["Content-Type"] = "text/html; charset=UTF-8";
    resp.headers["Connection"] = "close";
    resp.body = 
        "<html>"
        "<head><title>404 -Lo  sentimos</title></head>"
        "<body>"
        "<h1>404 -  Máximo número de clientes  alcanzado. Inténtelo más tarde</h1>"
        "<p><a href='/'>Volver al inicio</a></p>"
        "</body>"
        "</html>";
    
    std::string payload = buildHttpResponse(resp);
    sendAll(client, reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    closeSocket(client);
}


