#pragma once
#include <string>
#include <filesystem>

#include "CommandParser.h"
#include "FileManager.h"
#include "Authenticator.h"

class ClientSession
{
public:
    explicit ClientSession(int clientSocket);
    ~ClientSession();
    void start();

private:
    CommandParser parser_;
    void
    sendWelcomeMessage();
    void receiveCommands();
    bool processCommand(const std::string &command);
    void sendResponse(const std::string &reponse);

    bool sendData(const std::string &data); // helper
    bool acceptDataConnection();
    void closeDataConnection();

private:
    bool handleUSER(const std::string &argument);
    bool handlePASS(const std::string &argument);
    bool handleQUIT();
    bool handleNOOP();
    bool handlePWD();
    bool handleCWD(const std::string &argument);
    bool handleLIST(const std::string &argument);
    bool handleMKD(const std::string &argument);
    bool handleRMD(const std::string &argument);
    bool handleDELE(const std::string &argument);

    bool handlePASV();
    bool handleSTOR(const std::string & argument);
    bool handleRETR(const std::string & argument);

private:
    int clientSocket_;
    bool isAuthenticated_;
    std::string username_;
    std::filesystem::path rootDirectory_;
    std::filesystem::path currentDirectory_;
    FileManager filemanager_;

    int passiveListenSocket_;
    int dataSocket_;
    bool passiveMode_;

    Authenticator authenticator_;
};
