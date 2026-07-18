#pragma once
#include <string>
#include <algorithm>
#include <unordered_map>

class Authenticator
{
public:
    Authenticator(const std::string &filename);
    bool authenticate(const std::string &username, const std::string &password);

private:
    std::unordered_map<std::string, std::string> users;

    void loadUsers(const std::string &filename);
}; 