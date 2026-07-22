/**
 * @file TrackingEngine.h
 * @brief Multi-threaded telemetry ingestion engine for AIS & ADS-B targets with extensible provider registry.
 */

#pragma once

#include "LockFreeRingBuffer.h"
#include "ProviderRegistry.h"
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
    explicit TrackingEngine(std::size_t batchSize = 500);
    ~TrackingEngine();

    bool openDatabase(const std::string& dbPath);
    void addProvider(std::unique_ptr<ITrackingProvider> provider);
    bool start();
    void stop();

    std::size_t totalIngested() const { return m_totalIngested.load(); }
    std::size_t totalWritten() const { return m_totalWritten.load(); }
    std::size_t totalDropped() const { return m_totalDropped.load(); }

private:
    void consumerLoop();
    void pushFrame(const TargetFrame& frame);

    TrackingDatabase m_db;
    std::size_t m_batchSize;
    std::unique_ptr<LockFreeRingBuffer<TargetFrame, 8192>> m_targetRing;
    ProviderRegistry m_registry;

    std::atomic<bool> m_running {false};
    std::atomic<std::size_t> m_totalIngested {0};
    std::atomic<std::size_t> m_totalWritten {0};
    std::atomic<std::size_t> m_totalDropped {0};

    std::thread m_consumerThread;
};

} // namespace Communication
