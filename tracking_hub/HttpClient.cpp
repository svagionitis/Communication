/**
 * @file HttpClient.cpp
 * @brief Native C++ socket HTTP GET client implementation.
 */

#include "HttpClient.h"
#include "Platform/SocketUtils.h"

#include <cstring>
#include <glog/logging.h>
#include <sstream>

namespace Communication {

bool HttpClient::get(const std::string& host, uint16_t port, const std::string& path, std::string& outResponseBody)
{
    Platform::SocketInitializer::instance();

    struct addrinfo hints { };
    struct addrinfo* res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
        LOG(ERROR) << "HttpClient getaddrinfo failed for host: " << host;
        return false;
    }

    Platform::SocketHandle sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET_HANDLE) {
        LOG(ERROR) << "HttpClient failed to create socket";
        freeaddrinfo(res);
        return false;
    }

    if (connect(sock, res->ai_addr, static_cast<socklen_t>(res->ai_addrlen)) < 0) {
        LOG(ERROR) << "HttpClient failed to connect to " << host << ":" << port;
        Platform::closeSocketHandle(sock);
        freeaddrinfo(res);
        return false;
    }
    freeaddrinfo(res);

    std::stringstream reqStream;
    reqStream << "GET " << path << " HTTP/1.1\r\n"
              << "Host: " << host << "\r\n"
              << "User-Agent: AntigravityTrackingHub/1.0\r\n"
              << "Accept: application/json\r\n"
              << "Connection: close\r\n\r\n";

    std::string request = reqStream.str();
    if (::send(sock, request.data(), request.size(), 0) < 0) {
        LOG(ERROR) << "HttpClient failed to send HTTP request";
        Platform::closeSocketHandle(sock);
        return false;
    }

    std::string response;
    char buffer[4096];
    int bytesRead = 0;
    while ((bytesRead = ::recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        response.append(buffer, bytesRead);
    }

    Platform::closeSocketHandle(sock);

    std::size_t bodyPos = response.find("\r\n\r\n");
    if (bodyPos == std::string::npos) {
        outResponseBody = response;
        return false;
    }

    std::string header = response.substr(0, bodyPos);
    outResponseBody = response.substr(bodyPos + 4);

    return (header.find("200 OK") != std::string::npos || header.find("HTTP/1.1 200") != std::string::npos);
}

} // namespace Communication
