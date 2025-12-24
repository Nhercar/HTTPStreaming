#include "router.h"
#include "logger.h"

void Router::registerRoute(const std::string& method, const std::string& path, RouterHandler handler) {
    std::string key = makeKey(method, path);
    routes[key] = std::move(handler);
    Logger::getInstance().info("Ruta registrada: " + key);
}

bool Router::route(const std::string& method, const std::string& path, socket_t socket, const std::string& requestBody, std::stop_token st) const {
    std::string key = makeKey(method, path);
    auto it = routes.find(key);
    if (it != routes.end()) { 
        Logger::getInstance().info("Ruta encontrada: " + key);
        it->second->handle(socket, requestBody, st);
        return true;
    }
    if (defaultRoute) { // Usar ruta por defecto si existe Pero donde se le habría asignado
        Logger::getInstance().info("Usando ruta por defecto para: " + key);
        defaultRoute->handle(socket, requestBody, st);
        return true;
    }
    
    return false;
}

void Router::setDefaultRoute(RouterHandler handler) {
    defaultRoute = handler;
}

std::string Router::makeKey(const std::string& method, const std::string& path) const {
    return method + " " + path;
}

