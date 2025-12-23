#include "socket_server.h"
#include "http_parser.h"
#include "logger.h"
#include "router.h"
#include "mjpeg_handler.h"


#include <string>



int main() {
    Logger::getInstance().info("Iniciando servidor HTTP...");

    // Crear router y registrar rutas
    Router router;
    router.registerRoute("GET", "/", homeHandler);
    router.registerRoute("GET", "/stream", streamHandler);
    router.setDefaultRoute(notFoundHandler);

    // Handler que parsea el request y delega al router
    auto routedHandler = [&router](socket_t socket, const std::string& rawRequest, std::stop_token st) {
        // Parsear request
        HTTPRequest req = parseHTTPRequest(rawRequest);
        Logger::getInstance().info("Metodo: " + req.method + ", Path: " + req.path);
        
        // Despachar al router
        if (!router.route(req.method, req.path, socket, req.body, st)) {
            Logger::getInstance().error("Ruta no manejada (sin default): " + req.method + " " + req.path);
        }
    };

    // Crear servidor
    SocketServer server(8080);

    if (!server.start()) {
        Logger::getInstance().error("Fallo al iniciar el servidor");
        return 1;
    }

    // Correr servidor
    server.run(routedHandler);

    return 0;
}