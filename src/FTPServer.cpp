
#include <iostream>
#include <cstring>
#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include "FTPServer.h"
#include "ClientSession.h"
#include "ConfigManager.h"
#include "Logger.h"

FTPServer::FTPServer(int port)
    : port_(port), serverSocket_(-1)
{
}

bool FTPServer::createSocket()
{
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ == -1)
    {
        LOG_ERROR("Socket Creation Failed. " + std::string(strerror(errno)));
        return false;
    }
    LOG_INFO("Socket Created Successfully!!");
    return true;
}

bool FTPServer::bindSocket()
{
    sockaddr_in serverAddr{};

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_); // host to network
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Allow socket reuse
    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(serverSocket_, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)) == -1)
    {
        LOG_ERROR("Bind failed: " + std::string(strerror(errno)));
        return false;
    }
    LOG_INFO("Bind Successful!!");
    return true;
}

bool FTPServer::startListening()
{
    if (listen(serverSocket_, 5) == -1)
    {
        LOG_ERROR("Listen Failed: " + std::string(strerror(errno)));
        return false;
    }
    LOG_INFO("Listening on port " + std::to_string(port_));
    return true;
}

void FTPServer::acceptClients()
{
    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);

    LOG_INFO("Waiting for a client.....");

    while (true)
    {
        int clientSocket = accept(serverSocket_,
                                  reinterpret_cast<sockaddr *>(&clientAddr),
                                  &clientLen);

        if (clientSocket == -1)
        {
            LOG_ERROR("Accept failed: " + std::string(strerror(errno)));
            return;
        }

        std::string clientIp = inet_ntoa(clientAddr.sin_addr);
        LOG_INFO("Client Connected from IP: " + clientIp);

        handleClient(clientSocket, clientIp);
    }
}

void FTPServer::handleClient(int clientSocket, const std::string &clientIp)
{
    std::thread([clientSocket, clientIp]() {
        ClientSession session(clientSocket, clientIp);
        session.start();
    }).detach();
}

void FTPServer::start()
{
    // Try to load port from config
    int configPort = ConfigManager::getInstance().getPort();
    if (configPort > 0)
    {
        port_ = configPort;
    }

    if (!createSocket())
        return;
    if (!bindSocket())
        return;
    if (!startListening())
        return;

    acceptClients();

    if (serverSocket_ != -1)
    {
        ::close(serverSocket_);
        serverSocket_ = -1;
    }
}