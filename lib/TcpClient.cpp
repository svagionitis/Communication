/**
 * @file TcpClient.cpp
 * @brief Thread-safe implementation of TCP Client operations using native OS sockets.
 */

#include "TcpClient.h"
#include <array>
#include <cmath>
#include <cstring>
#include <glog/logging.h>

namespace Communication {

TcpClient::TcpClient(TcpConfig config)
    : m_config(std::move(config))
{
    Platform::SocketInitializer::instance();
}

TcpClient::~TcpClient()
{
    close();
}

TcpClient::TcpClient(TcpClient&& other) noexcept
{
    std::lock_guard<std::mutex> lockOtherSocket(other.m_socketMutex);
    std::lock_guard<std::mutex> lockOtherConfig(other.m_configMutex);

    other.stopReceiveThread();

    m_config = std::move(other.m_config);
    m_socket = other.m_socket;
    m_isConnected.store(other.m_isConnected.load());

    other.m_socket = INVALID_SOCKET_HANDLE;
    other.m_isConnected.store(false);

    {
        std::lock_guard<std::mutex> lockOtherCb(other.m_callbackMutex);
        m_callback = std::move(other.m_callback);
    }

    if (m_isConnected.load()) {
        m_isRunning.store(true);
        m_receiveThread = std::thread(&TcpClient::receiveLoop, this);
    }
}

TcpClient& TcpClient::operator=(TcpClient&& other) noexcept
{
    if (this != &other) {
        close();

        std::lock_guard<std::mutex> lockOtherSocket(other.m_socketMutex);
        std::lock_guard<std::mutex> lockOtherConfig(other.m_configMutex);

        other.stopReceiveThread();

        m_config = std::move(other.m_config);
        m_socket = other.m_socket;
        m_isConnected.store(other.m_isConnected.load());

        other.m_socket = INVALID_SOCKET_HANDLE;
        other.m_isConnected.store(false);

        {
            std::lock_guard<std::mutex> lockOtherCb(other.m_callbackMutex);
            m_callback = std::move(other.m_callback);
        }

        if (m_isConnected.load()) {
            m_isRunning.store(true);
            m_receiveThread = std::thread(&TcpClient::receiveLoop, this);
        }
    }
    return *this;
}

void TcpClient::registerConnectionStateCallback(ConnectionStateCallback callback)
{
    std::lock_guard<std::mutex> lock(m_stateCallbackMutex);
    m_stateCallback = std::move(callback);
}

void TcpClient::notifyConnectionState(bool connected)
{
    ConnectionStateCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_stateCallbackMutex);
        cb = m_stateCallback;
    }
    if (cb) {
        cb(connected);
    }
}

bool TcpClient::openInternal()
{
    std::lock_guard<std::mutex> lockSocket(m_socketMutex);
    if (m_socket != INVALID_SOCKET_HANDLE) {
        Platform::closeSocketHandle(m_socket);
        m_socket = INVALID_SOCKET_HANDLE;
    }

    TcpConfig cfg;
    {
        std::lock_guard<std::mutex> lockConfig(m_configMutex);
        cfg = m_config;
    }

    struct addrinfo hints { };
    struct addrinfo* res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string portStr = std::to_string(cfg.port);
    int status = getaddrinfo(cfg.host.c_str(), portStr.c_str(), &hints, &res);
    if (status != 0 || !res) {
        LOG(ERROR) << "TcpClient getaddrinfo failed for " << cfg.host << ":" << cfg.port;
        return false;
    }

    Platform::SocketHandle sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET_HANDLE) {
        LOG(ERROR) << "TcpClient failed to create socket: " << Platform::getSocketErrorString();
        freeaddrinfo(res);
        return false;
    }

    Platform::configureSocketKeepAlive(sock, cfg.keepAlive);
    Platform::setSocketNonBlocking(sock, true);

    int connectRes = ::connect(sock, res->ai_addr, static_cast<socklen_t>(res->ai_addrlen));
    freeaddrinfo(res);

    if (connectRes < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
            LOG(ERROR) << "TcpClient connect error: " << Platform::getSocketErrorString();
            Platform::closeSocketHandle(sock);
            return false;
        }
#else
        if (errno != EINPROGRESS) {
            LOG(ERROR) << "TcpClient connect error: " << Platform::getSocketErrorString();
            Platform::closeSocketHandle(sock);
            return false;
        }
#endif

        fd_set writeFds;
        FD_ZERO(&writeFds);
        FD_SET(sock, &writeFds);

        struct timeval timeout { };
        timeout.tv_sec = cfg.timeoutMs / 1000;
        timeout.tv_usec = (cfg.timeoutMs % 1000) * 1000;

        int selRes = select(static_cast<int>(sock + 1), nullptr, &writeFds, nullptr, &timeout);
        if (selRes <= 0) {
            LOG(ERROR) << "TcpClient connection timed out or select failed.";
            Platform::closeSocketHandle(sock);
            return false;
        }

        int sockErr = 0;
        socklen_t len = sizeof(sockErr);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&sockErr), &len);
        if (sockErr != 0) {
            LOG(ERROR) << "TcpClient socket error on non-blocking connect: " << sockErr;
            Platform::closeSocketHandle(sock);
            return false;
        }
    }

    Platform::setSocketNonBlocking(sock, false);

    m_socket = sock;
    m_isConnected.store(true);
    LOG(INFO) << "TcpClient successfully connected to " << cfg.host << ":" << cfg.port;
    return true;
}

bool TcpClient::open()
{
    if (m_isConnected.load()) {
        LOG(INFO) << "TcpClient already connected.";
        return true;
    }

    if (!openInternal()) {
        return false;
    }

    m_isRunning.store(true);
    m_receiveThread = std::thread(&TcpClient::receiveLoop, this);
    notifyConnectionState(true);
    return true;
}

bool TcpClient::connect()
{
    return open();
}

void TcpClient::close()
{
    stopReceiveThread();

    std::lock_guard<std::mutex> lockSocket(m_socketMutex);
    if (m_socket != INVALID_SOCKET_HANDLE) {
        Platform::closeSocketHandle(m_socket);
        m_socket = INVALID_SOCKET_HANDLE;
    }
    bool wasConnected = m_isConnected.exchange(false);
    if (wasConnected) {
        notifyConnectionState(false);
    }
    LOG(INFO) << "TcpClient disconnected and closed.";
}

void TcpClient::disconnect()
{
    close();
}

bool TcpClient::send(const std::vector<uint8_t>& data)
{
    if (data.empty()) {
        return true;
    }
    return send(std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
}

bool TcpClient::send(std::string_view data)
{
    std::lock_guard<std::mutex> lockSocket(m_socketMutex);
    if (!m_isConnected.load() || m_socket == INVALID_SOCKET_HANDLE) {
        LOG(ERROR) << "TcpClient send failed: Socket is not connected.";
        return false;
    }

    size_t totalSent = 0;
    size_t bytesRemaining = data.size();
    const char* ptr = data.data();

    while (bytesRemaining > 0) {
        int sent = ::send(m_socket, ptr + totalSent, static_cast<int>(bytesRemaining), 0);
        if (sent <= 0) {
            LOG(ERROR) << "TcpClient send error: " << Platform::getSocketErrorString();
            return false;
        }
        totalSent += static_cast<size_t>(sent);
        bytesRemaining -= static_cast<size_t>(sent);
    }
    return true;
}

void TcpClient::registerReceiveCallback(DataReceivedCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_callback = std::move(callback);
}

void TcpClient::registerReceiveViewCallback(DataViewCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_viewCallback = std::move(callback);
}

bool TcpClient::isOpen() const
{
    return m_isConnected.load();
}

bool TcpClient::isConnected() const
{
    return isOpen();
}

void TcpClient::setConfig(const TcpConfig& config)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config = config;
}

TcpConfig TcpClient::config() const
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

void TcpClient::stopReceiveThread()
{
    if (m_isRunning.exchange(false)) {
        m_reconnectCv.notify_all();
        if (m_socket != INVALID_SOCKET_HANDLE) {
#ifdef _WIN32
            shutdown(m_socket, SD_BOTH);
#else
            shutdown(m_socket, SHUT_RDWR);
#endif
        }
        if (m_receiveThread.joinable()) {
            m_receiveThread.join();
        }
    }
}

void TcpClient::receiveLoop()
{
    std::array<uint8_t, 4096> buffer;

    while (m_isRunning.load()) {
        Platform::SocketHandle sockHandle;
        {
            std::lock_guard<std::mutex> lock(m_socketMutex);
            sockHandle = m_socket;
        }

        if (sockHandle == INVALID_SOCKET_HANDLE) {
            break;
        }

        int bytesRead = ::recv(sockHandle, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);

        if (bytesRead > 0) {
            DataReceivedCallback cb;
            DataViewCallback vcb;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_callback;
                vcb = m_viewCallback;
            }
            if (vcb) {
                vcb(buffer.data(), static_cast<size_t>(bytesRead));
            }
            if (cb) {
                std::vector<uint8_t> data(buffer.begin(), buffer.begin() + bytesRead);
                cb(data);
            }
        } else {
            if (bytesRead == 0) {
                LOG(INFO) << "TcpClient connection closed by remote server.";
            } else {
                if (!m_isRunning.load()) {
                    break;
                }
                LOG(ERROR) << "TcpClient recv error: " << Platform::getSocketErrorString();
            }

            m_isConnected.store(false);
            notifyConnectionState(false);

            TcpConfig cfg;
            {
                std::lock_guard<std::mutex> lockConfig(m_configMutex);
                cfg = m_config;
            }

            if (!cfg.autoReconnect.enable || !m_isRunning.load()) {
                break;
            }

            uint32_t attempt = 0;
            bool reconnected = false;

            while (m_isRunning.load()) {
                attempt++;
                if (cfg.autoReconnect.maxRetries > 0 && attempt > cfg.autoReconnect.maxRetries) {
                    LOG(WARNING) << "TcpClient max reconnect retries reached (" << cfg.autoReconnect.maxRetries << ").";
                    break;
                }

                double delay = cfg.autoReconnect.initialDelayMs *
                               std::pow(cfg.autoReconnect.backoffMultiplier, static_cast<double>(attempt - 1));
                uint32_t delayMs =
                    static_cast<uint32_t>(std::min(static_cast<double>(cfg.autoReconnect.maxDelayMs), delay));
                LOG(INFO) << "TcpClient reconnect attempt " << attempt << " in " << delayMs << " ms...";

                std::unique_lock<std::mutex> lockRec(m_reconnectMutex);
                if (m_reconnectCv.wait_for(lockRec, std::chrono::milliseconds(delayMs),
                                           [this] { return !m_isRunning.load(); })) {
                    break;
                }

                if (openInternal()) {
                    LOG(INFO) << "TcpClient successfully reconnected on attempt " << attempt;
                    notifyConnectionState(true);
                    reconnected = true;
                    break;
                }
            }

            if (!reconnected) {
                break;
            }
        }
    }
}

} // namespace Communication
