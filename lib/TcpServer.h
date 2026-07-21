/**
 * @file TcpServer.h
 * @brief Thread-safe TCP Server for handling incoming TCP connections.
 */

#pragma once

#include "ICommunication.h"
#include "Platform/SocketUtils.h"
#include "Types.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace Communication {

/**
 * @brief Multi-client thread-safe TCP server implementing ICommunication contract.
 */
class TcpServer : public ICommunication {
public:
    explicit TcpServer(TcpConfig config = {});
    ~TcpServer() override;

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    bool open() override;
    bool connect() override;
    void close() override;
    void disconnect() override;

    bool send(const std::vector<uint8_t>& data) override;
    bool send(std::string_view data) override;

    /**
     * @brief Registers receive callback function.
     * @param callback Function called upon receiving data payload.
     */
    void registerReceiveCallback(DataReceivedCallback callback) override;

    /**
     * @brief Registers zero-copy raw buffer receive callback function.
     * @param callback Function called upon receiving data payload with raw pointer and size.
     */
    using ICommunication::registerReceiveViewCallback;
    void registerReceiveViewCallback(DataViewCallback callback) override;

    bool isOpen() const override;
    bool isConnected() const override;

    ConnectionStats stats() const override;
    void resetStats() override;

    void setConfig(const TcpConfig& config);
    TcpConfig config() const;

private:
    void acceptLoop();
    void clientReceiveLoop(Platform::SocketHandle clientSock);
    void stopServer();

    mutable std::mutex m_configMutex;
    TcpConfig m_config;

    mutable std::mutex m_serverMutex;
    Platform::SocketHandle m_listenSocket {INVALID_SOCKET_HANDLE};
    std::vector<Platform::SocketHandle> m_clientSockets;

    std::atomic<bool> m_isOpen {false};
    std::atomic<bool> m_isRunning {false};

    mutable std::mutex m_callbackMutex;
    DataReceivedCallback m_callback;
    DataViewCallback m_viewCallback;

    std::thread m_acceptThread;
    std::vector<std::thread> m_clientThreads;

    mutable std::mutex m_statsMutex;
    std::atomic<uint64_t> m_bytesSent {0};
    std::atomic<uint64_t> m_bytesReceived {0};
    std::atomic<uint64_t> m_packetsSent {0};
    std::atomic<uint64_t> m_packetsReceived {0};
    std::chrono::system_clock::time_point m_connectTime {};
};

} // namespace Communication
