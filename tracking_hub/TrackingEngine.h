/**
 * @file TrackingEngine.h
 * @brief Multi-threaded telemetry ingestion engine for AIS & ADS-B targets.
 */

#pragma once

#include "LockFreeRingBuffer.h"
#include "TargetTypes.h"
#include "TrackingDatabase.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace Communication {

class TrackingEngine {
public:
    TrackingEngine(std::size_t batchSize = 500);
    ~TrackingEngine();

    bool openDatabase(const std::string& dbPath);
    bool startSimulation(std::size_t ratePerSec, double durationSec);

    void stop();

    std::size_t totalIngested() const { return m_totalIngested.load(); }
    std::size_t totalWritten() const { return m_totalWritten.load(); }
    std::size_t totalDropped() const { return m_totalDropped.load(); }

private:
    void consumerLoop();
    void producerSimulateLoop(std::size_t ratePerSec, double durationSec);

    TrackingDatabase m_db;
    std::size_t m_batchSize;
    std::unique_ptr<LockFreeRingBuffer<TargetFrame, 8192>> m_targetRing;

    std::atomic<bool> m_running {false};
    std::atomic<std::size_t> m_totalIngested {0};
    std::atomic<std::size_t> m_totalWritten {0};
    std::atomic<std::size_t> m_totalDropped {0};

    std::thread m_producerThread;
    std::thread m_consumerThread;
};

} // namespace Communication
