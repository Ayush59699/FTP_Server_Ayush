cmake_minimum_required(VERSION 3.16)

project(FTPServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(ftp_server
    src/main.cpp
    src/FTPServer.cpp
    src/ClientSession.cpp
    src/CommandParser.cpp
    src/FileManager.cpp
    src/Authenticator.cpp
    src/Logger.cpp
    src/ConfigManager.cpp
)

target_link_libraries(ftp_server pthread)