/**
 * @file TrackingEngine.cpp
 * @brief TrackingEngine implementation using ProviderRegistry and LockFreeRingBuffer.
 */

#include "TrackingEngine.h"

#include <glog/logging.h>

namespace Communication {

TrackingEngine::TrackingEngine(std::size_t batchSize)
    : m_batchSize(batchSize)
    , m_targetRing(std::make_unique<LockFreeRingBuffer<TargetFrame, 8192>>())
{
}

TrackingEngine::~TrackingEngine()
{
    stop();
}

bool TrackingEngine::openDatabase(const std::string& dbPath)
{
    return m_db.open(dbPath);
}

void TrackingEngine::addProvider(std::unique_ptr<ITrackingProvider> provider)
{
    m_registry.addProvider(std::move(provider));
}

bool TrackingEngine::start()
{
    if (m_running.load()) {
        stop();
    }

    m_running.store(true, std::memory_order_release);

    m_consumerThread = std::thread(&TrackingEngine::consumerLoop, this);

    LOG(INFO) << "Starting " << m_registry.count() << " tracking provider(s)...";
    m_registry.startAll([this](const TargetFrame& frame) { pushFrame(frame); });

    return true;
}

void TrackingEngine::stop()
{
    if (!m_running.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    m_registry.stopAll();

    if (m_consumerThread.joinable()) {
        m_consumerThread.join();
    }

    m_db.close();
}

void TrackingEngine::pushFrame(const TargetFrame& frame)
{
    m_totalIngested.fetch_add(1, std::memory_order_relaxed);
    if (!m_targetRing->push(frame)) {
        m_totalDropped.fetch_add(1, std::memory_order_relaxed);
    }
}

void TrackingEngine::consumerLoop()
{
    std::vector<TargetFrame> batch;
    batch.reserve(m_batchSize);

    while (m_running.load(std::memory_order_relaxed) || !m_targetRing->empty()) {
        TargetFrame frame;
        while (batch.size() < m_batchSize && m_targetRing->pop(frame)) {
            batch.push_back(frame);
        }

        if (!batch.empty()) {
            std::size_t written = m_db.insertBatch(batch);
            m_totalWritten.fetch_add(written, std::memory_order_relaxed);
            batch.clear();
        } else {
            std::this_thread::yield();
        }
    }
}

} // namespace Communication
