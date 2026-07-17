#pragma once

#include <string>
#include <filesystem>

class ClientSession
{
public:
    explicit ClientSession(int clientSocket);
    ~ClientSession() = default;
    void start();

private:
    void sendWelcomeMessage();
    void receiveCommands();
    bool processCommand(const std::string &command);
    void sendResponse(const std::string &reponse);

private:
    bool handleUSER(const std::string &argument);
    bool handlePASS(const std::string &argument);
    bool handleQUIT();
    bool handleNOOP();
    bool handlePWD();
    bool handleCWD(const std::string &argument);
    bool handleLIST();

private:
    int clientSocket_;
    bool isAuthenticated_;
    std::string username_;
    std::filesystem::path rootDirectory_;
    std::filesystem::path currentDirectory_;
};
