#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include<string>
#include<map>


struct HTTPResponse {
    int statusCode = 200;             // 200 = OK, 404 = Not Found, etc.
    std::string statusMessage = "OK";               // OK, Not Found...
    std::map<std::string, std::string> headers; // {"Host": "localhost:8080", ...}
    std::string body;                           // Cuerpo cuando sea neceario
};


std::string buildHttpResponse(const HTTPResponse& resp);


#endif // HTTP_RESPONSE_H