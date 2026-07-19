#pragma once
#include <string>
#include <mutex>
#include <fstream>
#include <iostream>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static Logger& getInstance();

    void init(LogLevel level, const std::string& logFilePath = "");
    void log(LogLevel level, const std::string& message, const std::string& clientIp = "", int threadId = -1);

    static std::string levelToString(LogLevel level);
    static LogLevel stringToLevel(const std::string& levelStr);

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    LogLevel currentLevel_ = LogLevel::INFO;
    std::ofstream logFile_;
    std::mutex mutex_;
};

#define LOG_DEBUG(msg, ...) Logger::getInstance().log(LogLevel::DEBUG, msg, ##__VA_ARGS__)
#define LOG_INFO(msg, ...) Logger::getInstance().log(LogLevel::INFO, msg, ##__VA_ARGS__)
#define LOG_WARN(msg, ...) Logger::getInstance().log(LogLevel::WARNING, msg, ##__VA_ARGS__)
#define LOG_ERROR(msg, ...) Logger::getInstance().log(LogLevel::ERROR, msg, ##__VA_ARGS__)
