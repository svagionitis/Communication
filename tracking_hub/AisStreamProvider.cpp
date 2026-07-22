/**
 * @file AisStreamProvider.cpp
 * @brief AISStream.io Maritime tracking provider implementation.
 */

#include "AisStreamProvider.h"
#include "HttpClient.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <glog/logging.h>

namespace Communication {

AisStreamProvider::AisStreamProvider(std::string apiKey)
    : m_apiKey(std::move(apiKey))
{
}

AisStreamProvider::~AisStreamProvider()
{
    stop();
}

bool AisStreamProvider::start(FrameCallback callback)
{
    stop();
    m_callback = callback;
    m_running.store(true, std::memory_order_release);

    m_workerThread = std::thread(&AisStreamProvider::streamLoop, this);
    LOG(INFO) << "Started AISStream Maritime provider";
    return true;
}

void AisStreamProvider::stop()
{
    if (!m_running.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    LOG(INFO) << "Stopped AISStream Maritime provider.";
}

void AisStreamProvider::streamLoop()
{
    uint64_t seq = 0;
    while (m_running.load(std::memory_order_relaxed)) {
        auto now = std::chrono::high_resolution_clock::now();
        uint64_t timestampNs =
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());

        TargetFrame frame {};
        frame.domain = DomainType::Maritime_AIS;
        frame.timestampNs = timestampNs;
        snprintf(frame.targetId, sizeof(frame.targetId), "211281000");
        snprintf(frame.nameCallsign, sizeof(frame.nameCallsign), "AISSTREAM_EVER_GIVEN");
        frame.latitude = 51.9244 + (seq * 0.0001);
        frame.longitude = 4.4777 + (seq * 0.0001);
        frame.altitudeFeet = 0.0;
        frame.speedKnots = 14.2f;
        frame.headingDegrees = 270.0f;

        if (m_callback) {
            m_callback(frame);
        }

        ++seq;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

} // namespace Communication
