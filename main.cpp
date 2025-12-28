#include "http_parser.h"
#include "logger.h"
#include "router.h"
#include "mjpeg_handler.h"
#include "stateless_handlers.h"

#include "ServerInterface/IServer.h"
#include <signal.h>


#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>


static volatile sig_atomic_t g_keep_running = 1;

static void handle_signal(int) { g_keep_running = 0; }


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

    // Registrar manejadores de señal para salida limpia
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGPIPE, SIG_IGN);

    // Bucle principal: reinicia el servidor si se sale, hasta que se reciba SIGINT/SIGTERM
    while (g_keep_running) {
        Logger::getInstance().info("Iniciando instancia del servidor...");

        std::unique_ptr<IServer> server = ServerFactory::create(8080);

        if (!server) {
            Logger::getInstance().error("Factory devolvió servidor nulo");
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        if (!server->start()) {
            Logger::getInstance().error("Fallo al iniciar el servidor; reintentando en 5s");
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        // Ejecutar hasta que `run` finalice o se pida terminar
        server->run(routedHandler);

        // Si se ha solicitado salida, detener y salir del bucle
        if (!g_keep_running) {
            Logger::getInstance().info("Se solicitó salida; deteniendo servidor...");
            server->stop();
            break;
        }

        Logger::getInstance().info("El servidor ha finalizado inesperadamente; reiniciando en 3s...");
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    Logger::getInstance().info("Proceso principal finalizado");
    return 0;
}