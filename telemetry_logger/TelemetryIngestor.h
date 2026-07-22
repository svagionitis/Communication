/**
 * @file TelemetryIngestor.h
 * @brief High-frequency telemetry ingestion engine backed by LockFreeRingBuffer.
 */

#pragma once

#include "ITelemetryWriter.h"
#include "LockFreeRingBuffer.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace Communication {

enum class IngestMode { Simulate, Udp, Serial };

class TelemetryIngestor {
public:
    TelemetryIngestor(std::unique_ptr<ITelemetryWriter> writer, std::size_t batchSize = 500);
    ~TelemetryIngestor();

    bool startSimulation(std::size_t ratePerSec, double durationSec);
    bool startUdp(uint16_t port, double durationSec);

    void stop();

    std::size_t totalIngested() const { return m_totalIngested.load(); }
    std::size_t totalWritten() const { return m_totalWritten.load(); }
    std::size_t totalDropped() const { return m_totalDropped.load(); }

private:
    void consumerLoop();
    void producerSimulateLoop(std::size_t ratePerSec, double durationSec);

    std::unique_ptr<ITelemetryWriter> m_writer;
    std::size_t m_batchSize;
    LockFreeRingBuffer<TelemetryPacket, 8192> m_ringBuffer;

    std::atomic<bool> m_running {false};
    std::atomic<std::size_t> m_totalIngested {0};
    std::atomic<std::size_t> m_totalWritten {0};
    std::atomic<std::size_t> m_totalDropped {0};

    std::thread m_producerThread;
    std::thread m_consumerThread;
};

} // namespace Communication
