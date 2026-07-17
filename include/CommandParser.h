#pragma once
#include <string>

struct ParsedCommand
{
    std::string command;
    std::string argument;
};

class CommandParser
{
public:
    ParsedCommand parse(const std::string &input) const;
};