/**
 * @file DuplexBridge.h
 * @brief High-performance full-duplex Serial-to-TCP bridge engine.
 */

#pragma once

#include "GatewayFrame.h"
#include "LockFreeRingBuffer.h"
#include "SerialPort.h"
#include "TcpClient.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>

namespace Communication {

/**
 * @brief Full-duplex Serial <-> TCP Gateway Bridge metrics summary.
 */
struct BridgeStats {
    uint64_t serialToTcpBytes {0};
    uint64_t serialToTcpPackets {0};
    uint64_t tcpToSerialBytes {0};
    uint64_t tcpToSerialPackets {0};
    uint64_t droppedSerialToTcp {0};
    uint64_t droppedTcpToSerial {0};
};

class DuplexBridge {
public:
    DuplexBridge(SerialConfig serialConfig = {}, TcpConfig tcpConfig = {});
    ~DuplexBridge();

    bool start();
    void stop();

    BridgeStats runSimulation(std::size_t messageCount = 100000);

    BridgeStats stats() const;

private:
    void serialToTcpWorker();
    void tcpToSerialWorker();

    SerialConfig m_serialConfig;
    TcpConfig m_tcpConfig;

    SerialPort m_serialPort;
    TcpClient m_tcpClient;

    std::unique_ptr<LockFreeRingBuffer<GatewayFrame, 4096>> m_serialToTcpRing;
    std::unique_ptr<LockFreeRingBuffer<GatewayFrame, 4096>> m_tcpToSerialRing;

    std::atomic<bool> m_running {false};

    std::atomic<uint64_t> m_serialToTcpBytes {0};
    std::atomic<uint64_t> m_serialToTcpPackets {0};
    std::atomic<uint64_t> m_tcpToSerialBytes {0};
    std::atomic<uint64_t> m_tcpToSerialPackets {0};
    std::atomic<uint64_t> m_droppedSerialToTcp {0};
    std::atomic<uint64_t> m_droppedTcpToSerial {0};

    std::thread m_serialToTcpThread;
    std::thread m_tcpToSerialThread;
};

} // namespace Communication
