#include "CommandParser.h"
#include <algorithm>
#include <cctype>

ParsedCommand CommandParser::parse(const std::string &input) const
{
    ParsedCommand res;

    const auto pos = input.find(' ');

    if (pos == std::string::npos)
    {
        res.command = input;
    }
    else
    {
        res.command = input.substr(0, pos);
        res.argument = input.substr(pos + 1);
    }
    while (!res.command.empty() &&
           (res.command.back() == '\r' || res.command.back() == '\n'))
    {
        res.command.pop_back();
    }

    while (!res.argument.empty() &&
           (res.argument.back() == ' ' ||
            res.argument.back() == '\t' ||
            res.argument.back() == '\r' ||
            res.argument.back() == '\n'))
    {
        res.argument.pop_back();
    }

    const auto first = res.argument.find_first_not_of(" \t");

    if (first == std::string::npos)
    {
        res.argument.clear();
    }
    else
    {
        res.argument.erase(0, first);
    }

    std::transform(res.command.begin(),
                   res.command.end(),
                   res.command.begin(),
                   [](unsigned char c)
                   {
                       return std::toupper(c);
                   });

    return res;
}