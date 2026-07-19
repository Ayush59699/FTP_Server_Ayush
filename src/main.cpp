#include "FTPServer.h"
#include "ConfigManager.h"
#include "Logger.h"
#include <iostream>

int main()
{
    ConfigManager::getInstance().load("ftp.conf");
    
    LogLevel level = Logger::stringToLevel(ConfigManager::getInstance().getLogLevel());
    Logger::getInstance().init(level, ConfigManager::getInstance().getLogFile());

    FTPServer server(2121);
    server.start();

    return 0;
}