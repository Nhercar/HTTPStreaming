#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::log(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    
    // Timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    // Thread ID
    auto threadId = std::this_thread::get_id();
    
    std::cout << "[" << level << "] "
              << "[Thread-" << threadId << "] "
              << message << std::endl;
}

void Logger::info(const std::string& message) {
    log("INFO", message);
}

void Logger::error(const std::string& message) {
    log("ERROR", message);
}

void Logger::debug(const std::string& message) {
    log("DEBUG", message);
}
