#pragma once
#include <string>
#include <unordered_map>

class ConfigManager {
public:
    static ConfigManager& getInstance();

    bool load(const std::string& configFilePath);

    int getPort() const { return port_; }
    int getPassiveStart() const { return passiveStart_; }
    int getPassiveEnd() const { return passiveEnd_; }
    std::string getServerIp() const { return serverIp_; }
    int getMaxClients() const { return maxClients_; }
    std::string getLogLevel() const { return logLevel_; }
    std::string getLogFile() const { return logFile_; }
    std::string getRootDir() const { return rootDir_; }

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    int port_ = 2121;
    int passiveStart_ = 50000;
    int passiveEnd_ = 50020;
    std::string serverIp_ = "127.0.0.1";
    int maxClients_ = 100;
    std::string logLevel_ = "INFO";
    std::string logFile_ = "";
    std::string rootDir_ = "ftp_root";
};
