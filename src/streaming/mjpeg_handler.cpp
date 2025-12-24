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

void StreamHandler::stopAll(){
    std::lock_guard<std::mutex> lock(mtx);
    for (auto sock : clients) {
        closeSocket(sock);
    }
    clients.clear();
}

std::size_t StreamHandler::getClientCount() {
    std::lock_guard<std::mutex> lock(mtx);
    return clients.size();
}

void StreamHandler::handle(socket_t socket, const std::string&, std::stop_token st) {
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
