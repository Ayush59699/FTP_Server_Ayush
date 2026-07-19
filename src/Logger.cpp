#include "Logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <algorithm>

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

void Logger::init(LogLevel level, const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentLevel_ = level;
    if (!logFilePath.empty()) {
        if (logFile_.is_open()) {
            logFile_.close();
        }
        logFile_.open(logFilePath, std::ios::out | std::ios::app);
    }
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
    }
    return "INFO";
}

LogLevel Logger::stringToLevel(const std::string& levelStr) {
    std::string upperStr = levelStr;
    std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(), ::toupper);
    if (upperStr == "DEBUG") return LogLevel::DEBUG;
    if (upperStr == "WARNING" || upperStr == "WARN") return LogLevel::WARNING;
    if (upperStr == "ERROR") return LogLevel::ERROR;
    return LogLevel::INFO;
}

void Logger::log(LogLevel level, const std::string& message, const std::string& clientIp, int threadId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level < currentLevel_) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S") << " ";
    ss << "[" << levelToString(level) << "] ";

    if (threadId != -1) {
        ss << "[TID: " << threadId << "] ";
    } else {
        ss << "[TID: " << std::this_thread::get_id() << "] ";
    }

    if (!clientIp.empty()) {
        ss << "[Client: " << clientIp << "] ";
    }

    ss << message << "\n";
    std::string logMsg = ss.str();

    // Console output
    if (level == LogLevel::ERROR || level == LogLevel::WARNING) {
        std::cerr << logMsg << std::flush;
    } else {
        std::cout << logMsg << std::flush;
    }

    // File output
    if (logFile_.is_open()) {
        logFile_ << logMsg << std::flush;
    }
}
