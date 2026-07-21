/**
 * @file SerialPort.h
 * @brief Thread-safe cross-platform Serial Port implementation inheriting from ICommunication.
 */

#pragma once

#include "ICommunication.h"
#include "Types.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
typedef HANDLE SerialHandle;
#define INVALID_SERIAL_HANDLE INVALID_HANDLE_VALUE
#else
typedef int SerialHandle;
#define INVALID_SERIAL_HANDLE (-1)
#endif

namespace Communication {

/**
 * @brief Cross-platform, thread-safe Serial Port implementation using Win32 API on Windows and termios on Linux.
 */
class SerialPort : public ICommunication {
public:
    explicit SerialPort(SerialConfig config = {});
    ~SerialPort() override;

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    SerialPort(SerialPort&& other) noexcept;
    SerialPort& operator=(SerialPort&& other) noexcept;

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

    using ICommunication::registerReceiveViewCallback;
    /**
     * @brief Registers zero-copy raw buffer receive callback function.
     * @param callback Function called upon receiving data payload with raw pointer and size.
     */
    void registerReceiveViewCallback(DataViewCallback callback) override;

    /**
     * @brief Registers connection state transition callback.
     * @param callback Function called when Serial port opens or closes.
     */
    void registerConnectionStateCallback(ConnectionStateCallback callback) override;

    /**
     * @brief Checks if serial port is open.
     * @return True if open.
     */
    bool isOpen() const override;

    /**
     * @brief Alias for isOpen().
     */
    bool isConnected() const override;

    /**
     * @brief Updates Serial port configuration.
     * @param config New SerialConfig settings.
     */
    void setConfig(const SerialConfig& config);

    /**
     * @brief Gets current Serial port configuration.
     * @return Current SerialConfig.
     */
    SerialConfig config() const;

private:
    void receiveLoop();
    void stopReceiveThread();
    bool configurePlatformPort();
    void notifyConnectionState(bool connected);
    bool openInternal();

    mutable std::mutex m_configMutex;
    SerialConfig m_config;

    mutable std::mutex m_portMutex;
    SerialHandle m_handle {INVALID_SERIAL_HANDLE};
    std::atomic<bool> m_isOpen {false};

    mutable std::mutex m_callbackMutex;
    DataReceivedCallback m_callback;
    DataViewCallback m_viewCallback;

    mutable std::mutex m_stateCallbackMutex;
    ConnectionStateCallback m_stateCallback;

    std::condition_variable m_reconnectCv;
    std::mutex m_reconnectMutex;

    std::thread m_receiveThread;
    std::atomic<bool> m_isRunning {false};
};

} // namespace Communication
