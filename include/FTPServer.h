#pragma once
#include <string>

class FTPServer
{
public:
    explicit FTPServer(int port);

    void start();

private:
    int port_;
    int serverSocket_;

private:
    bool createSocket();
    bool bindSocket();
    bool startListening();

    void acceptClients();
    void handleClient(int clientSocket, const std::string &clientIp);
};