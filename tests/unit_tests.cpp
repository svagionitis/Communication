/**
 * @file unit_tests.cpp
 * @brief Comprehensive Google Test suite verifying ThreadSafeQueue, TcpClient, TcpServer, UdpSocket, and Mock
 * SerialPort.
 */

#include "ICommunication.h"
#include "LockFreeRingBuffer.h"
#include "SerialPort.h"
#include "TcpClient.h"
#include "TcpServer.h"
#include "ThreadSafeQueue.h"
#include "UdpSocket.h"
#include <atomic>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

using namespace Communication;

// ============================================================================
// 1. ThreadSafeQueue Unit Tests
// ============================================================================

TEST(ThreadSafeQueueTest, BasicPushPop)
{
    ThreadSafeQueue<int> queue;
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0u);

    queue.push(42);
    queue.push(100);

    EXPECT_FALSE(queue.empty());
    EXPECT_EQ(queue.size(), 2u);

    EXPECT_EQ(queue.pop(), 42);
    EXPECT_EQ(queue.pop(), 100);
    EXPECT_TRUE(queue.empty());
}

TEST(ThreadSafeQueueTest, TryPopAndPopForTimeout)
{
    ThreadSafeQueue<std::string> queue;

    auto val = queue.tryPop();
    EXPECT_FALSE(val.has_value());

    auto popTimeout = queue.popFor(std::chrono::milliseconds(50));
    EXPECT_FALSE(popTimeout.has_value());

    queue.push("Hello");
    auto popVal = queue.popFor(std::chrono::milliseconds(100));
    ASSERT_TRUE(popVal.has_value());
    EXPECT_EQ(popVal.value(), "Hello");
}

TEST(ThreadSafeQueueTest, ConcurrentStressTest)
{
    ThreadSafeQueue<int> queue;
    constexpr int totalItems = 1000;
    std::atomic<int> receivedCount {0};

    std::thread producer([&queue]() {
        for (int i = 0; i < totalItems; ++i) {
            queue.push(i);
        }
    });

    std::thread consumer([&queue, &receivedCount]() {
        for (int i = 0; i < totalItems; ++i) {
            auto item = queue.popFor(std::chrono::seconds(2));
            if (item.has_value()) {
                receivedCount++;
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(receivedCount.load(), totalItems);
    EXPECT_TRUE(queue.empty());
}

// ============================================================================
// 2. LockFreeRingBuffer Unit Tests
// ============================================================================

TEST(LockFreeRingBufferTest, BasicPushPop)
{
    LockFreeRingBuffer<int, 16> ringBuffer;
    EXPECT_TRUE(ringBuffer.empty());
    EXPECT_EQ(ringBuffer.capacity(), 16u);

    EXPECT_TRUE(ringBuffer.push(101));
    EXPECT_TRUE(ringBuffer.push(102));
    EXPECT_EQ(ringBuffer.size(), 2u);

    int item = 0;
    EXPECT_TRUE(ringBuffer.pop(item));
    EXPECT_EQ(item, 101);

    EXPECT_TRUE(ringBuffer.pop(item));
    EXPECT_EQ(item, 102);

    EXPECT_FALSE(ringBuffer.pop(item));
    EXPECT_TRUE(ringBuffer.empty());
}

TEST(LockFreeRingBufferTest, FullEmptyBoundary)
{
    LockFreeRingBuffer<int, 4> ringBuffer;

    EXPECT_TRUE(ringBuffer.push(1));
    EXPECT_TRUE(ringBuffer.push(2));
    EXPECT_TRUE(ringBuffer.push(3));
    EXPECT_TRUE(ringBuffer.push(4));

    EXPECT_TRUE(ringBuffer.full());
    EXPECT_FALSE(ringBuffer.push(5)); // Overflow rejected

    int val = 0;
    EXPECT_TRUE(ringBuffer.pop(val));
    EXPECT_EQ(val, 1);

    EXPECT_FALSE(ringBuffer.full());
    EXPECT_TRUE(ringBuffer.push(5)); // Can push now
}

TEST(LockFreeRingBufferTest, ConcurrentSpscStressTest)
{
    LockFreeRingBuffer<int, 1024> ringBuffer;
    constexpr int totalElements = 100000;

    std::thread producer([&ringBuffer]() {
        for (int i = 0; i < totalElements; ++i) {
            while (!ringBuffer.push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::vector<int> poppedItems;
    poppedItems.reserve(totalElements);

    std::thread consumer([&ringBuffer, &poppedItems]() {
        int item = 0;
        while (poppedItems.size() < totalElements) {
            if (ringBuffer.pop(item)) {
                poppedItems.push_back(item);
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(poppedItems.size(), static_cast<std::size_t>(totalElements));
    for (int i = 0; i < totalElements; ++i) {
        EXPECT_EQ(poppedItems[static_cast<std::size_t>(i)], i);
    }
}

// ============================================================================
// 3. TCP Client & Server Integration Tests
// ============================================================================

TEST(TcpCommunicationTest, ServerClientLoopback)
{
    constexpr uint16_t testPort = 9876;
    TcpConfig serverCfg;
    serverCfg.host = "127.0.0.1";
    serverCfg.port = testPort;

    TcpServer server(serverCfg);
    std::atomic<bool> serverReceived {false};
    std::string serverMsg;

    server.registerReceiveCallback([&serverReceived, &serverMsg](const std::vector<uint8_t>& data) {
        serverMsg = std::string(reinterpret_cast<const char*>(data.data()), data.size());
        serverReceived.store(true);
    });

    ASSERT_TRUE(server.open());

    TcpConfig clientCfg;
    clientCfg.host = "127.0.0.1";
    clientCfg.port = testPort;

    TcpClient client(clientCfg);
    std::atomic<bool> clientReceived {false};
    std::string clientMsg;

    client.registerReceiveCallback([&clientReceived, &clientMsg](const std::vector<uint8_t>& data) {
        clientMsg = std::string(reinterpret_cast<const char*>(data.data()), data.size());
        clientReceived.store(true);
    });

    ASSERT_TRUE(client.open());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(client.send("Hello from TCP Client"));

    int retries = 20;
    while (!serverReceived.load() && retries-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_TRUE(serverReceived.load());
    EXPECT_EQ(serverMsg, "Hello from TCP Client");

    EXPECT_TRUE(server.send("Echo from TCP Server"));

    retries = 20;
    while (!clientReceived.load() && retries-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_TRUE(clientReceived.load());
    EXPECT_EQ(clientMsg, "Echo from TCP Server");

    client.close();
    server.close();
}

TEST(TcpCommunicationTest, SocketKeepAliveConfiguration)
{
    TcpKeepAliveConfig keepAlive;
    keepAlive.enable = true;
    keepAlive.keepIdleSec = 30;
    keepAlive.keepIntervalSec = 2;
    keepAlive.keepCount = 4;

    TcpConfig cfg;
    cfg.port = 9877;
    cfg.keepAlive = keepAlive;

    TcpServer server(cfg);
    ASSERT_TRUE(server.open());

    TcpClient client(cfg);
    ASSERT_TRUE(client.open());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    client.close();
    server.close();
}

TEST(TcpCommunicationTest, AutoReconnectOnServerRestart)
{
    std::atomic<bool> stateConnected {false};

    TcpConfig cfg;
    cfg.port = 9899;
    cfg.autoReconnect.enable = true;
    cfg.autoReconnect.initialDelayMs = 50;
    cfg.autoReconnect.maxDelayMs = 200;

    auto server = std::make_unique<TcpServer>(cfg);
    ASSERT_TRUE(server->open());

    TcpClient client(cfg);
    client.registerConnectionStateCallback([&stateConnected](bool connected) { stateConnected.store(connected); });

    ASSERT_TRUE(client.open());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(stateConnected.load());

    server->close();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    server = std::make_unique<TcpServer>(cfg);
    ASSERT_TRUE(server->open());

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_TRUE(stateConnected.load());

    client.close();
    server->close();
}

TEST(TcpCommunicationTest, ZeroCopyViewCallback)
{
    std::string rxData;
    std::mutex rxMutex;

    TcpConfig cfg;
    cfg.port = 9890;

    TcpServer server(cfg);
    ASSERT_TRUE(server.open());

    TcpClient client(cfg);
    client.registerReceiveViewCallback([&rxData, &rxMutex](std::string_view data) {
        std::lock_guard<std::mutex> lock(rxMutex);
        rxData = std::string(data);
    });

    ASSERT_TRUE(client.open());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    server.send("ZERO_COPY_PING");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    {
        std::lock_guard<std::mutex> lock(rxMutex);
        EXPECT_EQ(rxData, "ZERO_COPY_PING");
    }

    client.close();
    server.close();
}

// ============================================================================
// 4. Connection Statistics & Metrics Tests
// ============================================================================
TEST(ConnectionStatsTest, TcpClientAndServerTelemetry)
{
    TcpConfig cfg;
    cfg.port = 9892;

    TcpServer server(cfg);
    ASSERT_TRUE(server.open());

    TcpClient client(cfg);
    ASSERT_TRUE(client.open());

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    EXPECT_TRUE(client.send("TEST_STATS_PAYLOAD"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto cStats = client.stats();
    EXPECT_EQ(cStats.bytesSent, 18u);
    EXPECT_EQ(cStats.packetsSent, 1u);

    auto sStats = server.stats();
    EXPECT_EQ(sStats.bytesReceived, 18u);
    EXPECT_EQ(sStats.packetsReceived, 1u);

    client.close();
    server.close();
}

// ============================================================================
// 3. UDP Socket Loopback Test
// ============================================================================

TEST(UdpCommunicationTest, SocketLoopback)
{
    UdpConfig udp1Cfg;
    udp1Cfg.localPort = 9123;
    udp1Cfg.remoteHost = "127.0.0.1";
    udp1Cfg.remotePort = 9124;

    UdpConfig udp2Cfg;
    udp2Cfg.localPort = 9124;
    udp2Cfg.remoteHost = "127.0.0.1";
    udp2Cfg.remotePort = 9123;

    UdpSocket udp1(udp1Cfg);
    UdpSocket udp2(udp2Cfg);

    std::atomic<bool> udp2Received {false};
    std::string udp2Msg;

    udp2.registerReceiveCallback([&udp2Received, &udp2Msg](const std::vector<uint8_t>& data) {
        udp2Msg = std::string(reinterpret_cast<const char*>(data.data()), data.size());
        udp2Received.store(true);
    });

    ASSERT_TRUE(udp1.open());
    ASSERT_TRUE(udp2.open());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_TRUE(udp1.send("UDP Packet Payload"));

    int retries = 20;
    while (!udp2Received.load() && retries-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_TRUE(udp2Received.load());
    EXPECT_EQ(udp2Msg, "UDP Packet Payload");

    udp1.close();
    udp2.close();
}

// ============================================================================
// 4. Mock SerialPort Test
// ============================================================================

class MockSerialPort : public ICommunication {
public:
    explicit MockSerialPort(SerialConfig config = {})
        : m_config(config)
    {
    }

    bool open() override
    {
        m_isOpen = true;
        return true;
    }
    bool connect() override { return open(); }
    void close() override { m_isOpen = false; }
    void disconnect() override { close(); }

    bool send(const std::vector<uint8_t>& data) override
    {
        if (!m_isOpen)
            return false;
        m_sentBuffer.insert(m_sentBuffer.end(), data.begin(), data.end());
        return true;
    }

    bool send(std::string_view data) override { return send(std::vector<uint8_t>(data.begin(), data.end())); }

    void registerReceiveCallback(DataReceivedCallback callback) override { m_callback = std::move(callback); }

    bool isOpen() const override { return m_isOpen; }
    bool isConnected() const override { return isOpen(); }

    void simulateReceive(const std::vector<uint8_t>& data)
    {
        if (m_callback) {
            m_callback(data);
        }
    }

    const std::vector<uint8_t>& sentBuffer() const { return m_sentBuffer; }

private:
    SerialConfig m_config;
    bool m_isOpen {false};
    DataReceivedCallback m_callback;
    std::vector<uint8_t> m_sentBuffer;
};

TEST(MockSerialPortTest, SendAndSimulateReceive)
{
    MockSerialPort serial;
    std::atomic<bool> received {false};
    std::string rxData;

    serial.registerReceiveCallback([&received, &rxData](const std::vector<uint8_t>& data) {
        rxData = std::string(reinterpret_cast<const char*>(data.data()), data.size());
        received.store(true);
    });

    ASSERT_TRUE(serial.open());
    EXPECT_TRUE(serial.send("AT+PING\r\n"));

    std::string sentText(reinterpret_cast<const char*>(serial.sentBuffer().data()), serial.sentBuffer().size());
    EXPECT_EQ(sentText, "AT+PING\r\n");

    std::string simulatedResp = "OK\r\n";
    serial.simulateReceive(std::vector<uint8_t>(simulatedResp.begin(), simulatedResp.end()));

    EXPECT_TRUE(received.load());
    EXPECT_EQ(rxData, "OK\r\n");

    serial.close();
}

TEST(MockSerialPortTest, FlowControlConfiguration)
{
    SerialConfig cfg;
    cfg.portName = "COM_MOCK";
    cfg.baudRate = BaudRate::B115200;
    cfg.flowControl = FlowControl::Hardware;

    MockSerialPort serial(cfg);
    ASSERT_TRUE(serial.open());
    EXPECT_TRUE(serial.isOpen());
    serial.close();

    cfg.flowControl = FlowControl::Software;
    MockSerialPort serialSoft(cfg);
    ASSERT_TRUE(serialSoft.open());
    EXPECT_TRUE(serialSoft.isOpen());
    serialSoft.close();
}

// ============================================================================
// 5. Polymorphic ICommunication Base Vector Test
// ============================================================================

TEST(PolymorphismTest, PolymorphicChannelVector)
{
    std::vector<std::unique_ptr<ICommunication>> channels;

    TcpConfig tcpCfg;
    tcpCfg.port = 9888;
    channels.push_back(std::make_unique<TcpServer>(tcpCfg));

    UdpConfig udpCfg;
    udpCfg.localPort = 9889;
    channels.push_back(std::make_unique<UdpSocket>(udpCfg));

    channels.push_back(std::make_unique<MockSerialPort>());

    for (auto& ch : channels) {
        EXPECT_FALSE(ch->isOpen());
        EXPECT_TRUE(ch->open());
        EXPECT_TRUE(ch->isOpen());
        ch->close();
        EXPECT_FALSE(ch->isOpen());
    }
}
