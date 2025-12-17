#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <mutex>
#include <iostream>

// Logger thread-safe simple
class Logger {
public:
    static Logger& getInstance();
    
    void info(const std::string& message);
    void error(const std::string& message);
    void debug(const std::string& message);

private:
    Logger() = default;
    std::mutex logMutex;
    
    void log(const std::string& level, const std::string& message);
};

#endif // LOGGER_H
