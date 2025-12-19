#include "mjpeg_handler.h"
#include "socket_utils.h"
#include "logger.h"




void homeHandler(SOCKET socket, const std::string&, std::stop_token st) {
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
    sendAll(socket, reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
}


void notFoundHandler(SOCKET socket, const std::string&, std::stop_token st) {
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
    sendAll(socket, reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
}


void streamHandler(SOCKET socket, const std::string&, std::stop_token st) {
    HTTPResponse resp;
    resp.statusCode = 200;
    resp.statusMessage = "OK";
    resp.headers["Content-Type"] = "multipart/x-mixed-replace; boundary=frame";
    resp.headers["Connection"] = "keep-alive";
    resp.headers["Cache-Control"] = "no-cache";
    resp.headers["Pragma"] = "no-cache";
    resp.body.clear();

    std::string initial = buildHttpResponse(resp);
    if(!sendAll(socket, reinterpret_cast<const uint8_t*>(initial.data()), initial.size())) return;

    const std::string boundary = "frame";
    int frameId = 0;
    
    while (!st.stop_requested()) {
        std::string body = "dummy frame " + std::to_string(frameId++) + "\n"; // cambia a JPEG real después

        std::ostringstream part;
        part << "--" << boundary << "\r\n"
             << "Content-Type: text/plain\r\n"
             << "Content-Length: " << body.size() << "\r\n\r\n"
             << body << "\r\n";

        std::string chunk = part.str();
        if (!sendAll(socket, reinterpret_cast<const uint8_t*>(chunk.data()), chunk.size())) break;

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}
