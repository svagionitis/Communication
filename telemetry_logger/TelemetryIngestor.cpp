/**
 * @file TelemetryIngestor.cpp
 * @brief Multi-threaded ingestion engine implementation.
 */

#include "TelemetryIngestor.h"
#include "UdpSocket.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <glog/logging.h>

namespace Communication {

TelemetryIngestor::TelemetryIngestor(std::unique_ptr<ITelemetryWriter> writer, std::size_t batchSize)
    : m_writer(std::move(writer))
    , m_batchSize(batchSize)
{
}

TelemetryIngestor::~TelemetryIngestor()
{
    stop();
}

bool TelemetryIngestor::startSimulation(std::size_t ratePerSec, double durationSec)
{
    if (m_running.load()) {
        stop();
    }
    m_running.store(true, std::memory_order_release);

    m_consumerThread = std::thread(&TelemetryIngestor::consumerLoop, this);
    m_producerThread = std::thread(&TelemetryIngestor::producerSimulateLoop, this, ratePerSec, durationSec);

    return true;
}

bool TelemetryIngestor::startUdp(uint16_t port, double durationSec)
{
    if (m_running.load()) {
        stop();
    }
    m_running.store(true, std::memory_order_release);

    m_consumerThread = std::thread(&TelemetryIngestor::consumerLoop, this);

    m_producerThread = std::thread([this, port, durationSec]() {
        UdpConfig config;
        config.localPort = port;

        auto socket = std::make_unique<UdpSocket>(config);
        socket->registerReceiveViewCallback([this](const uint8_t* data, std::size_t size) {
            if (size == sizeof(TelemetryPacket)) {
                TelemetryPacket packet;
                std::memcpy(&packet, data, sizeof(packet));
                m_totalIngested.fetch_add(1, std::memory_order_relaxed);
                if (!m_ringBuffer.push(packet)) {
                    m_totalDropped.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });

        if (!socket->open()) {
            LOG(ERROR) << "Failed to open UDP telemetry socket on port " << port;
            m_running.store(false);
            return;
        }

        LOG(INFO) << "Listening for UDP telemetry packets on port " << port << "...";

        auto start = std::chrono::high_resolution_clock::now();
        while (m_running.load(std::memory_order_relaxed)) {
            auto now = std::chrono::high_resolution_clock::now();
            if (durationSec > 0.0 && std::chrono::duration<double>(now - start).count() >= durationSec) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        socket->close();
        m_running.store(false);
    });

    return true;
}

void TelemetryIngestor::stop()
{
    if (!m_running.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (m_producerThread.joinable()) {
        m_producerThread.join();
    }
    if (m_consumerThread.joinable()) {
        m_consumerThread.join();
    }
    if (m_writer) {
        m_writer->close();
    }
}

void TelemetryIngestor::producerSimulateLoop(std::size_t ratePerSec, double durationSec)
{
    LOG(INFO) << "Starting telemetry simulator: Target rate = " << ratePerSec
              << " packets/sec, duration = " << durationSec << " sec";

    auto start = std::chrono::high_resolution_clock::now();
    uint64_t seq = 0;

    double intervalUs = (ratePerSec > 0) ? (1000000.0 / static_cast<double>(ratePerSec)) : 0.0;

    while (m_running.load(std::memory_order_relaxed)) {
        auto now = std::chrono::high_resolution_clock::now();
        double elapsedSec = std::chrono::duration<double>(now - start).count();
        if (durationSec > 0.0 && elapsedSec >= durationSec) {
            break;
        }

        TelemetryPacket packet {};
        packet.timestampNs =
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
        snprintf(packet.sensorId, sizeof(packet.sensorId), "IMU_SENSOR_01");
        packet.accelX = std::sin(elapsedSec * 2.0);
        packet.accelY = std::cos(elapsedSec * 2.0);
        packet.accelZ = 9.81f;
        packet.gyroX = 0.01f;
        packet.gyroY = 0.02f;
        packet.gyroZ = 0.03f;
        packet.temperature = 25.5f + (seq % 10) * 0.1f;

        m_totalIngested.fetch_add(1, std::memory_order_relaxed);
        if (!m_ringBuffer.push(packet)) {
            m_totalDropped.fetch_add(1, std::memory_order_relaxed);
        }

        ++seq;

        if (intervalUs > 0.0) {
            auto nextTime = start + std::chrono::microseconds(static_cast<int64_t>(seq * intervalUs));
            std::this_thread::sleep_until(nextTime);
        }
    }

    LOG(INFO) << "Producer simulation completed. Total generated: " << seq;
}

void TelemetryIngestor::consumerLoop()
{
    std::vector<TelemetryPacket> batch;
    batch.reserve(m_batchSize);

    while (m_running.load(std::memory_order_relaxed) || !m_ringBuffer.empty()) {
        TelemetryPacket packet;
        while (batch.size() < m_batchSize && m_ringBuffer.pop(packet)) {
            batch.push_back(packet);
        }

        if (!batch.empty()) {
            std::size_t written = m_writer->writeBatch(batch);
            m_totalWritten.fetch_add(written, std::memory_order_relaxed);
            batch.clear();
        } else {
            std::this_thread::yield();
        }
    }
}

} // namespace Communication
