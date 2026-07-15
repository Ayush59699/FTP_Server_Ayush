#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "FTPServer.h"

FTPServer::FTPServer(int port)
    : port_(port)
{
}

void FTPServer::start()
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket == -1)
    {
        std::cerr << "Socker Creation Failed...\n";
        return;
    }
    // std::cout << serverSocket << '\n';
    std::cout << "Socket Created Successfully!!\n";

    sockaddr_in serverAddr{};

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_); // host to network
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)) == -1)
    {
        std::cerr << "Bind Failed...\n";
        close(serverSocket);
        return;
    }
    std::cout << "Bind Successful!!\n";

    if (listen(serverSocket, 5) == -1)
    {
        std::cerr << "Listen Failed..\n";
        close(serverSocket);
        return;
    }
    std::cout << "Listening on port " << port_ << '\n';

    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    std::cout << "Waiting for a client.....\n";

    int clientSocket = accept(serverSocket,
                              reinterpret_cast<sockaddr *>(&clientAddr),
                              &clientLen);
    if (clientSocket == -1)
    {
        std::cerr << "Accept faied\n";
        close(serverSocket);
        return;
    }
    std::cout << "Client Connected!!..\n";

    std::string welcome = "220 Welcome to Ayush FTP Server\r\n";

    ssize_t bytesSent = send(clientSocket, welcome.c_str(), welcome.length(), 0);

    char buffer[1024];
    while (true)
    {
        ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived == -1)
        {
            std::cerr << "Receive failed: " << strerror(errno) << '\n';
            break;
        }
        if (bytesReceived == 0)
        {
            std::cout << "Client disconnected.\n";
            break;
        }
        buffer[bytesReceived] = '\0';
        std::cout << "Received: " << buffer;
    }

    close(clientSocket);
    close(serverSocket);

    // if (bytesSent == -1)
    // {
    //     std::cerr << "Send Failed: " << strerror(errno) << '\n';
    // }
    // else
    // {
    //     std::cout << "Sent " << bytesSent << " bytes\n";
    // }
}