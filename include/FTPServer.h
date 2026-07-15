#pragma once

class FTPServer
{
public:
    explicit FTPServer(int port);

    void start();

private:
    int port_;
};