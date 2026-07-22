/**
 * @file DuplexBridge.cpp
 * @brief Full-duplex Serial-to-TCP gateway bridge engine implementation.
 */

#include "DuplexBridge.h"

#include <chrono>
#include <cstring>
#include <glog/logging.h>
#include <iomanip>
#include <iostream>

namespace Communication {

DuplexBridge::DuplexBridge(SerialConfig serialConfig, TcpConfig tcpConfig)
    : m_serialConfig(serialConfig)
    , m_tcpConfig(tcpConfig)
    , m_serialPort(serialConfig)
    , m_tcpClient(tcpConfig)
    , m_serialToTcpRing(std::make_unique<LockFreeRingBuffer<GatewayFrame, 4096>>())
    , m_tcpToSerialRing(std::make_unique<LockFreeRingBuffer<GatewayFrame, 4096>>())
{
}

DuplexBridge::~DuplexBridge()
{
    stop();
}

bool DuplexBridge::start()
{
    stop();

    m_serialPort.registerReceiveViewCallback([this](const uint8_t* data, std::size_t size) {
        std::size_t remaining = size;
        std::size_t offset = 0;
        while (remaining > 0) {
            GatewayFrame frame {};
            frame.size = (remaining > sizeof(frame.data)) ? sizeof(frame.data) : remaining;
            std::memcpy(frame.data, data + offset, frame.size);

            if (!m_serialToTcpRing->push(frame)) {
                m_droppedSerialToTcp.fetch_add(1, std::memory_order_relaxed);
            }

            remaining -= frame.size;
            offset += frame.size;
        }
    });

    m_tcpClient.registerReceiveViewCallback([this](const uint8_t* data, std::size_t size) {
        std::size_t remaining = size;
        std::size_t offset = 0;
        while (remaining > 0) {
            GatewayFrame frame {};
            frame.size = (remaining > sizeof(frame.data)) ? sizeof(frame.data) : remaining;
            std::memcpy(frame.data, data + offset, frame.size);

            if (!m_tcpToSerialRing->push(frame)) {
                m_droppedTcpToSerial.fetch_add(1, std::memory_order_relaxed);
            }

            remaining -= frame.size;
            offset += frame.size;
        }
    });

    LOG(INFO) << "Opening Serial Port " << m_serialConfig.portName << " at baud "
              << static_cast<uint32_t>(m_serialConfig.baudRate);
    if (!m_serialPort.open()) {
        LOG(ERROR) << "Failed to open Serial port: " << m_serialConfig.portName;
        return false;
    }

    LOG(INFO) << "Connecting TCP Client to " << m_tcpConfig.host << ":" << m_tcpConfig.port;
    if (!m_tcpClient.open()) {
        LOG(ERROR) << "Failed to connect TCP client to " << m_tcpConfig.host << ":" << m_tcpConfig.port;
        m_serialPort.close();
        return false;
    }

    m_running.store(true, std::memory_order_release);

    m_serialToTcpThread = std::thread(&DuplexBridge::serialToTcpWorker, this);
    m_tcpToSerialThread = std::thread(&DuplexBridge::tcpToSerialWorker, this);

    LOG(INFO) << "Full-duplex gateway bridge successfully started.";
    return true;
}

void DuplexBridge::stop()
{
    if (!m_running.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    if (m_serialToTcpThread.joinable()) {
        m_serialToTcpThread.join();
    }
    if (m_tcpToSerialThread.joinable()) {
        m_tcpToSerialThread.join();
    }

    m_serialPort.close();
    m_tcpClient.close();

    LOG(INFO) << "Full-duplex gateway bridge stopped.";
}

void DuplexBridge::serialToTcpWorker()
{
    while (m_running.load(std::memory_order_relaxed) || !m_serialToTcpRing->empty()) {
        GatewayFrame frame;
        if (m_serialToTcpRing->pop(frame)) {
            std::vector<uint8_t> payload(frame.data, frame.data + frame.size);
            if (m_tcpClient.send(payload)) {
                m_serialToTcpBytes.fetch_add(frame.size, std::memory_order_relaxed);
                m_serialToTcpPackets.fetch_add(1, std::memory_order_relaxed);
            }
        } else {
            std::this_thread::yield();
        }
    }
}

void DuplexBridge::tcpToSerialWorker()
{
    while (m_running.load(std::memory_order_relaxed) || !m_tcpToSerialRing->empty()) {
        GatewayFrame frame;
        if (m_tcpToSerialRing->pop(frame)) {
            std::vector<uint8_t> payload(frame.data, frame.data + frame.size);
            if (m_serialPort.send(payload)) {
                m_tcpToSerialBytes.fetch_add(frame.size, std::memory_order_relaxed);
                m_tcpToSerialPackets.fetch_add(1, std::memory_order_relaxed);
            }
        } else {
            std::this_thread::yield();
        }
    }
}

BridgeStats DuplexBridge::runSimulation(std::size_t messageCount)
{
    LOG(INFO) << "Starting full-duplex loopback simulation for " << messageCount << " bi-directional messages...";

    auto simSerialToTcpRing = std::make_unique<LockFreeRingBuffer<GatewayFrame, 4096>>();
    auto simTcpToSerialRing = std::make_unique<LockFreeRingBuffer<GatewayFrame, 4096>>();

    std::atomic<bool> simRunning {true};
    std::atomic<uint64_t> simSerToTcpPackets {0};
    std::atomic<uint64_t> simTcpToSerPackets {0};
    std::atomic<uint64_t> simSerToTcpBytes {0};
    std::atomic<uint64_t> simTcpToSerBytes {0};

    auto serialProducer = [&]() {
        for (uint64_t i = 0; i < messageCount; ++i) {
            GatewayFrame frame {};
            frame.size = 128;
            snprintf(reinterpret_cast<char*>(frame.data), frame.size, "SERIAL_MSG_%llu",
                     static_cast<unsigned long long>(i));

            while (!simSerialToTcpRing->push(frame)) {
                std::this_thread::yield();
            }
        }
    };

    auto tcpProducer = [&]() {
        for (uint64_t i = 0; i < messageCount; ++i) {
            GatewayFrame frame {};
            frame.size = 128;
            snprintf(reinterpret_cast<char*>(frame.data), frame.size, "TCP_MSG_%llu",
                     static_cast<unsigned long long>(i));

            while (!simTcpToSerialRing->push(frame)) {
                std::this_thread::yield();
            }
        }
    };

    auto tcpEgressConsumer = [&]() {
        for (uint64_t i = 0; i < messageCount; ++i) {
            GatewayFrame frame;
            while (!simSerialToTcpRing->pop(frame)) {
                std::this_thread::yield();
            }
            simSerToTcpPackets.fetch_add(1, std::memory_order_relaxed);
            simSerToTcpBytes.fetch_add(frame.size, std::memory_order_relaxed);
        }
    };

    auto serialEgressConsumer = [&]() {
        for (uint64_t i = 0; i < messageCount; ++i) {
            GatewayFrame frame;
            while (!simTcpToSerialRing->pop(frame)) {
                std::this_thread::yield();
            }
            simTcpToSerPackets.fetch_add(1, std::memory_order_relaxed);
            simTcpToSerBytes.fetch_add(frame.size, std::memory_order_relaxed);
        }
    };

    auto startTime = std::chrono::high_resolution_clock::now();

    std::thread t1(serialProducer);
    std::thread t2(tcpProducer);
    std::thread t3(tcpEgressConsumer);
    std::thread t4(serialEgressConsumer);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    auto endTime = std::chrono::high_resolution_clock::now();
    double totalSec = std::chrono::duration<double>(endTime - startTime).count();

    BridgeStats simStats {};
    simStats.serialToTcpBytes = simSerToTcpBytes.load();
    simStats.serialToTcpPackets = simSerToTcpPackets.load();
    simStats.tcpToSerialBytes = simTcpToSerBytes.load();
    simStats.tcpToSerialPackets = simTcpToSerPackets.load();

    std::cout << "\n=================================================================================\n";
    std::cout << "             FULL-DUPLEX SIMULATION BENCHMARK SUMMARY                           \n";
    std::cout << "=================================================================================\n";
    std::cout << " Execution Time:       " << std::fixed << std::setprecision(3) << totalSec << " seconds\n";
    std::cout << " Serial -> TCP Stream: " << simStats.serialToTcpPackets << " msgs ("
              << (simStats.serialToTcpBytes / (1024.0 * 1024.0)) << " MB)\n";
    std::cout << " TCP -> Serial Stream: " << simStats.tcpToSerialPackets << " msgs ("
              << (simStats.tcpToSerialBytes / (1024.0 * 1024.0)) << " MB)\n";
    if (totalSec > 0.0) {
        double serToTcpRate = static_cast<double>(simStats.serialToTcpPackets) / totalSec;
        double tcpToSerRate = static_cast<double>(simStats.tcpToSerialPackets) / totalSec;
        double combinedRate = serToTcpRate + tcpToSerRate;
        std::cout << " Combined Throughput:  " << static_cast<uint64_t>(combinedRate) << " bi-directional msgs/sec\n";
        std::cout << " Total Bandwidth:      " << std::fixed << std::setprecision(1)
                  << (((simStats.serialToTcpBytes + simStats.tcpToSerialBytes) / (1024.0 * 1024.0)) / totalSec)
                  << " MB/s\n";
    }
    std::cout << "=================================================================================\n\n";

    return simStats;
}

BridgeStats DuplexBridge::stats() const
{
    BridgeStats s {};
    s.serialToTcpBytes = m_serialToTcpBytes.load();
    s.serialToTcpPackets = m_serialToTcpPackets.load();
    s.tcpToSerialBytes = m_tcpToSerialBytes.load();
    s.tcpToSerialPackets = m_tcpToSerialPackets.load();
    s.droppedSerialToTcp = m_droppedSerialToTcp.load();
    s.droppedTcpToSerial = m_droppedTcpToSerial.load();
    return s;
}

} // namespace Communication
