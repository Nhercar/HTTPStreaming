#include "http_parser.h"
#include "logger.h"
#include "router.h"
#include "mjpeg_handler.h"
#include "stateless_handlers.h"

#include "ServerInterface/IServer.h"
#include <signal.h>


#include <string>



int main() {
    Logger::getInstance().info("Iniciando servidor HTTP...");

    // Crear router y registrar rutas
    Router router;
    router.registerRoute("GET", "/", std::make_shared<HomeHandler>());
    router.registerRoute("GET", "/stream", std::make_shared<StreamHandler>());
    router.setDefaultRoute(std::make_shared<NotFoundHandler>());
    router.maxClientsRoute(std::make_shared<MaxClientsReached>());

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
    std::unique_ptr<IServer> server = ServerFactory::create(8080);

    if (!server->start()) {
        Logger::getInstance().error("Fallo al iniciar el servidor");
        return 1;
    }

    // Evitar que un send() a un socket cerrado genere SIGPIPE y termine el proceso
    signal(SIGPIPE, SIG_IGN);

    // Correr servidor
    server->run(routedHandler);

    return 0;
}