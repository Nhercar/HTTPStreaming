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
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#if defined(_WIN32)
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif
    std::ostringstream ts;
    ts << std::put_time(&tm_buf, "%F %T");

    // Thread ID
    auto threadId = std::this_thread::get_id();

    std::cout << "[" << level << "] "
              << "[" << ts.str() << "] "
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
