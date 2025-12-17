#include "http_response.h"
#include<sstream>

std::string buildHttpResponse(const HTTPResponse& resp){
    std::ostringstream oss;

    oss << "HTTP/1.1 " << resp.statusCode << " " << resp.statusMessage << "\r\n";
    std::map<std::string, std::string> headers = resp.headers; // Copia para no tocar los headers originales
    
    if (!resp.body.empty() && headers.find("Content-Length") == headers.end()){ //Si no se ha añadido el argumento Content-Length se añade automáticamente
        headers["Content-Length"] = std::to_string(resp.body.size());
    }

    if(headers.find("Connection") == headers.end()) headers["Connection"] = "close";    

    for(const auto& kv : headers){
        oss << kv.first << ": " << kv.second << "\r\n";
    }

    oss << "\r\n"; // Linea en blanco

    if (!resp.body.empty()) oss << resp.body; //Añadimos el cuerpo solo si no es empty


    return oss.str();
}