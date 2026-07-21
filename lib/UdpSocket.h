/**
 * @file UdpSocket.h
 * @brief Thread-safe UDP Socket implementation inheriting from ICommunication.
 */

#pragma once

#include "ICommunication.h"
#include "Platform/SocketUtils.h"
#include "Types.h"
#include <atomic>
#include <mutex>
#include <thread>

namespace Communication {

/**
 * @brief Thread-safe UDP Socket supporting unicast, broadcast, and asynchronous background reception.
 */
class UdpSocket : public ICommunication {
public:
    explicit UdpSocket(UdpConfig config = {});
    ~UdpSocket() override;

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

    bool open() override;
    bool connect() override;
    void close() override;
    void disconnect() override;

    bool send(const std::vector<uint8_t>& data) override;
    bool send(std::string_view data) override;

    /**
     * @brief Sends data to a specific remote host and port.
     * @param data Data payload.
     * @param host Remote host address.
     * @param port Remote port.
     * @return True if sent successfully.
     */
    bool sendTo(const std::vector<uint8_t>& data, const std::string& host, uint16_t port);

    void registerReceiveCallback(DataReceivedCallback callback) override;

    using ICommunication::registerReceiveViewCallback;
    void registerReceiveViewCallback(DataViewCallback callback) override;

    bool isOpen() const override;
    bool isConnected() const override;

    void setConfig(const UdpConfig& config);
    UdpConfig config() const;

private:
    void receiveLoop();
    void stopReceiveThread();

    mutable std::mutex m_configMutex;
    UdpConfig m_config;

    mutable std::mutex m_socketMutex;
    Platform::SocketHandle m_socket {INVALID_SOCKET_HANDLE};
    std::atomic<bool> m_isOpen {false};

    mutable std::mutex m_callbackMutex;
    DataReceivedCallback m_callback;
    DataViewCallback m_viewCallback;

    std::thread m_receiveThread;
    std::atomic<bool> m_isRunning {false};
};

} // namespace Communication
