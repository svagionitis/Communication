/**
 * @file SocketUtils.cpp
 * @brief Cross-platform implementation of socket initialization and helper operations.
 */

#include "SocketUtils.h"
#include <cstring>
#include <glog/logging.h>

namespace Communication {
namespace Platform {

SocketInitializer& SocketInitializer::instance()
{
    static SocketInitializer instance;
    return instance;
}

SocketInitializer::SocketInitializer()
{
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        LOG(ERROR) << "WSAStartup failed with error code: " << result;
    } else {
        LOG(INFO) << "Winsock initialized successfully.";
    }
#endif
}

SocketInitializer::~SocketInitializer()
{
#ifdef _WIN32
    WSACleanup();
    LOG(INFO) << "Winsock cleaned up.";
#endif
}

void closeSocketHandle(SocketHandle handle)
{
    if (handle == INVALID_SOCKET_HANDLE) {
        return;
    }

#ifdef _WIN32
    closesocket(handle);
#else
    ::close(handle);
#endif
}

bool setSocketNonBlocking(SocketHandle handle, bool nonBlocking)
{
    if (handle == INVALID_SOCKET_HANDLE) {
        return false;
    }

#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    return ioctlsocket(handle, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(handle, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    flags = nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return fcntl(handle, F_SETFL, flags) == 0;
#endif
}

bool configureSocketKeepAlive(SocketHandle handle, const TcpKeepAliveConfig& config)
{
    if (handle == INVALID_SOCKET_HANDLE) {
        return false;
    }

    int optEnable = config.enable ? 1 : 0;
    if (setsockopt(handle, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&optEnable), sizeof(optEnable)) <
        0) {
        LOG(ERROR) << "Failed to set SO_KEEPALIVE option: " << getSocketErrorString();
        return false;
    }

    if (!config.enable) {
        return true;
    }

#ifdef _WIN32
    struct tcp_keepalive keepaliveSettings { };
    keepaliveSettings.onoff = 1;
    keepaliveSettings.keepalivetime = config.keepIdleSec * 1000;
    keepaliveSettings.keepaliveinterval = config.keepIntervalSec * 1000;

    DWORD bytesReturned = 0;
    if (WSAIoctl(handle, SIO_KEEPALIVE_VALS, &keepaliveSettings, sizeof(keepaliveSettings), nullptr, 0, &bytesReturned,
                 nullptr, nullptr) != 0) {
        LOG(WARNING) << "WSAIoctl SIO_KEEPALIVE_VALS warning: " << getSocketErrorString();
    }
#else
#ifdef TCP_KEEPIDLE
    int idleSec = static_cast<int>(config.keepIdleSec);
    setsockopt(handle, IPPROTO_TCP, TCP_KEEPIDLE, &idleSec, sizeof(idleSec));
#elif defined(TCP_KEEPALIVE)
    int idleSec = static_cast<int>(config.keepIdleSec);
    setsockopt(handle, IPPROTO_TCP, TCP_KEEPALIVE, &idleSec, sizeof(idleSec));
#endif

#ifdef TCP_KEEPINTVL
    int intvlSec = static_cast<int>(config.keepIntervalSec);
    setsockopt(handle, IPPROTO_TCP, TCP_KEEPINTVL, &intvlSec, sizeof(intvlSec));
#endif

#ifdef TCP_KEEPCNT
    int cnt = static_cast<int>(config.keepCount);
    setsockopt(handle, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
#endif
#endif

    return true;
}

std::string getSocketErrorString()
{
#ifdef _WIN32
    int errCode = WSAGetLastError();
    char* errText = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                   errCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errText, 0, nullptr);
    std::string message = errText ? errText : "Unknown Winsock Error";
    if (errText) {
        LocalFree(errText);
    }
    return message;
#else
    return std::string(strerror(errno));
#endif
}

} // namespace Platform
} // namespace Communication
