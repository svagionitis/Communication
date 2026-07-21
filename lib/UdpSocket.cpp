/**
 * @file UdpSocket.cpp
 * @brief Thread-safe implementation of UDP Socket.
 */

#include "UdpSocket.h"
#include <array>
#include <glog/logging.h>

namespace Communication {

UdpSocket::UdpSocket(UdpConfig config)
    : m_config(std::move(config))
{
    Platform::SocketInitializer::instance();
}

UdpSocket::~UdpSocket()
{
    close();
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
{
    std::lock_guard<std::mutex> lockOtherSocket(other.m_socketMutex);
    std::lock_guard<std::mutex> lockOtherConfig(other.m_configMutex);

    other.stopReceiveThread();

    m_config = std::move(other.m_config);
    m_socket = other.m_socket;
    m_isOpen.store(other.m_isOpen.load());

    other.m_socket = INVALID_SOCKET_HANDLE;
    other.m_isOpen.store(false);

    {
        std::lock_guard<std::mutex> lockOtherCb(other.m_callbackMutex);
        m_callback = std::move(other.m_callback);
    }

    if (m_isOpen.load()) {
        m_isRunning.store(true);
        m_receiveThread = std::thread(&UdpSocket::receiveLoop, this);
    }
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept
{
    if (this != &other) {
        close();

        std::lock_guard<std::mutex> lockOtherSocket(other.m_socketMutex);
        std::lock_guard<std::mutex> lockOtherConfig(other.m_configMutex);

        other.stopReceiveThread();

        m_config = std::move(other.m_config);
        m_socket = other.m_socket;
        m_isOpen.store(other.m_isOpen.load());

        other.m_socket = INVALID_SOCKET_HANDLE;
        other.m_isOpen.store(false);

        {
            std::lock_guard<std::mutex> lockOtherCb(other.m_callbackMutex);
            m_callback = std::move(other.m_callback);
        }

        if (m_isOpen.load()) {
            m_isRunning.store(true);
            m_receiveThread = std::thread(&UdpSocket::receiveLoop, this);
        }
    }
    return *this;
}

bool UdpSocket::open()
{
    std::lock_guard<std::mutex> lockSocket(m_socketMutex);
    if (m_isOpen.load()) {
        return true;
    }

    UdpConfig cfg;
    {
        std::lock_guard<std::mutex> lockConfig(m_configMutex);
        cfg = m_config;
    }

    Platform::SocketHandle sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET_HANDLE) {
        LOG(ERROR) << "UdpSocket failed to create socket: " << Platform::getSocketErrorString();
        return false;
    }

    if (cfg.enableBroadcast) {
        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&opt), sizeof(opt));
    }

    int optReuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&optReuse), sizeof(optReuse));

    struct sockaddr_in bindAddr { };
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(cfg.localPort);
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, reinterpret_cast<struct sockaddr*>(&bindAddr), sizeof(bindAddr)) < 0) {
        LOG(ERROR) << "UdpSocket bind failed on local port " << cfg.localPort << ": "
                   << Platform::getSocketErrorString();
        Platform::closeSocketHandle(sock);
        return false;
    }

    m_socket = sock;
    m_isOpen.store(true);
    m_isRunning.store(true);

    m_receiveThread = std::thread(&UdpSocket::receiveLoop, this);
    LOG(INFO) << "UdpSocket opened successfully on local port " << cfg.localPort;
    return true;
}

bool UdpSocket::connect()
{
    return open();
}

void UdpSocket::close()
{
    stopReceiveThread();

    std::lock_guard<std::mutex> lockSocket(m_socketMutex);
    if (m_socket != INVALID_SOCKET_HANDLE) {
        Platform::closeSocketHandle(m_socket);
        m_socket = INVALID_SOCKET_HANDLE;
    }
    m_isOpen.store(false);
    LOG(INFO) << "UdpSocket closed.";
}

void UdpSocket::disconnect()
{
    close();
}

bool UdpSocket::send(const std::vector<uint8_t>& data)
{
    if (data.empty()) {
        return true;
    }
    return send(std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
}

bool UdpSocket::send(std::string_view data)
{
    UdpConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        cfg = m_config;
    }
    return sendTo(std::vector<uint8_t>(data.begin(), data.end()), cfg.remoteHost, cfg.remotePort);
}

bool UdpSocket::sendTo(const std::vector<uint8_t>& data, const std::string& host, uint16_t port)
{
    std::lock_guard<std::mutex> lockSocket(m_socketMutex);
    if (!m_isOpen.load() || m_socket == INVALID_SOCKET_HANDLE) {
        LOG(ERROR) << "UdpSocket sendTo failed: socket is closed.";
        return false;
    }

    struct sockaddr_in destAddr { };
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &destAddr.sin_addr);

    int sent = ::sendto(m_socket, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0,
                        reinterpret_cast<struct sockaddr*>(&destAddr), sizeof(destAddr));
    if (sent < 0) {
        LOG(ERROR) << "UdpSocket sendTo error: " << Platform::getSocketErrorString();
        return false;
    }
    return true;
}

void UdpSocket::registerReceiveCallback(DataReceivedCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_callback = std::move(callback);
}

void UdpSocket::registerReceiveViewCallback(DataViewCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_viewCallback = std::move(callback);
}

bool UdpSocket::isOpen() const
{
    return m_isOpen.load();
}

bool UdpSocket::isConnected() const
{
    return isOpen();
}

void UdpSocket::setConfig(const UdpConfig& config)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config = config;
}

UdpConfig UdpSocket::config() const
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

void UdpSocket::stopReceiveThread()
{
    if (m_isRunning.exchange(false)) {
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

void UdpSocket::receiveLoop()
{
    std::array<uint8_t, 65536> buffer;

    while (m_isRunning.load()) {
        Platform::SocketHandle sockHandle;
        {
            std::lock_guard<std::mutex> lock(m_socketMutex);
            sockHandle = m_socket;
        }

        if (sockHandle == INVALID_SOCKET_HANDLE) {
            break;
        }

        struct sockaddr_in srcAddr { };
        socklen_t srcLen = sizeof(srcAddr);

        int bytesRead = ::recvfrom(sockHandle, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()),
                                   0, reinterpret_cast<struct sockaddr*>(&srcAddr), &srcLen);

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
            if (!m_isRunning.load()) {
                break;
            }
        }
    }
}

} // namespace Communication
