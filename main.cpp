#include "socket_server.h"
#include "http_parser.h"
#include "http_response.h"
#include "logger.h"
#include <WinSock2.h>
#include <string>

// Handler con cancelación: procesa request y escribe respuesta al socket
static void basicHandler(SOCKET client, const std::string& rawRequest, std::stop_token st) {
    HTTPRequest req = parseHTTPRequest(rawRequest);
    Logger::getInstance().info("Metodo: " + req.method + ", Path: " + req.path);

    HTTPResponse resp;
    resp.statusCode = 200;
    resp.statusMessage = "OK";
    resp.headers["Content-Type"] = "text/html; charset=UTF-8";
    resp.body = "<html><body><h1>¡Hola desde C++ Multihilo!</h1></body></html>";

    const std::string payload = buildHttpResponse(resp);

    size_t total = 0;
    while (total < payload.size() && !st.stop_requested()) {
        int sent = send(client, payload.data() + total, static_cast<int>(payload.size() - total), 0);
        if (sent == SOCKET_ERROR) {
            Logger::getInstance().error("Error en send: " + std::to_string(WSAGetLastError()));
            break;
        }
        total += static_cast<size_t>(sent);
    }
}

int main() {
    Logger::getInstance().info("Iniciando servidor HTTP...");

    SocketServer server(8080);

    if (!server.start()) {
        Logger::getInstance().error("Fallo al iniciar el servidor");
        return 1;
    }

    // Correr servidor con handler con stop_token
    server.run(basicHandler);

    return 0;
}