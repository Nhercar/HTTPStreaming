#pragma once

#include <string>
#include <sstream>

#include "handler_inerface.h"
#include "http_parser.h"

class HomeHandler : public IHandler {
public:
    void handle(socket_t client, const std::string& path, std::stop_token st) override;
    void stopAll() override {} // Nothing to stop
    std::size_t getClientCount() override { return 0; }
};

class NotFoundHandler : public IHandler {
public:
    void handle(socket_t client, const std::string& path, std::stop_token st) override;
    void stopAll() override {} // Nothing to stop
    std::size_t getClientCount() override { return 0; }
};

class MaxClientsReached : public IHandler {
public:
    void handle(socket_t client, const std::string& path, std::stop_token st) override;
    void stopAll() override {} // Nothing to stop
    std::size_t getClientCount() override { return 0; }
};