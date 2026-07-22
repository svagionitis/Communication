/**
 * @file SocketUtils.h
 * @brief Cross-platform abstraction for Winsock2 and POSIX socket operations.
 */

#pragma once

#include "Types.h"
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace Communication {
namespace Platform {

#ifdef _WIN32
typedef SOCKET SocketHandle;
#define INVALID_SOCKET_HANDLE INVALID_SOCKET
#else
typedef int SocketHandle;
#define INVALID_SOCKET_HANDLE (-1)
#endif

/**
 * @brief RAII Manager for Windows Sockets (WSAStartup / WSACleanup).
 */
class SocketInitializer {
public:
    static SocketInitializer& instance();
    ~SocketInitializer();

    SocketInitializer(const SocketInitializer&) = delete;
    SocketInitializer& operator=(const SocketInitializer&) = delete;

private:
    SocketInitializer();
};

/**
 * @brief Closes a socket handle safely cross-platform.
 * @param handle Socket handle to close.
 */
void closeSocketHandle(SocketHandle handle);

/**
 * @brief Sets socket non-blocking mode.
 * @param handle Socket handle.
 * @param nonBlocking True to set non-blocking, false for blocking.
 * @return True on success.
 */
bool setSocketNonBlocking(SocketHandle handle, bool nonBlocking);

/**
 * @brief Configures native TCP Keep-Alive options on a socket handle.
 * @param handle Active TCP socket handle.
 * @param config TcpKeepAliveConfig parameters.
 * @return True if configuration succeeded, false otherwise.
 */
bool configureSocketKeepAlive(SocketHandle handle, const TcpKeepAliveConfig& config);

/**
 * @brief Retrieves formatted string of last socket error.
 * @return String description of last error.
 */
std::string getSocketErrorString();

} // namespace Platform
} // namespace Communication
