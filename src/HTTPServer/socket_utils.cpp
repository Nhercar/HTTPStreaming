#include "socket_utils.h"

bool sendAll(socket_t sock, const uint8_t* data, size_t len) {
    size_t totalSent = 0;
    while (totalSent < len) {
        int sent = send(sock, reinterpret_cast<const char*>(data + totalSent), 
                       static_cast<int>(len - totalSent), 0);
        if (sent == SOCKET_ERROR || sent == 0) {
            return false;
        }
        totalSent += static_cast<size_t>(sent);
    }
    return true;
}

bool receiveAll(socket_t sock, uint8_t* data, size_t len) {
    size_t totalReceived = 0;
    while (totalReceived < len) {
        int received = recv(sock, reinterpret_cast<char*>(data + totalReceived), 
                           static_cast<int>(len - totalReceived), 0);
        if (received == SOCKET_ERROR || received == 0) {
            return false;
        }
        totalReceived += static_cast<size_t>(received);
    }
    return true;
}
