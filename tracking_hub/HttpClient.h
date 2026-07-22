/**
 * @file HttpClient.h
 * @brief Native lightweight HTTP client for querying REST APIs.
 */

#pragma once

#include <cstdint>
#include <string>

namespace Communication {

class HttpClient {
public:
    /**
     * @brief Performs an HTTP GET request to specified host and path.
     * @param host Remote server hostname or IP address (e.g. "opensky-network.org").
     * @param port Port number (default: 80).
     * @param path Request path (e.g. "/api/states/all").
     * @param outResponseBody Output parameter storing the HTTP body content.
     * @return True if HTTP status 200 OK was received.
     */
    static bool get(const std::string& host, uint16_t port, const std::string& path, std::string& outResponseBody);
};

} // namespace Communication
