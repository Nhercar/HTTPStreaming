#include "mjpeg_handler.h"
#include "socket_utils.h"
#include "logger.h"
#include "../codec/frame_encoder.h"


void StreamHandler::addClient(socket_t client) {
    std::lock_guard<std::mutex> lock(mtx);
    clients.insert(client);
}

void StreamHandler::removeClient(socket_t client) {
    std::lock_guard<std::mutex> lock(mtx);
    clients.erase(client);
}

std::size_t StreamHandler::getClientCount() {
    std::lock_guard<std::mutex> lock(mtx);
    return clients.size();
}



void homeHandler(socket_t socket, const std::string&, std::stop_token st) {
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


void notFoundHandler(socket_t socket, const std::string&, std::stop_token st) {
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


void streamHandler(socket_t socket, const std::string&, std::stop_token st) {
    FrameEncoder encoder;
    const std::string imagePath = "C:/Users/nacho/OneDrive/Escritorio/nacho.jpg";

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
    
    while (!st.stop_requested()) {
        std::vector<uint8_t> jpegData;
        if (!encoder.readJpegFromDisk(imagePath, jpegData)){
            Logger::getInstance().error("No se pudo leer la imagen JPEG de disco");
            break;
        }

           std::ostringstream part;
           part << "--" << boundary << "\r\n"
               << "Content-Type: image/jpeg\r\n"
               << "Content-Length: " << jpegData.size() << "\r\n\r\n";

           std::string chunk = part.str();
           if (!sendAll(socket, reinterpret_cast<const uint8_t*>(chunk.data()), chunk.size())) break;
           if (!sendAll(socket, jpegData.data(), jpegData.size())) break;
           if (!sendAll(socket, reinterpret_cast<const uint8_t*>("\r\n"), 2)) break;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}
