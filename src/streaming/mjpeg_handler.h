#pragma once

#include <stop_token>
#include <string>
#include <chrono>
#include <thread>
#include <sstream>
#include <set>

#include "logger.h"
#include "http_parser.h"
#include "socket_server.h"
#include "handler_inerface.h"

class StreamHandler : public IHandler {
public:

    ~StreamHandler() noexcept override = default; // Match base class exception spec
    void handle(socket_t client, const std::string& path, std::stop_token st) override;

    void addClient(socket_t client) override;
    void removeClient(socket_t client) override;
    void stopAll() override;
    std::size_t getClientCount() override;

private:
    mutable std::mutex mtx;
    std::set<socket_t> clients;
};

void homeHandler(socket_t socket, const std::string&, std::stop_token st);

void notFoundHandler(socket_t socket, const std::string&, std::stop_token st);

void streamHandler(socket_t socket, const std::string&, std::stop_token st);


