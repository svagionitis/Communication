/**
 * @file SimulatorProvider.h
 * @brief High-frequency multi-domain feed simulation tracking provider.
 */

#pragma once

#include "ITrackingProvider.h"

#include <atomic>
#include <cstddef>
#include <thread>

namespace Communication {

class SimulatorProvider : public ITrackingProvider {
public:
    explicit SimulatorProvider(std::size_t ratePerSec = 10000);
    ~SimulatorProvider() override;

    std::string name() const override { return "MultiDomain_Simulator"; }
    bool start(FrameCallback callback) override;
    void stop() override;
    bool isRunning() const override { return m_running.load(); }

private:
    void simulateLoop();

    std::size_t m_ratePerSec;
    FrameCallback m_callback;
    std::atomic<bool> m_running {false};
    std::thread m_workerThread;
};

} // namespace Communication
