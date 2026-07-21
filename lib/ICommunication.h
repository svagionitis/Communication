/**
 * @file ICommunication.h
 * @brief Abstract baseline interface for unified communication channels.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace Communication {

/**
 * @brief Receive callback function signature.
 */
using DataReceivedCallback = std::function<void(const std::vector<uint8_t>& data)>;

/**
 * @brief Connection state callback function signature.
 */
using ConnectionStateCallback = std::function<void(bool connected)>;

/**
 * @brief Interface defining the unified API contract for all communication channels (TCP, UDP, Serial).
 *
 * Enforces pure virtual operations for opening/connecting, closing/disconnecting,
 * sending binary/text payloads, registering asynchronous receive callbacks, and status checking.
 */
class ICommunication {
public:
    /**
     * @brief Virtual destructor to ensure proper RAII cleanup in derived classes.
     */
    virtual ~ICommunication() = default;

    /**
     * @brief Opens or establishes the communication channel.
     * @return True if opened successfully, false otherwise.
     */
    virtual bool open() = 0;

    /**
     * @brief Alias for open() to support client-oriented naming convention.
     * @return True if connected successfully, false otherwise.
     */
    virtual bool connect() = 0;

    /**
     * @brief Closes or disconnects the communication channel gracefully.
     */
    virtual void close() = 0;

    /**
     * @brief Alias for close() to support client-oriented naming convention.
     */
    virtual void disconnect() = 0;

    /**
     * @brief Sends binary data over the active channel.
     * @param data Vector containing byte sequence to send.
     * @return True if sent successfully, false otherwise.
     */
    virtual bool send(const std::vector<uint8_t>& data) = 0;

    /**
     * @brief Sends text data over the active channel.
     * @param data String view representing data payload to send.
     * @return True if sent successfully, false otherwise.
     */
    virtual bool send(std::string_view data) = 0;

    /**
     * @brief Registers an asynchronous receive callback.
     * @param callback Function to invoke when new data arrives.
     */
    virtual void registerReceiveCallback(DataReceivedCallback callback) = 0;

    /**
     * @brief Registers a connection state transition callback.
     * @param callback Function to invoke when connection state changes.
     */
    virtual void registerConnectionStateCallback(ConnectionStateCallback callback) { (void)callback; }

    /**
     * @brief Checks whether the channel is currently open and operational.
     * @return True if open, false otherwise.
     */
    virtual bool isOpen() const = 0;

    /**
     * @brief Alias for isOpen() to support client-oriented naming convention.
     * @return True if connected, false otherwise.
     */
    virtual bool isConnected() const = 0;
};

} // namespace Communication
