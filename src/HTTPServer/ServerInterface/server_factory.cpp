#include "IServer.h"
#ifdef _WIN32
 #include "../WindowsServer/windows_server.h"
#else
 #include "../LinuxServer/linux_server.h"
#endif

std::unique_ptr<IServer> ServerFactory::create(int port) {
#ifdef _WIN32
    return std::make_unique<WinSocketServer>(port);
#else
    return std::make_unique<LinuxSocketServer>(port);
#endif
}