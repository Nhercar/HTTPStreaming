#include "stateless_handlers.h"
#include "socket_utils.h"
#include "http_parser.h"
#include "logger.h"

void HomeHandler::handle(socket_t client, const std::string&, std::stop_token) {
    HTTPResponse resp;
    resp.statusCode = 200;
    resp.statusMessage = "OK";
    resp.headers["Content-Type"] = "text/html; charset=UTF-8";
    resp.body = 
        "<html>"
        "<head><title>Servidor HTTP Streaming</title></head>"
        "<body>"
        "<h1>Bienvenido al servidor HTTP</h1>"
        "<p>Rutas disponibles:</p>"
        "<ul>"
        "<li><a href='/'>/ - Esta página (Home)</a></li>"
        "<li><a href='/stream'>/stream - Stream MJPEG (por implementar)</a></li>"
        "</ul>"
        "</body>"
        "</html>";
    
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