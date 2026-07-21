/**
 * @file Types.h
 * @brief Common types, configuration structs, enums, and variants for Communication library.
 */

#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace Communication {

/**
 * @brief Comprehensive enumeration of all standard supported serial baud rates.
 */
enum class BaudRate : uint32_t {
    B110 = 110,
    B300 = 300,
    B600 = 600,
    B1200 = 1200,
    B2400 = 2400,
    B4800 = 4800,
    B9600 = 9600,
    B14400 = 14400,
    B19200 = 19200,
    B38400 = 38400,
    B57600 = 57600,
    B115200 = 115200,
    B128000 = 128000,
    B230400 = 230400,
    B256000 = 256000,
    B460800 = 460800,
    B500000 = 500000,
    B576000 = 576000,
    B921600 = 921600,
    B1000000 = 1000000,
    B1152000 = 1152000,
    B1500000 = 1500000,
    B2000000 = 2000000,
    B2500000 = 2500000,
    B3000000 = 3000000,
    B3500000 = 3500000,
    B4000000 = 4000000
};

/**
 * @brief Serial parity setting.
 */
enum class Parity { None, Odd, Even, Mark, Space };

/**
 * @brief Serial stop bits setting.
 */
enum class StopBits { One, OnePointFive, Two };

/**
 * @brief Serial data bits setting.
 */
enum class DataBits { Five = 5, Six = 6, Seven = 7, Eight = 8 };

/**
 * @brief Serial flow control setting.
 */
enum class FlowControl {
    None,
    Hardware, // RTS/CTS
    Software  // XON/XOFF
};

/**
 * @brief Configuration parameters for native TCP keep-alive socket options.
 */
struct TcpKeepAliveConfig {
    bool enable {true};
    uint32_t keepIdleSec {60};
    uint32_t keepIntervalSec {5};
    uint32_t keepCount {3};
};

/**
 * @brief Configuration parameters for TCP client/server connections.
 */
struct TcpConfig {
    std::string host {"127.0.0.1"};
    uint16_t port {8080};
    uint32_t timeoutMs {3000};
    TcpKeepAliveConfig keepAlive;
};

/**
 * @brief Configuration parameters for UDP sockets.
 */
struct UdpConfig {
    std::string remoteHost {"127.0.0.1"};
    uint16_t remotePort {8080};
    uint16_t localPort {0}; // 0 for auto-assign
    bool enableBroadcast {false};
};

/**
 * @brief Configuration parameters for Serial communication.
 */
struct SerialConfig {
    std::string portName {"/dev/ttyUSB0"}; // e.g. "COM1" or "/dev/ttyUSB0"
    BaudRate baudRate {BaudRate::B115200};
    DataBits dataBits {DataBits::Eight};
    Parity parity {Parity::None};
    StopBits stopBits {StopBits::One};
    FlowControl flowControl {FlowControl::None};
    uint32_t timeoutMs {1000};
};

/**
 * @brief Unified configuration variant for polymorphic configuration.
 */
using CommunicationConfig = std::variant<TcpConfig, UdpConfig, SerialConfig>;

} // namespace Communication
