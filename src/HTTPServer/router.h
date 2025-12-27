#pragma once


#include <functional>
#include <string>
#include <unordered_map>
#include <stop_token>
#include <memory>

#include "../streaming/handler_inerface.h"


using RouterHandler = std::shared_ptr<IHandler>;


class Router{
public:
    
    void registerRoute(const std::string& method, const std::string& path, RouterHandler handler);
    bool route(const std::string& method, const std::string& path, socket_t socket, const std::string& requestBody, std::stop_token st) const;
    void setDefaultRoute(RouterHandler handler);
    void maxClientsRoute(RouterHandler handler);

private:
    
    std::unordered_map<std::string, RouterHandler> routes;
    RouterHandler defaultRoute;
    std::string makeKey(const std::string& method, const std::string& path) const;
    

};