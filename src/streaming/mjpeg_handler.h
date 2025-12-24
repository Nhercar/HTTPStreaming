#pragma once

#include <stop_token>
#include <string>
#include <chrono>
#include <thread>
#include <sstream>
#include <set>
#include <mutex>
#include <atomic>
#include <vector>
#include <condition_variable>
#include <stop_token>

#include "logger.h"
#include "http_parser.h"
#include "socket_server.h"
#include "handler_inerface.h"

class StreamHandler : public IHandler {
public:
    StreamHandler();
    ~StreamHandler() noexcept override;
    void handle(socket_t client, const std::string& path, std::stop_token st) override;
    void addClient(socket_t client) override;
    void removeClient(socket_t client) override;
    void stopAll() override;
    std::size_t getClientCount() override;

private:
    mutable std::mutex mtx;
    std::set<socket_t> clients;
    std::atomic<int> activeClients{0};
    // Frame producer state
    std::jthread frameProducer;
    std::atomic<bool> producing{false};
    std::vector<uint8_t> latestFrame;
    std::mutex frameMtx;
    std::condition_variable frameCv;

    void startProducer();
    void stopProducer();
    void producerLoop(std::stop_token st);
};


