/**
 * @file TcpClient.h
 * @brief Thread-safe TCP client implementation inheriting from ICommunication.
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
     * @brief Checks if socket is open and connected.
     * @return True if connected.
     */
    bool isOpen() const override;

    /**
     * @brief Alias for isOpen().
     */
    bool isConnected() const override;

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

    mutable std::mutex m_configMutex;
    TcpConfig m_config;

    mutable std::mutex m_socketMutex;
    Platform::SocketHandle m_socket {INVALID_SOCKET_HANDLE};
    std::atomic<bool> m_isConnected {false};

    mutable std::mutex m_callbackMutex;
    DataReceivedCallback m_callback;

    std::thread m_receiveThread;
    std::atomic<bool> m_isRunning {false};
};

} // namespace Communication
