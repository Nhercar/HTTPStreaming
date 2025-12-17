#pragma once
#include <string>
#include <cstdint>

struct ServerConfig {
    std::string bindAddress = "0.0.0.0";
    uint16_t port = 5000;
    int deviceIndex = 0;
    int width = 640;
    int height = 480;
    int fps = 15;
    int jpegQuality = 80;
};

void parseServerArgs(int argc, char** argv, ServerConfig& cfg);