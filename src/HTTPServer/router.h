#pragma once


#include <functional>
#include <string>
#include <unordered_map>
#include <stop_token>
#include "socket_types.h"

using RouterHandler = std::function<void(socket_t, const std::string&, std::stop_token)>;


class Router{
public:
    
    void registerRoute(const std::string& method, const std::string& path, RouterHandler handler);
    bool route(const std::string& method, const std::string& path, SOCKET socket, const std::string& requestBody, std::stop_token st) const;
    void setDefaultRoute(RouterHandler handler);

private:
    
    std::unordered_map<std::string, RouterHandler> routes;
    RouterHandler defaultRoute;
    std::string makeKey(const std::string& method, const std::string& path) const;
    

};