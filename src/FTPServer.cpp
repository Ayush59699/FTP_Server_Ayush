
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

FTPServer::FTPServer(int port)
    : port_(port), serverSocket_(-1)
{
}

bool FTPServer::createSocket()
{
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ == -1)
    {
        std::cerr << "Socket Creation Failed. " << strerror(errno) << '\n';
        return false;
    }
    std::cout << "Socket Created Successfully!!\n";
    return true;
}

bool FTPServer::bindSocket()
{
    sockaddr_in serverAddr{};

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_); // host to network
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket_, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)) == -1)
    {
        std::cerr << "Bind failed: " << strerror(errno) << '\n';

        return false;
    }
    std::cout << "Bind Successful!!\n";
    return true;
}

bool FTPServer::startListening()
{

    if (listen(serverSocket_, 5) == -1)
    {
        std::cerr << "Listen Failed." << strerror(errno) << '\n';

        return false;
    }
    std::cout << "Listening on port " << port_ << '\n';
    return true;
}

void FTPServer::acceptClients()
{
    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);

    std::cout << "Waiting for a client.....\n";

    while (true)
    {

        int clientSocket = accept(serverSocket_,
                                  reinterpret_cast<sockaddr *>(&clientAddr),
                                  &clientLen);

        if (clientSocket == -1)
        {
            std::cerr << "Accept failed" << strerror(errno) << '\n';
            return;
        }
        std::cout << "Client Connected!!..\n";

        handleClient(clientSocket);
        // ::close(clientSocket);
    }
}

void FTPServer::handleClient(int clientSocket)
{
    std::thread([clientSocket]() {
        ClientSession session(clientSocket);
        session.start();
    }).detach();
}

void FTPServer::start()
{

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