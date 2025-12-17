#include "server_config.h"
#include <iostream>
#include <string>

void parseServerArgs(int argc, char** argv, ServerConfig& cfg) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 < argc) return argv[++i];
            return nullptr;
        };
        if (arg == "--port") {
            if (const char* v = next()) {
                try {
                    int val = std::stoi(v);
                    if (val < 0 || val > 65535) {
                        std::cerr << "Error: port must be 0-65535, got " << val << std::endl;
                    } else {
                        cfg.port = static_cast<uint16_t>(val);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing port: " << e.what() << std::endl;
                }
            }
        } else if (arg == "--bind") {
            if (const char* v = next()) cfg.bindAddress = v;
        } else if (arg == "--device") {
            if (const char* v = next()) {
                try {
                    cfg.deviceIndex = std::stoi(v);
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing device: " << e.what() << std::endl;
                }
            }
        } else if (arg == "--width") {
            if (const char* v = next()) {
                try {
                    int val = std::stoi(v);
                    if (val <= 0) {
                        std::cerr << "Error: width must be positive" << std::endl;
                    } else {
                        cfg.width = val;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing width: " << e.what() << std::endl;
                }
            }
        } else if (arg == "--height") {
            if (const char* v = next()) {
                try {
                    int val = std::stoi(v);
                    if (val <= 0) {
                        std::cerr << "Error: height must be positive" << std::endl;
                    } else {
                        cfg.height = val;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing height: " << e.what() << std::endl;
                }
            }
        } else if (arg == "--fps") {
            if (const char* v = next()) {
                try {
                    int val = std::stoi(v);
                    if (val < 0) {
                        std::cerr << "Error: fps must be non-negative" << std::endl;
                    } else {
                        cfg.fps = val;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing fps: " << e.what() << std::endl;
                }
            }
        } else if (arg == "--quality") {
            if (const char* v = next()) {
                try {
                    int val = std::stoi(v);
                    if (val < 0 || val > 100) {
                        std::cerr << "Error: quality must be 0-100, got " << val << std::endl;
                    } else {
                        cfg.jpegQuality = val;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing quality: " << e.what() << std::endl;
                }
            }
        }
    }
}
