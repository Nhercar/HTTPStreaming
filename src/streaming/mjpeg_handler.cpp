#include "mjpeg_handler.h"
#include "socket_utils.h"
#include "logger.h"
#include "../codec/frame_encoder.h"
#include "../HTTPServer/ServerInterface/IServer.h"
#include "../camera/webcam.h"



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
    // Increase JPEG quality to 90
    FrameEncoder encoder(90);
    const std::string imagePath = "../nacho.jpg";
    // Target ~25 FPS
    const auto frameDelay = std::chrono::milliseconds(40);

    // Try to open webcam devices /dev/video0..3 automatically
    int openedIndex = -1;
    Webcam cam(0);
    if (cam.isOpen()) {
        openedIndex = 0;
    } else {
        for (int i = 1; i <= 3; ++i) {
            Webcam tryCam(i);
            if (tryCam.isOpen()) {
                cam = std::move(tryCam);
                openedIndex = i;
                break;
            }
        }
    }

    if (openedIndex == -1) {
        Logger::getInstance().info("Producer: no webcam device opened, falling back to disk image");
    } else {
        cam.setResolution(1280, 720);
        cam.setFPS(25);
        Logger::getInstance().info("Producer: webcam opened on device " + std::to_string(openedIndex));
    }

    while (!st.stop_requested() && producing.load(std::memory_order_relaxed)) {
        std::vector<uint8_t> jpegData;

        if (cam.isOpen()) {
            cv::Mat frame;
            if (!cam.read(frame) || frame.empty()) {
                Logger::getInstance().error("Producer: failed to read frame from webcam");
                // fallback to disk read for this iteration
                if (!encoder.readJpegFromDisk(imagePath, jpegData)){
                    Logger::getInstance().error("Producer: fallback also failed to read jpeg from disk");
                    std::this_thread::sleep_for(frameDelay);
                    continue;
                }
            } else {
                if (!encoder.encode(frame, jpegData)) {
                    Logger::getInstance().error("Producer: failed to encode frame to JPEG");
                    std::this_thread::sleep_for(frameDelay);
                    continue;
                }
            }
        } else {
            if (!encoder.readJpegFromDisk(imagePath, jpegData)){
                Logger::getInstance().error("Producer: no se pudo leer la imagen JPEG de disco");
                std::this_thread::sleep_for(frameDelay);
                continue;
            }
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
    // 1. OPTIMIZACIÓN: Quitar el LAG (Nagle Algorithm)
    int flag = 1;
    setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

    // Gestión de productores
    auto prev = activeClients.fetch_add(1, std::memory_order_acq_rel);
    if (prev == 0) startProducer();

    // 2. PREPARAR CABECERAS
    HTTPResponse resp;
    resp.statusCode = 200;
    resp.statusMessage = "OK";
    resp.headers["Content-Type"] = "multipart/x-mixed-replace; boundary=frame";
    resp.headers["Cache-Control"] = "no-cache, no-store, must-revalidate"; // corregida la coma que faltaba
    resp.headers["Pragma"] = "no-cache";
    resp.headers["Expires"] = "0";
    resp.headers["X-Accel-Buffering"] = "no";
    resp.body.clear();

    std::string initial = buildHttpResponse(resp);

    // 3. ENVIAR CABECERAS (Corregido el check < 0 y el cast a char*)
    if (send(socket, initial.data(), initial.size(), MSG_NOSIGNAL) < 0) {
        auto prev2 = activeClients.fetch_sub(1, std::memory_order_acq_rel);
        if (prev2 == 1) stopProducer();
        return;
    }

    const std::string boundary = "frame";

    while (!st.stop_requested()) {
        
        // 4. DETECCIÓN DE DESCONEXIÓN (El fix de los 30s)
        char peekBuf;
        int peekResult = recv(socket, &peekBuf, 1, MSG_PEEK | MSG_DONTWAIT);
        if (peekResult == 0) {
            break; // Cliente cerró (FIN)
        }
        
        // Obtener frame
        std::vector<uint8_t> frameCopy;
        {
            std::unique_lock<std::mutex> lk(frameMtx);
            if (latestFrame.empty()) {
                frameCv.wait_for(lk, std::chrono::milliseconds(500));
            }
            if (!latestFrame.empty()) frameCopy = latestFrame;
        }

        if (frameCopy.empty()) {
            if (st.stop_requested()) break;
            continue;
        }

        // Construir cabecera del frame
        std::ostringstream part;
        part << "--" << boundary << "\r\n"
             << "Content-Type: image/jpeg\r\n"
             << "Content-Length: " << frameCopy.size() << "\r\n\r\n";

        // 5. CORRECCIÓN PRINCIPAL: Crear la variable 'head'
        std::string head = part.str(); 

        // Enviar cabecera del frame
        if (send(socket, head.data(), head.size(), MSG_NOSIGNAL) < 0) break;
        
        // Enviar imagen (Casting a char* para compatibilidad)
        if (send(socket, (const char*)frameCopy.data(), frameCopy.size(), MSG_NOSIGNAL) < 0) break;
        
        // Enviar salto de línea
        if (send(socket, "\r\n", 2, MSG_NOSIGNAL) < 0) break;
        
        // Control de FPS
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    // Limpieza final
    auto prev3 = activeClients.fetch_sub(1, std::memory_order_acq_rel);
    if (prev3 == 1) stopProducer();
    
    // Asegúrate de cerrar el socket aquí o que el IServer lo cierre al volver
    closeSocket(socket); 
}
