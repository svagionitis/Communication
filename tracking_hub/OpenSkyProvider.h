/**
 * @file OpenSkyProvider.h
 * @brief Live OpenSky Network ADS-B REST API tracking provider.
 */

#pragma once

#include "ITrackingProvider.h"

#include <atomic>
#include <cstddef>
#include <thread>

namespace Communication {

class OpenSkyProvider : public ITrackingProvider {
public:
    explicit OpenSkyProvider(double pollIntervalSec = 5.0);
    ~OpenSkyProvider() override;

    std::string name() const override { return "OpenSkyNetwork_ADSB"; }
    bool start(FrameCallback callback) override;
    void stop() override;
    bool isRunning() const override { return m_running.load(); }

private:
    void pollLoop();
    void parseOpenSkyJson(const std::string& jsonResponse);

    double m_pollIntervalSec;
    FrameCallback m_callback;
    std::atomic<bool> m_running {false};
    std::thread m_workerThread;
};

} // namespace Communication
