/**
 * @file SimulatorProvider.cpp
 * @brief Simulator provider implementation.
 */

#include "SimulatorProvider.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <glog/logging.h>

namespace Communication {

SimulatorProvider::SimulatorProvider(std::size_t ratePerSec)
    : m_ratePerSec(ratePerSec)
{
}

SimulatorProvider::~SimulatorProvider()
{
    stop();
}

bool SimulatorProvider::start(FrameCallback callback)
{
    stop();
    m_callback = callback;
    m_running.store(true, std::memory_order_release);

    m_workerThread = std::thread(&SimulatorProvider::simulateLoop, this);
    LOG(INFO) << "Started MultiDomain Simulator provider (rate = " << m_ratePerSec << " updates/sec)";
    return true;
}

void SimulatorProvider::stop()
{
    if (!m_running.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    LOG(INFO) << "Stopped MultiDomain Simulator provider.";
}

void SimulatorProvider::simulateLoop()
{
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t seq = 0;
    double intervalUs = (m_ratePerSec > 0) ? (1000000.0 / static_cast<double>(m_ratePerSec)) : 0.0;

    struct SimulatedShip {
        const char* mmsi;
        const char* name;
        double startLat;
        double startLon;
        float speed;
        float heading;
    } ships[] = {{"211281000", "EVER_GIVEN", 51.9244, 4.4777, 14.2f, 270.0f},
                 {"311000450", "MAERSK_MCKINNEY", 51.9500, 4.2000, 18.5f, 265.0f},
                 {"228345000", "CMA_CGM_TROCUT", 51.8800, 4.3000, 12.0f, 90.0f},
                 {"244670000", "ROTTERDAM_TUG_01", 51.9100, 4.4500, 8.5f, 180.0f}};

    struct SimulatedAircraft {
        const char* icao;
        const char* callsign;
        double startLat;
        double startLon;
        double alt;
        float speed;
        float heading;
    } planes[] = {{"4B1800", "DLH412", 50.0379, 8.5622, 34000.0, 460.0f, 280.0f},
                  {"3C6580", "BAW117", 51.4700, -0.4543, 38000.0, 480.0f, 290.0f},
                  {"400A2B", "AFR006", 49.0097, 2.5479, 28000.0, 430.0f, 310.0f},
                  {"A01234", "UAL925", 51.2000, 3.1000, 12000.0, 250.0f, 120.0f}};

    std::size_t numShips = sizeof(ships) / sizeof(ships[0]);
    std::size_t numPlanes = sizeof(planes) / sizeof(planes[0]);

    while (m_running.load(std::memory_order_relaxed)) {
        auto now = std::chrono::high_resolution_clock::now();
        double elapsedSec = std::chrono::duration<double>(now - start).count();

        TargetFrame frame {};
        frame.timestampNs =
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());

        if (seq % 2 == 0) {
            auto& ship = ships[(seq / 2) % numShips];
            frame.domain = DomainType::Maritime_AIS;
            snprintf(frame.targetId, sizeof(frame.targetId), "%s", ship.mmsi);
            snprintf(frame.nameCallsign, sizeof(frame.nameCallsign), "%s", ship.name);
            frame.latitude = ship.startLat + (elapsedSec * 0.0001);
            frame.longitude = ship.startLon + (elapsedSec * 0.0001);
            frame.altitudeFeet = 0.0;
            frame.speedKnots = ship.speed;
            frame.headingDegrees = ship.heading;
        } else {
            auto& plane = planes[(seq / 2) % numPlanes];
            frame.domain = DomainType::Aviation_ADSB;
            snprintf(frame.targetId, sizeof(frame.targetId), "%s", plane.icao);
            snprintf(frame.nameCallsign, sizeof(frame.nameCallsign), "%s", plane.callsign);
            frame.latitude = plane.startLat + (elapsedSec * 0.001);
            frame.longitude = plane.startLon - (elapsedSec * 0.001);
            frame.altitudeFeet = plane.alt + (std::sin(elapsedSec) * 100.0);
            frame.speedKnots = plane.speed;
            frame.headingDegrees = plane.heading;
        }

        if (m_callback) {
            m_callback(frame);
        }

        ++seq;

        if (intervalUs > 0.0) {
            auto nextTime = start + std::chrono::microseconds(static_cast<int64_t>(seq * intervalUs));
            std::this_thread::sleep_until(nextTime);
        }
    }
}

} // namespace Communication
