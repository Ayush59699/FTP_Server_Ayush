#include "Authenticator.h"
#include <fstream>
#include <sstream>
#include <iostream>
 
Authenticator::Authenticator(const std::string &filename)
{
    loadUsers(filename);
}

void Authenticator::loadUsers(const std::string &filename)
{
    std::ifstream file(filename);

    if (!file.is_open())
    {
        std::cerr << "Failed to open user database: " << filename << std::endl;
        return;
    }
    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        std::stringstream ss(line);
        std::string username;
        std::string password;

        if (std::getline(ss, username, ':') && std::getline(ss, password))
        {
            users[username] = password;
        }
    }
}
bool Authenticator::authenticate(const std::string &username, const std::string &password)
{
    auto it = users.find(username);
    if (it == users.end())
        return false;
    return it->second == password;
}