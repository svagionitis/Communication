/**
 * @file AisStreamProvider.h
 * @brief Live AISStream.io / AISHub Maritime tracking provider.
 */

#pragma once

#include "ITrackingProvider.h"

#include <atomic>
#include <string>
#include <thread>

namespace Communication {

class AisStreamProvider : public ITrackingProvider {
public:
    explicit AisStreamProvider(std::string apiKey = "");
    ~AisStreamProvider() override;

    std::string name() const override { return "AISStream_Maritime"; }
    bool start(FrameCallback callback) override;
    void stop() override;
    bool isRunning() const override { return m_running.load(); }

private:
    void streamLoop();

    std::string m_apiKey;
    FrameCallback m_callback;
    std::atomic<bool> m_running {false};
    std::thread m_workerThread;
};

} // namespace Communication
