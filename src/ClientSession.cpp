
#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include "ClientSession.h"
#include "FileManager.h"

ClientSession::ClientSession(int clientSocket)
    : clientSocket_(clientSocket),
      isAuthenticated_(false),
      rootDirectory_(
          std::filesystem::canonical(
              std::filesystem::current_path().parent_path() / "ftp_root")),
      currentDirectory_(rootDirectory_),
      filemanager_(rootDirectory_)
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
        newPath = filemanager_.resolvePath(currentDirectory_, argument);

        if (newPath.empty())
        {
            sendResponse("550 Access denied\r\n");
            return false;
        }
    }
    if (!std::filesystem::exists(newPath) ||
        !std::filesystem::is_directory(newPath))
    {
        sendResponse("550 Directory not found\r\n");
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
bool ClientSession::handleRMD(const std::string &argument)
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

    std::filesystem::path directory = filemanager_.resolvePath(currentDirectory_, argument);

    if (directory.empty())
    {
        sendResponse("550 Access denied\r\n");
        return false;
    }

    try
    {
        if (!std::filesystem::exists(directory))
        {
            sendResponse("550 Directory not found\r\n");
            return false;
        }

        if (!std::filesystem::is_directory(directory))
        {
            sendResponse("550 Not a directory\r\n");
            return false;
        }

        if (filemanager_.removeDirectory(directory))
        {
            sendResponse("250 Directory removed successfully\r\n");
        }
        else
        {
            sendResponse("550 Failed to remove directory\r\n");
        }
    }
    catch (const std::filesystem::filesystem_error &)
    {
        sendResponse("550 Failed to remove directory\r\n");
    }

    return false;
}

bool ClientSession::handleMKD(const std::string &argument)
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

    std::filesystem::path newDirectory = filemanager_.resolvePath(currentDirectory_, argument);

    if (newDirectory.empty())
    {
        sendResponse("550 Access denied\r\n");
        return false;
    }

    try
    {
        if (std::filesystem::exists(newDirectory))
        {
            sendResponse("550 Directory already exists\r\n");
            return false;
        }

        if (filemanager_.createDirectory(newDirectory))
        {
            sendResponse("257 \"" + argument + "\" directory created\r\n");
        }
        else
        {
            sendResponse("550 Failed to create directory\r\n");
        }
    }
    catch (const std::filesystem::filesystem_error &)
    {
        sendResponse("550 Failed to create directory\r\n");
    }

    return false;
}
bool ClientSession::handleDELE(const std::string &argument)
{
    if (!isAuthenticated_)
    {
        sendResponse("530 Please login with USER and PASS\r\n");
        return false;
    }

    if (argument.empty())
    {
        sendResponse("501 Missing file name\r\n");
        return false;
    }

    auto file = filemanager_.resolvePath(currentDirectory_, argument);

    if (file.empty())
    {
        sendResponse("550 Access denied\r\n");
        return false;
    }

    try
    {
        if (!std::filesystem::exists(file))
        {
            sendResponse("550 File not found\r\n");
            return false;
        }

        if (!std::filesystem::is_regular_file(file))
        {
            sendResponse("550 Not a regular file\r\n");
            return false;
        }

        if (filemanager_.deleteFile(file))
        {
            sendResponse("250 File deleted successfully\r\n");
        }
        else
        {
            sendResponse("550 Failed to delete file\r\n");
        }
    }
    catch (const std::filesystem::filesystem_error &)
    {
        sendResponse("550 Failed to delete file\r\n");
    }

    return false;
}

bool ClientSession::processCommand(const std::string &command)
{
    std::cout << "Received: " << command;

    ParsedCommand parsed = parser_.parse(command);

    if (parsed.command == "USER")
        return handleUSER(parsed.argument);

    if (parsed.command == "PASS")
        return handlePASS(parsed.argument);

    if (parsed.command == "PWD")
    {
        return handlePWD();
    }
    if (parsed.command == "CWD")
    {
        return handleCWD(parsed.argument);
    }

    if (parsed.command == "LIST")
        return handleLIST();

    if (parsed.command == "QUIT")
        return handleQUIT();

    if (parsed.command == "NOOP")
        return handleNOOP();

    if (parsed.command == "MKD")
    {
        return handleMKD(parsed.argument);
    }
    if (parsed.command == "RMD")
    {
        return handleRMD(parsed.argument);
    }
    if (parsed.command == "DELE")
    {
        return handleDELE(parsed.argument);
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