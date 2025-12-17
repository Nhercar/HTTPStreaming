#include "http_parser.h"
#include <sstream>
#include <algorithm>
#include <cctype>

// ---------- utilidades internas ----------
static inline void trim(std::string& s) {
    // quitar espacios en ambos extremos (incluye \r y \n)
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))  s.pop_back();
}

// request-line: rellena method/path/version y devuelve posición de fin de línea
static size_t parseRequestLine(const std::string& rawRequest, HTTPRequest& req) {
    size_t firstLineEnd = rawRequest.find("\r\n");
    if (firstLineEnd == std::string::npos) return std::string::npos;

    std::string requestLine = rawRequest.substr(0, firstLineEnd);
    std::istringstream iss(requestLine);
    iss >> req.method >> req.path >> req.httpVersion;
    return firstLineEnd;
}

// headers: devuelve posición del inicio del body (o npos si no hay)
static size_t parseHeaders(const std::string& rawRequest, size_t firstLineEnd, HTTPRequest& req) {
    size_t headerStart = firstLineEnd + 2; // saltar "\r\n"
    size_t headersEnd = rawRequest.find("\r\n\r\n", headerStart);
    size_t bodyStart = std::string::npos;

    if (headersEnd == std::string::npos) {
        headersEnd = rawRequest.length(); // sin body
    } else {
        bodyStart = headersEnd + 4; // saltar "\r\n\r\n"
    }

    std::string headerSection = rawRequest.substr(headerStart, headersEnd - headerStart);
    std::istringstream headerStream(headerSection);
    std::string headerLine;

    while (std::getline(headerStream, headerLine)) {
        trim(headerLine);
        if (headerLine.empty()) continue;

        size_t colonPos = headerLine.find(":");
        if (colonPos != std::string::npos) {
            std::string key = headerLine.substr(0, colonPos);
            std::string value = headerLine.substr(colonPos + 1); // saltar ':'
            trim(key);
            trim(value);
            req.headers[key] = value;
        }
    }

    return bodyStart;
}

static void parseBody(const std::string& rawRequest, size_t bodyStart, HTTPRequest& req) {
    if (bodyStart == std::string::npos || bodyStart >= rawRequest.size()) return;

    size_t availableBody = rawRequest.size() - bodyStart;
    if (availableBody > MAX_BODY_SIZE) return; // demasiado grande, rechazar

    auto it = req.headers.find("Content-Length");
    if (it != req.headers.end()) {
        try {
            size_t len = static_cast<size_t>(std::stoul(it->second));
            if (len <= MAX_BODY_SIZE) {
                size_t toCopy = std::min(len, availableBody);
                req.body = rawRequest.substr(bodyStart, toCopy);
            }
        } catch (...) {
            // Content-Length inválido: ignorar body
        }
    } else {
        // Sin Content-Length, acepta lo disponible (ya validado contra MAX_BODY_SIZE)
        req.body = rawRequest.substr(bodyStart, availableBody);
    }
}

// ---------- función pública ----------
HTTPRequest parseHTTPRequest(const std::string& rawRequest) {
    HTTPRequest req;

    // 1) Request Line
    size_t firstLineEnd = parseRequestLine(rawRequest, req);
    if (firstLineEnd == std::string::npos) return req;

    // 2) Headers
    size_t bodyStart = parseHeaders(rawRequest, firstLineEnd, req);

    // 3) Body (opcional)
    parseBody(rawRequest, bodyStart, req);

    return req;
}
