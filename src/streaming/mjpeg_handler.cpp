#include "mjpeg_handler.h"
#include "socket_utils.h"
#include "logger.h"
#include "../codec/frame_encoder.h"


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


// MJPEGHandler implementation
MJPEGHandler::MJPEGHandler(int deviceIndex)
    : deviceIndex_(deviceIndex) {
}

void MJPEGHandler::operator()(socket_t socket, const std::string& /*rawRequest*/, std::stop_token st) {
    // Stream in the current thread (server's per-client thread) so the server
    // keeps the socket open while streaming. The provided stop_token is used
    // for cooperative shutdown. Manage webcam lifecycle: open on first client,
    // release when no clients remain.
    activeClients_.fetch_add(1, std::memory_order_relaxed);

    // Lazy-open webcam if needed
    if (!webcam_ || !webcam_->isOpen()) {
        webcam_ = std::make_shared<Webcam>(deviceIndex_);
        if (!webcam_->isOpen()) {
            Logger::getInstance().error("No se pudo abrir la webcam en deviceIndex " + std::to_string(deviceIndex_));
            activeClients_.fetch_sub(1, std::memory_order_relaxed);
            return;
        }
    }
    // Send initial response headers
    HTTPResponse resp;
    resp.statusCode = 200;
    resp.statusMessage = "OK";
    resp.headers["Content-Type"] = "multipart/x-mixed-replace; boundary=frame";
    resp.headers["Connection"] = "keep-alive";
    resp.headers["Cache-Control"] = "no-cache";
    resp.headers["Pragma"] = "no-cache";
    resp.body.clear();

    std::string initial = buildHttpResponse(resp);
    if(!sendAll(socket, reinterpret_cast<const uint8_t*>(initial.data()), initial.size())) {
        Logger::getInstance().info("Failed to send initial headers to socket " + std::to_string((uintptr_t)socket));
        return;
    }
    Logger::getInstance().info("Started MJPEG stream on socket " + std::to_string((uintptr_t)socket));

    const std::string boundary = "frame";

    if (!webcam_ || !webcam_->isOpen()) {
        Logger::getInstance().error("Webcam no disponible para streaming");
        return;
    }

    while (!st.stop_requested()) {
        cv::Mat frame;
        if (!webcam_->read(frame)) {
            Logger::getInstance().error("No se pudo capturar frame desde la webcam");
            break;
        }

        std::vector<uint8_t> jpegData;
        if (!encoder_.encode(frame, jpegData)) {
            Logger::getInstance().error("Error codificando frame JPEG");
            break;
        }

        std::ostringstream part;
        part << "--" << boundary << "\r\n"
             << "Content-Type: image/jpeg\r\n"
             << "Content-Length: " << jpegData.size() << "\r\n\r\n";

        std::string chunk = part.str();
        if (!sendAll(socket, reinterpret_cast<const uint8_t*>(chunk.data()), chunk.size())) {
            Logger::getInstance().info("Failed to send chunk header on socket " + std::to_string((uintptr_t)socket));
            break;
        }
        if (!sendAll(socket, jpegData.data(), jpegData.size())) {
            Logger::getInstance().info("Failed to send jpeg data on socket " + std::to_string((uintptr_t)socket));
            break;
        }
        if (!sendAll(socket, reinterpret_cast<const uint8_t*>("\r\n"), 2)) {
            Logger::getInstance().info("Failed to send chunk terminator on socket " + std::to_string((uintptr_t)socket));
            break;
        }
        Logger::getInstance().info("Sent frame (" + std::to_string(jpegData.size()) + " bytes) to socket " + std::to_string((uintptr_t)socket));

        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 15));
    }

    // Client disconnected; decrement and if zero, free webcam
    size_t prev = activeClients_.fetch_sub(1, std::memory_order_relaxed);
    if (prev == 1) {
        // last client gone
        webcam_.reset();
        Logger::getInstance().info("Webcam liberada (no hay clientes activos)");
    }
}

void MJPEGHandler::cleanupFinishedThreads() {
    // No-op: client threads are managed by the server now.
}

void MJPEGHandler::stopAll() {
    // Request stop: handlers don't directly control server threads.
    webcam_.reset();
    activeClients_.store(0, std::memory_order_relaxed);
    Logger::getInstance().info("MJPEGHandler stopAll: webcam released and client count reset");
}

std::size_t MJPEGHandler::getClientCount() {
    return activeClients_.load(std::memory_order_relaxed);
}
