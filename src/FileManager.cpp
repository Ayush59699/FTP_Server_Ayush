#include "FileManager.h"
#include <iomanip>
#include <sstream>
#include <chrono>

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


// unix format
std::vector<std::string> FileManager::listDirectory(
    const std::filesystem::path &path) const
{
    std::vector<std::string> result;

    for (const auto &entry : std::filesystem::directory_iterator(path))
    {
        std::string filename = entry.path().filename().string();
        
        // Hide temporary upload files (*.tmp, *.part)
        if (filename.size() >= 4 && filename.compare(filename.size() - 4, 4, ".tmp") == 0) {
            continue;
        }
        if (filename.size() >= 5 && filename.compare(filename.size() - 5, 5, ".part") == 0) {
            continue;
        }

        std::ostringstream oss;

        if (entry.is_directory())
            oss << "drwxr-xr-x ";
        else
            oss << "-rw-r--r-- ";

        oss << "1 owner group ";

        if (entry.is_regular_file())
            oss << std::setw(10) << entry.file_size() << " ";
        else
            oss << std::setw(10) << 0 << " ";

        auto ftime = entry.last_write_time();

        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - decltype(ftime)::clock::now()
            + std::chrono::system_clock::now());

        std::time_t tt = std::chrono::system_clock::to_time_t(sctp);

        oss << std::put_time(std::localtime(&tt), "%b %d %H:%M") << " ";

        oss << filename;

        result.push_back(oss.str());
    }

    return result;
}