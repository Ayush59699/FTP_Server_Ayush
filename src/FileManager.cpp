#include "FileManager.h"

FileManager::FileManager(const std::filesystem::path &rootDirectory)
    : rootDirectory_(rootDirectory)
{
}

std::filesystem::path FileManager::resolvePath(
    const std::filesystem::path &currentDirectory,
    const std::string &path) const
{
    std::filesystem::path resolved;

    if (!path.empty() && path[0] == '/')
    {
        resolved = rootDirectory_ / path.substr(1);
    }
    else
    {
        resolved = currentDirectory / path;
    }

    try
    {
        resolved = std::filesystem::weakly_canonical(resolved);
    }
    catch (const std::filesystem::filesystem_error &)
    {
        return {};
    }

    const auto relative = std::filesystem::relative(resolved, rootDirectory_);

    if (relative.string().compare(0, 2, "..") == 0)
    {
        return {};
    }

    return resolved;
}

bool FileManager::createDirectory(const std::filesystem::path &path) const
{
    return std::filesystem::create_directories(path);
}
bool FileManager::removeDirectory(const std::filesystem::path &path) const
{
    return std::filesystem::remove(path);
}
bool FileManager::deleteFile(const std::filesystem::path &path) const
{
    return std::filesystem::remove(path);
}
std::vector<std::string> FileManager::listDirectory(
    const std::filesystem::path &path) const
{
    std::vector<std::string> result;

    for (const auto &entry : std::filesystem::directory_iterator(path))
    {
        std::string line;

        if (entry.is_directory())
            line = "[DIR] ";
        else
            line = "[FILE] ";

        line += entry.path().filename().string();

        result.push_back(line);
    }

    return result;
}