#include "mjpeg_handler.h"
#include "socket_utils.h"
#include "logger.h"
#include "../codec/frame_encoder.h"
#include "../HTTPServer/ServerInterface/IServer.h"


StreamHandler::StreamHandler() {

     Logger::getInstance().info("Stream  Handler inicializado");
}


StreamHandler::~StreamHandler() {
    stopAll();
}

void StreamHandler::startProducer() {
    bool expected = false;
    if (!producing.compare_exchange_strong(expected, true)) return; // already producing
    frameProducer = std::jthread([this](std::stop_token st){ producerLoop(st); });
}

void StreamHandler::stopProducer() {
    bool expected = true;
    if (!producing.compare_exchange_strong(expected, false)) return; // not producing
    try {
        frameProducer.request_stop();
    } catch(...) {}
    if (frameProducer.joinable()) frameProducer.join();
    {
        std::lock_guard<std::mutex> lk(frameMtx);
        latestFrame.clear();
    }
}

void StreamHandler::producerLoop(std::stop_token st) {
    FrameEncoder encoder;
    const std::string imagePath = "../nacho.jpg";
    const auto frameDelay = std::chrono::milliseconds(100); // ~10 FPS by default

    while (!st.stop_requested() && producing.load(std::memory_order_relaxed)) {
        std::vector<uint8_t> jpegData;
        if (!encoder.readJpegFromDisk(imagePath, jpegData)){
            Logger::getInstance().error("Producer: no se pudo leer la imagen JPEG de disco");
            std::this_thread::sleep_for(frameDelay);
            continue;
        }

        {
            std::lock_guard<std::mutex> lk(frameMtx);
            latestFrame = std::move(jpegData);
        }
        frameCv.notify_all();

        // Sleep in small increments while checking stop
        auto slept = std::chrono::milliseconds(0);
        while (slept < frameDelay && !st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            slept += std::chrono::milliseconds(10);
        }
    }
}


// stopAll implemented below (cleans clients)

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
    stopProducer();
}

std::size_t StreamHandler::getClientCount() {
    std::lock_guard<std::mutex> lock(mtx);
    return clients.size();
}

void StreamHandler::handle(socket_t socket, const std::string&, std::stop_token st) {
    // Increment active clients and start producer if this is the first
    auto prev = activeClients.fetch_add(1, std::memory_order_acq_rel);
    if (prev == 0) startProducer();

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
        auto prev2 = activeClients.fetch_sub(1, std::memory_order_acq_rel);
        if (prev2 == 1) stopProducer();
        return;
    }

    const std::string boundary = "frame";

    while (!st.stop_requested()) {
        std::vector<uint8_t> frameCopy;
        {
            std::unique_lock<std::mutex> lk(frameMtx);
            if (latestFrame.empty()) {
                frameCv.wait_for(lk, std::chrono::milliseconds(500));
            }
            if (!latestFrame.empty()) frameCopy = latestFrame; // copy
        }

        if (frameCopy.empty()) {
            // nothing to send right now
            if (st.stop_requested()) break;
            continue;
        }

        std::ostringstream part;
        part << "--" << boundary << "\r\n"
             << "Content-Type: image/jpeg\r\n"
             << "Content-Length: " << frameCopy.size() << "\r\n\r\n";

        std::string chunk = part.str();
        if (!sendAll(socket, reinterpret_cast<const uint8_t*>(chunk.data()), chunk.size())) break;
        if (!sendAll(socket, frameCopy.data(), frameCopy.size())) break;
        if (!sendAll(socket, reinterpret_cast<const uint8_t*>("\r\n"), 2)) break;
    }

    auto prev3 = activeClients.fetch_sub(1, std::memory_order_acq_rel);
    if (prev3 == 1) stopProducer();
}
