
#include "ClientSession.h"
#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

ClientSession::ClientSession(int clientSocket)
    : clientSocket_(clientSocket),
      isAuthenticated_(false),
      rootDirectory_(
          std::filesystem::canonical(
              std::filesystem::current_path().parent_path() / "ftp_root")),
      currentDirectory_(rootDirectory_)
{
}

void ClientSession::start()
{
    sendWelcomeMessage();
    receiveCommands();
}

void ClientSession::sendWelcomeMessage()
{

    sendResponse("220 Welcome to Ayush FTP Server\r\n");
}

bool ClientSession::handleUSER(const std::string &argument)
{

    username_ = argument;
    isAuthenticated_ = false;

    std::cout << "Username: " << username_ << '\n';
    sendResponse("331 Username OK, need password\r\n");
    return false;
}
bool ClientSession::handlePASS(const std::string &argument)
{
    (void)argument;
    if (username_.empty())
    {
        sendResponse("503 Login with USER first\r\n");
        return false;
    }

    isAuthenticated_ = true;
    currentDirectory_ = rootDirectory_;

    sendResponse("230 Login successful\r\n");

    return false;
}
bool ClientSession::handleQUIT()
{
    sendResponse("221 Goodbye\r\n");
    return true;
}
bool ClientSession::handleNOOP()
{
    sendResponse("200 OK\r\n");
    return false;
}

bool ClientSession::handlePWD()
{
    if (!isAuthenticated_)
    {
        sendResponse("530 Please login with USER and PASS\r\n");
        return false;
    }

    std::filesystem::path relative = std::filesystem::relative(currentDirectory_, rootDirectory_);
    std::string pwd = "/" + relative.string();

    if (pwd == "/.")
        pwd = "/";

    sendResponse("257 \"" + pwd + "\" is the current directory\r\n");

    return false;
}

bool ClientSession::handleCWD(const std::string &argument)
{
    if (!isAuthenticated_)
    {
        sendResponse("530 Please login with USER and PASS\r\n");
        return false;
    }

    if (argument.empty())
    {
        sendResponse("501 Missing directory name\r\n");
        return false;
    }

    std::filesystem::path newPath;
    if (argument == "/")
    {
        newPath = rootDirectory_;
    }
    else
    {
        newPath = currentDirectory_ / argument;
    }

    newPath = std::filesystem::weakly_canonical(newPath);

    if (!std::filesystem::exists(newPath) ||
        !std::filesystem::is_directory(newPath))
    {
        sendResponse("550 Directory not found\r\n");
        return false;
    }

    // Prevent escaping ftp_root
    auto relative = std::filesystem::relative(newPath, rootDirectory_);

    if (relative.string().compare(0, 2, "..") == 0)
    {
        sendResponse("550 Access denied\r\n");
        return false;
    }

    currentDirectory_ = newPath;

    sendResponse("250 Directory changed successfully\r\n");
    return false;
}

bool ClientSession::handleLIST()
{
    if (!isAuthenticated_)
    {
        sendResponse("530 Please login with USER and PASS\r\n");
        return false;
    }

    sendResponse("150 Opening directory listing\r\n");

    try
    {
        for (const auto &entry :
             std::filesystem::directory_iterator(currentDirectory_))
        {
            std::string line =
                (entry.is_directory() ? "[DIR] " : "[FILE] ");

            line += entry.path().filename().string();
            line += "\r\n";

            sendResponse(line);
        }
    }
    catch (const std::filesystem::filesystem_error &)
    {
        sendResponse("550 Failed to list directory\r\n");
        return false;
    }
    sendResponse("226 Transfer complete\r\n");

    return false;
}

bool ClientSession::processCommand(const std::string &command)
{
    std::cout << "Received: " << command;
    const auto pos = command.find(' ');

    std::string cmd;
    std::string arg;

    if (pos == std::string::npos)
    {
        cmd = command;
    }
    else
    {
        cmd = command.substr(0, pos);
        arg = command.substr(pos + 1);
    }
    while (!cmd.empty() &&
           (cmd.back() == '\r' || cmd.back() == '\n'))
    {
        cmd.pop_back();
    }

    while (!arg.empty() &&
           (arg.back() == '\r' || arg.back() == '\n'))
    {
        arg.pop_back();
    }

    if (cmd == "USER")
        return handleUSER(arg);

    if (cmd == "PASS")
        return handlePASS(arg);

    if (cmd == "PWD")
    {
        return handlePWD();
    }
    if (cmd == "CWD")
    {
        return handleCWD(arg);
    }

    if (cmd == "LIST")
    {
        return handleLIST();
    }

    if (cmd == "QUIT")
        return handleQUIT();

    if (cmd == "NOOP")
    {
        return handleNOOP();
    }

    sendResponse("502 Command not implemented\r\n");
    return false;
}

void ClientSession::receiveCommands()
{
    constexpr std::size_t BUFFER_SIZE = 1024;
    std::array<char, BUFFER_SIZE> buffer;

    while (true)
    {
        ssize_t bytesReceived = recv(clientSocket_, buffer.data(), buffer.size() - 1, 0);

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
        std::string command(buffer.data(), bytesReceived);

        if (processCommand(command))
        {
            break;
        }
    }
}

void ClientSession::sendResponse(const std::string &response)
{
    ssize_t bytesSent = send(clientSocket_, response.c_str(), response.length(), 0);
    if (bytesSent == -1)
    {
        std::cerr << "Send failed: "
                  << strerror(errno)
                  << '\n';
    }
}