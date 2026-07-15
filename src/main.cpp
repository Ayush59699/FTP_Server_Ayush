#include "FTPServer.h"

#include <iostream>
int main()
{

    FTPServer server(2121);
    server.start();

    // std::cout << "FTP SERVER Starting...\n";
    return 0;
}