/**
 * @file SerialPort.h
 * @brief Thread-safe cross-platform Serial Port implementation inheriting from ICommunication.
 */

#pragma once

#include "ICommunication.h"
#include "Types.h"
#include <atomic>
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

    void registerReceiveCallback(DataReceivedCallback callback) override;

    bool isOpen() const override;
    bool isConnected() const override;

    void setConfig(const SerialConfig& config);
    SerialConfig config() const;

private:
    void receiveLoop();
    void stopReceiveThread();

    bool configurePlatformPort();

    mutable std::mutex m_configMutex;
    SerialConfig m_config;

    mutable std::mutex m_portMutex;
    SerialHandle m_handle {INVALID_SERIAL_HANDLE};
    std::atomic<bool> m_isOpen {false};

    mutable std::mutex m_callbackMutex;
    DataReceivedCallback m_callback;

    std::thread m_receiveThread;
    std::atomic<bool> m_isRunning {false};
};

} // namespace Communication
