/**
 * @file TcpClient.h
 * @brief Thread-safe TCP client implementation inheriting from ICommunication.
 */

#pragma once

#include "ICommunication.h"
#include "Platform/SocketUtils.h"
#include "Types.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace Communication {

/**
 * @brief High-performance, thread-safe TCP client utilizing background worker thread for async reading.
 */
class TcpClient : public ICommunication {
public:
    /**
     * @brief Constructs TcpClient with explicit configuration.
     * @param config TcpConfig containing host, port, and timeout.
     */
    explicit TcpClient(TcpConfig config = {});

    /**
     * @brief Destructor ensuring graceful connection closure and worker thread join (RAII).
     */
    ~TcpClient() override;

    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    TcpClient(TcpClient&& other) noexcept;
    TcpClient& operator=(TcpClient&& other) noexcept;

    /**
     * @brief Establishes TCP connection to remote host.
     * @return True if connected successfully, false otherwise.
     */
    bool open() override;

    /**
     * @brief Alias for open().
     */
    bool connect() override;

    /**
     * @brief Disconnects and shuts down active connection.
     */
    void close() override;

    /**
     * @brief Alias for close().
     */
    void disconnect() override;

    /**
     * @brief Transmits binary payload over socket.
     * @param data Byte vector to send.
     * @return True if all bytes sent, false otherwise.
     */
    bool send(const std::vector<uint8_t>& data) override;

    /**
     * @brief Transmits text string over socket.
     * @param data String view to send.
     * @return True if all bytes sent, false otherwise.
     */
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

    /**
     * @brief Registers connection state transition callback.
     * @param callback Function called when TCP client connects or disconnects.
     */
    void registerConnectionStateCallback(ConnectionStateCallback callback) override;

    /**
     * @brief Checks if socket is open and connected.
     * @return True if connected.
     */
    bool isOpen() const override;

    /**
     * @brief Alias for isOpen().
     */
    bool isConnected() const override;

    /**
     * @brief Gets current TCP client statistics.
     * @return ConnectionStats object.
     */
    ConnectionStats stats() const override;

    /**
     * @brief Resets TCP client statistics.
     */
    void resetStats() override;

    /**
     * @brief Updates TCP client configuration.
     * @param config New TcpConfig settings.
     */
    void setConfig(const TcpConfig& config);

    /**
     * @brief Gets current TCP client configuration.
     * @return Current TcpConfig.
     */
    TcpConfig config() const;

private:
    void receiveLoop();
    void stopReceiveThread();
    void notifyConnectionState(bool connected);
    bool openInternal();

    mutable std::mutex m_configMutex;
    TcpConfig m_config;

    mutable std::mutex m_socketMutex;
    Platform::SocketHandle m_socket {INVALID_SOCKET_HANDLE};
    std::atomic<bool> m_isConnected {false};

    mutable std::mutex m_callbackMutex;
    DataReceivedCallback m_callback;
    DataViewCallback m_viewCallback;

    mutable std::mutex m_stateCallbackMutex;
    ConnectionStateCallback m_stateCallback;

    std::condition_variable m_reconnectCv;
    std::mutex m_reconnectMutex;

    std::thread m_receiveThread;
    std::atomic<bool> m_isRunning {false};

    mutable std::mutex m_statsMutex;
    std::atomic<uint64_t> m_bytesSent {0};
    std::atomic<uint64_t> m_bytesReceived {0};
    std::atomic<uint64_t> m_packetsSent {0};
    std::atomic<uint64_t> m_packetsReceived {0};
    std::atomic<uint64_t> m_reconnectCount {0};
    std::chrono::system_clock::time_point m_connectTime {};
};

} // namespace Communication
