
#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <system_error>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include "FileManager.h"
#include "ClientSession.h"
#include "Authenticator.h"

#include "Logger.h"
#include "ConfigManager.h"

namespace
{
    std::filesystem::path resolveRootDirectory()
    {
        const std::array<std::filesystem::path, 4> candidates{
            std::filesystem::current_path() / ConfigManager::getInstance().getRootDir(),
            std::filesystem::current_path().parent_path() / ConfigManager::getInstance().getRootDir(),
            std::filesystem::absolute(ConfigManager::getInstance().getRootDir()),
            std::filesystem::current_path().parent_path().parent_path() / ConfigManager::getInstance().getRootDir()};

        std::error_code ec;
        for (const auto &candidate : candidates)
        {
            if (std::filesystem::exists(candidate, ec) && !ec &&
                std::filesystem::is_directory(candidate, ec) && !ec)
            {
                return std::filesystem::weakly_canonical(candidate, ec);
            }
        }

        const auto fallback = std::filesystem::absolute(ConfigManager::getInstance().getRootDir());
        std::filesystem::create_directories(fallback);
        return fallback;
    }
} // namespace

ClientSession::ClientSession(int clientSocket, const std::string &clientIp)
    : clientSocket_(clientSocket),
      isAuthenticated_(false),
      rootDirectory_(resolveRootDirectory()),
      currentDirectory_(rootDirectory_),
      filemanager_(rootDirectory_),
      passiveListenSocket_(-1),
      dataSocket_(-1),
      passiveMode_(false),
      authenticator_((rootDirectory_.parent_path() / "users.txt").string()),
      clientIp_(clientIp)

{
}

ClientSession::~ClientSession()
{
    closeDataConnection();
    if (clientSocket_ != -1)
    {
        ::close(clientSocket_);
        clientSocket_ = -1;
    }
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

    LOG_INFO("Username: " + username_, clientIp_);
    sendResponse("331 Username OK, need password\r\n");
    return false;
}
bool ClientSession::handlePASS(const std::string &argument)
{
    if (username_.empty())
    {
        sendResponse("503 Login with USER first\r\n");
        return false;
    }
    if (!authenticator_.authenticate(username_, argument))
    {
        LOG_WARN("Login incorrect for user: " + username_, clientIp_);
        sendResponse("530 Login incorrect\r\n");
        return false;
    }

    isAuthenticated_ = true;
    currentDirectory_ = rootDirectory_;

    LOG_INFO("Login successful for user: " + username_, clientIp_);
    sendResponse("230 Login successful\r\n");
    return false;
}
bool ClientSession::handleQUIT()
{
    LOG_INFO("Client quit", clientIp_);
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

bool ClientSession::sendData(const std::string &data)
{
    if (dataSocket_ == -1)
    {
        return false;
    }
    size_t totalSent = 0;
    while (totalSent < data.size())
    {
        ssize_t bytesSent = send(
            dataSocket_,
            data.c_str() + totalSent,
            data.size() - totalSent,
            0);

        if (bytesSent <= 0)
        {
            return false;
        }

        totalSent += static_cast<size_t>(bytesSent);
    }

    return true;
}

bool ClientSession::handleLIST(const std::string &argument)
{
    if (!isAuthenticated_)
    {
        sendResponse("530 Please login with USER and PASS\r\n");
        return false;
    }

    if (!passiveMode_)
    {
        sendResponse("425 Use PASV first\r\n");
        return false;
    }

    std::filesystem::path directory;

    if (argument.empty() || argument[0] == '-')
    {
        directory = currentDirectory_;
    }
    else
    {
        LOG_INFO("LIST argument = [" + argument + "]", clientIp_);
        directory = filemanager_.resolvePath(currentDirectory_, argument);
        if (directory.empty() || argument[0] == '-')
        {
            sendResponse("550 Access denied\r\n");
            return false;
        }
    }
    if (!std::filesystem::exists(directory) ||
        !std::filesystem::is_directory(directory))
    {
        sendResponse("550 Directory not found\r\n");
        return false;
    }
    

    LOG_INFO("[LIST] Entered", clientIp_);

    sendResponse("150 Opening directory listing\r\n");
    LOG_INFO("[LIST] Sent 150", clientIp_);

    LOG_INFO("[LIST] Waiting for data connection...", clientIp_);

    if (!acceptDataConnection())
    {
        LOG_ERROR("[LIST] accept failed", clientIp_);
        sendResponse("425 Can't open data connection\r\n");
        closeDataConnection();
        return false;
    }

    LOG_INFO("[LIST] Data connection accepted", clientIp_);

    auto entries = filemanager_.listDirectory(directory);

    LOG_INFO("[LIST] Entries: " + std::to_string(entries.size()), clientIp_);

    for (const auto &entry : entries)
    {
        LOG_DEBUG("[LIST] " + entry, clientIp_);

        if (!sendData(entry + "\r\n"))
        {
            LOG_ERROR("[LIST] sendData failed", clientIp_);
            closeDataConnection();
            sendResponse("426 Transfer aborted\r\n");
            return false;
        }
    }

    LOG_INFO("[LIST] Closing connection", clientIp_);

    closeDataConnection();

    sendResponse("226 Transfer complete\r\n");

    LOG_INFO("[LIST] Done", clientIp_);

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

bool ClientSession::acceptDataConnection()
{
    if (!passiveMode_ || passiveListenSocket_ == -1)
    {
        return false;
    }

    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);

    dataSocket_ = accept(passiveListenSocket_,
                         reinterpret_cast<sockaddr *>(&clientAddr), &clientLen);

    if (dataSocket_ == -1)
    {
        return false;
    }

    return true;
}

void ClientSession::closeDataConnection()
{
    if (dataSocket_ != -1)
    {
        ::close(dataSocket_);
        dataSocket_ = -1;
    }
    if (passiveListenSocket_ != -1)
    {
        ::close(passiveListenSocket_);
        passiveListenSocket_ = -1;
    }
    passiveMode_ = false;
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

bool ClientSession::handlePASV()
{
    if (!isAuthenticated_)
    {
        sendResponse("530 Please login with USER and PASS\r\n");
        return false;
    }
    if (passiveListenSocket_ != -1)
    {
        close(passiveListenSocket_);
        passiveListenSocket_ = -1;
    }

    passiveListenSocket_ = socket(AF_INET, SOCK_STREAM, 0);

    if (passiveListenSocket_ == -1)
    {
        sendResponse("425 Can't open passive connection\r\n");
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    bool bound = false;
    int pStart = ConfigManager::getInstance().getPassiveStart();
    int pEnd = ConfigManager::getInstance().getPassiveEnd();
    for (int port = pStart; port <= pEnd; ++port) {
        addr.sin_port = htons(port);
        if (bind(passiveListenSocket_,
                 reinterpret_cast<sockaddr *>(&addr),
                 sizeof(addr)) != -1) {
            bound = true;
            break;
        }
    }

    if (!bound)
    {
        close(passiveListenSocket_);
        passiveListenSocket_ = -1;

        sendResponse("425 Can't open passive connection\r\n");
        return false;
    }

    if (listen(passiveListenSocket_, 1) == -1)
    {
        close(passiveListenSocket_);
        passiveListenSocket_ = -1;

        sendResponse("425 Can't open passive connection\r\n");
        return false;
    }

    sockaddr_in boundAddr{};
    socklen_t len = sizeof(boundAddr);

    if (getsockname(passiveListenSocket_,
                    reinterpret_cast<sockaddr *>(&boundAddr),
                    &len) == -1)
    {
        close(passiveListenSocket_);
        passiveListenSocket_ = -1;

        sendResponse("425 Can't open passive connection\r\n");
        return false;
    }

    int port = ntohs(boundAddr.sin_port);

    int p1 = port / 256;
    int p2 = port % 256;

    passiveMode_ = true;

    // Use server IP from config. Convert dots to commas
    std::string ip = ConfigManager::getInstance().getServerIp();
    std::replace(ip.begin(), ip.end(), '.', ',');

    std::string response =
        "227 Entering Passive Mode (" + ip + "," +
        std::to_string(p1) + "," +
        std::to_string(p2) + ")\r\n";

    sendResponse(response);

    return false;
}

bool ClientSession:: handleSTOR(const std::string & argument){

    if(!isAuthenticated_){
        sendResponse("530 Please login with USER and PASS\r\n");
        return false;
    }
    if (argument.empty())
    {
        sendResponse("501 Missing file name\r\n");
        return false;
    }

    if (!passiveMode_)
    {
        sendResponse("425 Use PASV first\r\n");
        return false;
    }

    std::filesystem::path filePath = filemanager_.resolvePath(currentDirectory_, argument);

    if(filePath.empty()){
sendResponse("550 Access denied\r\n");
        return false;
    }

 if (!acceptDataConnection())
    {
        sendResponse("425 Can't open data connection\r\n");
        closeDataConnection();
        return false;
    }
    
    std::ofstream output (filePath, std::ios::binary);

    if(!output.is_open()){
         closeDataConnection();
        sendResponse("550 Failed to create file\r\n");
        return false;
    }
    sendResponse("150 Opening binary mode data connection\r\n");

    constexpr size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];

    while (true)
    {
        ssize_t bytesReceived =
            recv(dataSocket_, buffer, BUFFER_SIZE, 0);

        if (bytesReceived < 0)
        {
            output.close();
            std::filesystem::remove(filePath);
            closeDataConnection();
            sendResponse("426 Transfer aborted\r\n");
            return false;
        }

        if (bytesReceived == 0)
            break;

        output.write(buffer, bytesReceived);

        if (!output)
        {
            output.close();
            std::filesystem::remove(filePath);
            closeDataConnection();
            sendResponse("451 Local error writing file\r\n");
            return false;
        }
    }

    output.close();
    closeDataConnection();

    sendResponse("226 File upload successful\r\n");

    std::cout << "Uploaded: "
              << filePath
              << std::endl;

    return false;
}


bool ClientSession:: handleRETR(const std::string & argument){

    if(!isAuthenticated_){
        sendResponse("530 Please login with USER and PASS\r\n");
        return false;
    }
    if (argument.empty())
    {
        sendResponse("501 Missing file name\r\n");
        return false;
    }

    if (!passiveMode_)
    {
        sendResponse("425 Use PASV first\r\n");
        return false;
    }

     std::filesystem::path filePath =
        filemanager_.resolvePath(currentDirectory_, argument);

    if (filePath.empty())
    {
        sendResponse("550 Access denied\r\n");
        return false;
    }

    if (!std::filesystem::exists(filePath))
    {
        sendResponse("550 File not found\r\n");
        return false;
    }

    if (!std::filesystem::is_regular_file(filePath))
    {
        sendResponse("550 Not a regular file\r\n");
        return false;
    }

    if (!acceptDataConnection())
    {
        sendResponse("425 Can't open data connection\r\n");
        closeDataConnection();
        return false;
    }

    std::ifstream input(filePath, std::ios::binary);

    if (!input.is_open())
    {
        closeDataConnection();
        sendResponse("550 Failed to open file\r\n");
        return false;
    }

    sendResponse("150 Opening binary mode data connection\r\n");

    constexpr size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];

    while (input)
    {
        input.read(buffer, BUFFER_SIZE);
        std::streamsize bytesRead = input.gcount();

        if (bytesRead <= 0)
            break;

        size_t totalSent = 0;

        while (totalSent < static_cast<size_t>(bytesRead))
        {
            ssize_t bytesSent = send(dataSocket_, buffer + totalSent, static_cast<size_t>(bytesRead) - totalSent, 0);

            if (bytesSent <= 0)
            {
                input.close();
                closeDataConnection();
                sendResponse("426 Transfer aborted\r\n");
                return false;
            }

            totalSent += bytesSent;
        }
    }
    input.close();
    closeDataConnection();

    sendResponse("226 File download successful\r\n");

    LOG_INFO("Downloaded: " + filePath.string(), clientIp_);

    return false;
}


bool ClientSession::handleTYPE(const std::string &argument)
{
    if (!isAuthenticated_)
    {
        sendResponse("530 Please login with USER and PASS\r\n");
        return false;
    }
    
    if (argument == "I" || argument == "A")
    {
        sendResponse("200 Type set to " + argument + "\r\n");
    }
    else
    {
        sendResponse("504 Command not implemented for that parameter\r\n");
    }
    return false;
}

bool ClientSession::handleSYST()
{
    sendResponse("215 UNIX Type: L8\r\n");
    return false;
}

bool ClientSession::handleRNFR(const std::string &argument)
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

    auto path = filemanager_.resolvePath(currentDirectory_, argument);
    if (path.empty() || !std::filesystem::exists(path))
    {
        LOG_WARN("RNFR failed: path empty or does not exist: " + argument, clientIp_);
        sendResponse("550 File not found\r\n");
        return false;
    }

    renameSource_ = path;
    LOG_INFO("RNFR success: stored source " + renameSource_.string(), clientIp_);
    sendResponse("350 Requested file action pending further information\r\n");
    return false;
}

bool ClientSession::handleRNTO(const std::string &argument)
{
    if (!isAuthenticated_)
    {
        sendResponse("530 Please login with USER and PASS\r\n");
        return false;
    }
    if (renameSource_.empty())
    {
        sendResponse("503 Bad sequence of commands\r\n");
        return false;
    }
    if (argument.empty())
    {
        sendResponse("501 Missing destination name\r\n");
        return false;
    }

    auto destPath = filemanager_.resolvePath(currentDirectory_, argument);
    if (destPath.empty())
    {
        sendResponse("550 Access denied\r\n");
        return false;
    }

    try
    {
        std::filesystem::rename(renameSource_, destPath);
        LOG_INFO("RNTO success: renamed " + renameSource_.string() + " to " + destPath.string(), clientIp_);
        renameSource_.clear();
        sendResponse("250 Rename successful\r\n");
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("RNTO exception: " + std::string(e.what()), clientIp_);
        sendResponse("550 Rename failed\r\n");
    }

    return false;
}

bool ClientSession::handleSIZE(const std::string &argument)
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

    auto path = filemanager_.resolvePath(currentDirectory_, argument);
    if (path.empty() || !std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
    {
        sendResponse("550 File not found or not a regular file\r\n");
        return false;
    }

    try
    {
        auto size = std::filesystem::file_size(path);
        sendResponse("213 " + std::to_string(size) + "\r\n");
    }
    catch (const std::exception &e)
    {
        sendResponse("550 Failed to get file size\r\n");
    }

    return false;
}

bool ClientSession::handleMDTM(const std::string &argument)
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

    auto path = filemanager_.resolvePath(currentDirectory_, argument);
    if (path.empty() || !std::filesystem::exists(path))
    {
        sendResponse("550 File not found\r\n");
        return false;
    }

    try
    {
        auto ftime = std::filesystem::last_write_time(path);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - decltype(ftime)::clock::now() + std::chrono::system_clock::now());
        std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
        
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&tt), "%Y%m%d%H%M%S");
        sendResponse("213 " + oss.str() + "\r\n");
    }
    catch (const std::exception &e)
    {
        sendResponse("550 Failed to get modification time\r\n");
    }

    return false;
}

bool ClientSession::processCommand(const std::string &command)
{
    LOG_DEBUG("Received command: " + command, clientIp_);

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
        return handleLIST(parsed.argument);

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
    if (parsed.command == "PASV")
    {
        return handlePASV();
    }
    if(parsed.command == "STOR"){
        return handleSTOR(parsed.argument);
    }
        if(parsed.command == "RETR"){
        return handleRETR(parsed.argument);
    }
    if (parsed.command == "TYPE")
    {
        return handleTYPE(parsed.argument);
    }
    if (parsed.command == "SYST")
    {
        return handleSYST();
    }
    if (parsed.command == "RNFR")
    {
        return handleRNFR(parsed.argument);
    }
    if (parsed.command == "RNTO")
    {
        return handleRNTO(parsed.argument);
    }
    if (parsed.command == "SIZE")
    {
        return handleSIZE(parsed.argument);
    }
    if (parsed.command == "MDTM")
    {
        return handleMDTM(parsed.argument);
    }

    sendResponse("502 Command not implemented\r\n");
    return false;
}

void ClientSession::receiveCommands()
{
    constexpr std::size_t BUFFER_SIZE = 1024;
    std::array<char, BUFFER_SIZE> buffer;
    std::string accumulated;

    while (true)
    {
        ssize_t bytesReceived = recv(clientSocket_, buffer.data(), buffer.size() - 1, 0);

        if (bytesReceived == -1)
        {
            LOG_ERROR("Receive failed: " + std::string(strerror(errno)), clientIp_);
            break;
        }
        if (bytesReceived == 0)
        {
            LOG_INFO("Client disconnected.", clientIp_);
            break;
        }

        accumulated.append(buffer.data(), bytesReceived);

        std::string::size_type pos;
        while ((pos = accumulated.find("\r\n")) != std::string::npos)
        {
            std::string command = accumulated.substr(0, pos + 2);
            accumulated.erase(0, pos + 2);

            if (processCommand(command))
                return;
        }
    }
}

void ClientSession::sendResponse(const std::string &response)
{
    size_t totalSent = 0;
    while (totalSent < response.length())
    {
        ssize_t bytesSent = send(clientSocket_, response.c_str() + totalSent,
                                 response.length() - totalSent, 0);
        if (bytesSent <= 0)
        {
            LOG_ERROR("Send failed: " + std::string(strerror(errno)), clientIp_);
            return;
        }
        totalSent += static_cast<size_t>(bytesSent);
    }
}