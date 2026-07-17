#pragma once
#include <string>
#include <algorithm>
#include <filesystem>
#include <vector>

class FileManager
{
public:
    explicit FileManager(const std::filesystem::path &rootDirectory);

    std::filesystem::path resolvePath(const std::filesystem::path &currentDirectory, const std::string &path) const;
    bool createDirectory(const std::filesystem::path &path) const;

    bool removeDirectory(const std::filesystem::path &path) const;

    bool deleteFile(const std::filesystem::path &path) const;

    std::vector<std::string> listDirectory(
        const std::filesystem::path &path) const;

private:
    std::filesystem::path rootDirectory_;
};