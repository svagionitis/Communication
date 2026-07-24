/**
 * @file TcpServer.cpp
 * @brief Thread-safe TCP Server implementation.
 */

#include "TcpServer.h"
#include <algorithm>
#include <array>
#include <glog/logging.h>

namespace Communication {

TcpServer::TcpServer(TcpConfig config)
    : m_config(std::move(config))
{
    Platform::SocketInitializer::instance();
}

TcpServer::~TcpServer()
{
    TcpServer::close();
}

bool TcpServer::open()
{
    std::lock_guard<std::mutex> lockServer(m_serverMutex);
    if (m_isOpen.load()) {
        return true;
    }

    TcpConfig cfg;
    {
        std::lock_guard<std::mutex> lockConfig(m_configMutex);
        cfg = m_config;
    }

    Platform::SocketHandle listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET_HANDLE) {
        LOG(ERROR) << "TcpServer failed to create listen socket: " << Platform::getSocketErrorString();
        return false;
    }

    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    struct sockaddr_in serverAddr { };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(cfg.port);
    if (cfg.host.empty() || cfg.host == "0.0.0.0" || cfg.host == "127.0.0.1") {
        serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        inet_pton(AF_INET, cfg.host.c_str(), &serverAddr.sin_addr);
    }

    if (bind(listenSock, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        LOG(ERROR) << "TcpServer bind failed on port " << cfg.port << ": " << Platform::getSocketErrorString();
        Platform::closeSocketHandle(listenSock);
        return false;
    }

    if (listen(listenSock, SOMAXCONN) < 0) {
        LOG(ERROR) << "TcpServer listen failed: " << Platform::getSocketErrorString();
        Platform::closeSocketHandle(listenSock);
        return false;
    }

    m_listenSocket = listenSock;
    m_isOpen.store(true);
    m_isRunning.store(true);
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_connectTime = std::chrono::system_clock::now();
    }

    m_acceptThread = std::thread(&TcpServer::acceptLoop, this);
    LOG(INFO) << "TcpServer listening on port " << cfg.port;
    return true;
}

bool TcpServer::connect()
{
    return open();
}

void TcpServer::close()
{
    stopServer();

    std::lock_guard<std::mutex> lockServer(m_serverMutex);
    if (m_listenSocket != INVALID_SOCKET_HANDLE) {
        Platform::closeSocketHandle(m_listenSocket);
        m_listenSocket = INVALID_SOCKET_HANDLE;
    }

    for (auto clientSock : m_clientSockets) {
        Platform::closeSocketHandle(clientSock);
    }
    m_clientSockets.clear();

    m_isOpen.store(false);
    LOG(INFO) << "TcpServer closed.";
}

void TcpServer::disconnect()
{
    close();
}

bool TcpServer::send(const std::vector<uint8_t>& data)
{
    if (data.empty()) {
        return true;
    }
    return send(std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
}

bool TcpServer::send(std::string_view data)
{
    std::lock_guard<std::mutex> lockServer(m_serverMutex);
    if (!m_isOpen.load() || m_clientSockets.empty()) {
        return false;
    }

    bool allSent = true;
    for (auto clientSock : m_clientSockets) {
        size_t totalSent = 0;
        size_t bytesRemaining = data.size();
        const char* ptr = data.data();

        while (bytesRemaining > 0) {
            int sent = ::send(clientSock, ptr + totalSent, static_cast<int>(bytesRemaining), 0);
            if (sent <= 0) {
                allSent = false;
                break;
            }
            totalSent += static_cast<size_t>(sent);
            bytesRemaining -= static_cast<size_t>(sent);
        }
        if (totalSent > 0) {
            m_bytesSent.fetch_add(totalSent);
            m_packetsSent.fetch_add(1);
        }
    }
    return allSent;
}

void TcpServer::registerReceiveCallback(DataReceivedCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_callback = std::move(callback);
}

void TcpServer::registerReceiveViewCallback(DataViewCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_viewCallback = std::move(callback);
}

bool TcpServer::isOpen() const
{
    return m_isOpen.load();
}

bool TcpServer::isConnected() const
{
    return isOpen();
}

ConnectionStats TcpServer::stats() const
{
    ConnectionStats s;
    s.bytesSent = m_bytesSent.load();
    s.bytesReceived = m_bytesReceived.load();
    s.packetsSent = m_packetsSent.load();
    s.packetsReceived = m_packetsReceived.load();

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        s.connectTimestamp = m_connectTime;
    }

    if (m_isOpen.load() && s.connectTimestamp != std::chrono::system_clock::time_point {}) {
        auto durationSec = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::system_clock::now() -
                                                                                     s.connectTimestamp)
                               .count();
        if (durationSec > 0.0001) {
            s.sendBytesPerSec = static_cast<double>(s.bytesSent) / durationSec;
            s.rxBytesPerSec = static_cast<double>(s.bytesReceived) / durationSec;
        }
    }
    return s;
}

void TcpServer::resetStats()
{
    m_bytesSent.store(0);
    m_bytesReceived.store(0);
    m_packetsSent.store(0);
    m_packetsReceived.store(0);
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_connectTime = std::chrono::system_clock::now();
    }
}

void TcpServer::setConfig(const TcpConfig& config)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config = config;
}

TcpConfig TcpServer::config() const
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

void TcpServer::stopServer()
{
    if (m_isRunning.exchange(false)) {
        if (m_listenSocket != INVALID_SOCKET_HANDLE) {
#ifdef _WIN32
            shutdown(m_listenSocket, SD_BOTH);
#else
            shutdown(m_listenSocket, SHUT_RDWR);
#endif
        }

        {
            std::lock_guard<std::mutex> lockServer(m_serverMutex);
            for (auto clientSock : m_clientSockets) {
                if (clientSock != INVALID_SOCKET_HANDLE) {
#ifdef _WIN32
                    shutdown(clientSock, SD_BOTH);
#else
                    shutdown(clientSock, SHUT_RDWR);
#endif
                }
            }
        }

        if (m_acceptThread.joinable()) {
            m_acceptThread.join();
        }

        for (auto& clientThread : m_clientThreads) {
            if (clientThread.joinable()) {
                clientThread.join();
            }
        }
        m_clientThreads.clear();
    }
}

void TcpServer::acceptLoop()
{
    while (m_isRunning.load()) {
        struct sockaddr_in clientAddr { };
        socklen_t clientLen = sizeof(clientAddr);
        Platform::SocketHandle clientSock =
            accept(m_listenSocket, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);

        if (clientSock == INVALID_SOCKET_HANDLE) {
            if (!m_isRunning.load()) {
                break;
            }
            continue;
        }

        if (!m_isRunning.load()) {
            Platform::closeSocketHandle(clientSock);
            break;
        }

        TcpConfig cfg;
        {
            std::lock_guard<std::mutex> lockConfig(m_configMutex);
            cfg = m_config;
        }
        Platform::configureSocketKeepAlive(clientSock, cfg.keepAlive);

        {
            std::lock_guard<std::mutex> lock(m_serverMutex);
            m_clientSockets.push_back(clientSock);
        }

        m_clientThreads.emplace_back(&TcpServer::clientReceiveLoop, this, clientSock);
    }
}

void TcpServer::clientReceiveLoop(Platform::SocketHandle clientSock)
{
    std::array<uint8_t, 4096> buffer;

    while (m_isRunning.load()) {
        int bytesRead = ::recv(clientSock, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);

        if (bytesRead > 0) {
            m_bytesReceived.fetch_add(static_cast<uint64_t>(bytesRead));
            m_packetsReceived.fetch_add(1);
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
            break;
        }
    }

    std::lock_guard<std::mutex> lock(m_serverMutex);
    m_clientSockets.erase(std::remove(m_clientSockets.begin(), m_clientSockets.end(), clientSock),
                          m_clientSockets.end());
    Platform::closeSocketHandle(clientSock);
}

} // namespace Communication
