#include "ConfigManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::load(const std::string& configFilePath) {
    std::ifstream file(configFilePath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Strip comments
        auto commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }

        // Trim whitespace
        line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        line.erase(std::find_if(line.rbegin(), line.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), line.end());

        if (line.empty()) {
            continue;
        }

        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);

        // Trim key/value
        key.erase(key.begin(), std::find_if(key.begin(), key.end(), [](unsigned char ch) { return !std::isspace(ch); }));
        key.erase(std::find_if(key.rbegin(), key.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), key.end());

        value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); }));
        value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), value.end());

        if (key == "port") {
            port_ = std::stoi(value);
        } else if (key == "passive_start") {
            passiveStart_ = std::stoi(value);
        } else if (key == "passive_end") {
            passiveEnd_ = std::stoi(value);
        } else if (key == "server_ip") {
            serverIp_ = value;
        } else if (key == "max_clients") {
            maxClients_ = std::stoi(value);
        } else if (key == "log_level") {
            logLevel_ = value;
        } else if (key == "log_file") {
            logFile_ = value;
        } else if (key == "root_directory") {
            rootDir_ = value;
        }
    }
    return true;
}
