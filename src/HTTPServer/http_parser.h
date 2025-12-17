#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include<string>
#include <map>

#define MAX_BODY_SIZE (10 * 1024 * 1024)  // 10 MB

//Estructuras de los requests HTTP

struct HTTPRequest {
    std::string method;             // "GET", "POST", etc.
    std::string path;               // "/", "/imagen.jpg", etc
    std::string httpVersion;        // "HTTP/1.1"
    std::map<std::string, std::string> headers; // {"Host": "localhost:8080", ...}
    std::string body;               // Contenido del POST si aplica.
};

//Función encargada de parsear
HTTPRequest parseHTTPRequest(const std::string& rawRequest);

#endif