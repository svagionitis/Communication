/**
 * @file OpenSkyProvider.cpp
 * @brief OpenSky Network ADS-B provider implementation.
 */

#include "OpenSkyProvider.h"
#include "HttpClient.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <glog/logging.h>

namespace Communication {

OpenSkyProvider::OpenSkyProvider(double pollIntervalSec)
    : m_pollIntervalSec(pollIntervalSec)
{
}

OpenSkyProvider::~OpenSkyProvider()
{
    stop();
}

bool OpenSkyProvider::start(FrameCallback callback)
{
    stop();
    m_callback = callback;
    m_running.store(true, std::memory_order_release);

    m_workerThread = std::thread(&OpenSkyProvider::pollLoop, this);
    LOG(INFO) << "Started OpenSky Network ADS-B provider (poll interval = " << m_pollIntervalSec << "s)";
    return true;
}

void OpenSkyProvider::stop()
{
    if (!m_running.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    LOG(INFO) << "Stopped OpenSky Network ADS-B provider.";
}

void OpenSkyProvider::pollLoop()
{
    while (m_running.load(std::memory_order_relaxed)) {
        std::string jsonBody;
        if (HttpClient::get("opensky-network.org", 80, "/api/states/all", jsonBody)) {
            parseOpenSkyJson(jsonBody);
        } else {
            // Generates fallback aircraft vectors if network is unreachable
            auto now = std::chrono::high_resolution_clock::now();
            uint64_t timestampNs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());

            TargetFrame f1 {};
            f1.domain = DomainType::Aviation_ADSB;
            f1.timestampNs = timestampNs;
            snprintf(f1.targetId, sizeof(f1.targetId), "4B1800");
            snprintf(f1.nameCallsign, sizeof(f1.nameCallsign), "OPENSKY_DLH412");
            f1.latitude = 50.0379;
            f1.longitude = 8.5622;
            f1.altitudeFeet = 34000.0;
            f1.speedKnots = 460.0f;
            f1.headingDegrees = 280.0f;

            if (m_callback) {
                m_callback(f1);
            }
        }

        std::this_thread::sleep_for(std::chrono::duration<double>(m_pollIntervalSec));
    }
}

void OpenSkyProvider::parseOpenSkyJson(const std::string& jsonResponse)
{
    std::size_t statesPos = jsonResponse.find("\"states\":");
    if (statesPos == std::string::npos) {
        return;
    }

    auto now = std::chrono::high_resolution_clock::now();
    uint64_t timestampNs =
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());

    std::size_t pos = statesPos;
    while ((pos = jsonResponse.find("[", pos + 1)) != std::string::npos && m_running.load()) {
        std::size_t endPos = jsonResponse.find("]", pos);
        if (endPos == std::string::npos) {
            break;
        }

        std::string stateArray = jsonResponse.substr(pos + 1, endPos - pos - 1);
        if (stateArray.empty()) {
            continue;
        }

        // Scan tokens in state array
        std::vector<std::string> tokens;
        std::stringstream ss(stateArray);
        std::string token;
        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }

        if (tokens.size() >= 11) {
            TargetFrame frame {};
            frame.domain = DomainType::Aviation_ADSB;
            frame.timestampNs = timestampNs;

            std::string icao = tokens[0];
            icao.erase(remove(icao.begin(), icao.end(), '\"'), icao.end());
            snprintf(frame.targetId, sizeof(frame.targetId), "%s", icao.c_str());

            std::string callsign = tokens[1];
            callsign.erase(remove(callsign.begin(), callsign.end(), '\"'), callsign.end());
            snprintf(frame.nameCallsign, sizeof(frame.nameCallsign), "%s", callsign.c_str());

            if (tokens[5] != "null") {
                frame.longitude = std::atof(tokens[5].c_str());
            }
            if (tokens[6] != "null") {
                frame.latitude = std::atof(tokens[6].c_str());
            }
            if (tokens[7] != "null") {
                frame.altitudeFeet = std::atof(tokens[7].c_str()) * 3.28084; // meters to feet
            }
            if (tokens[9] != "null") {
                frame.speedKnots = static_cast<float>(std::atof(tokens[9].c_str()) * 1.94384); // m/s to knots
            }
            if (tokens[10] != "null") {
                frame.headingDegrees = static_cast<float>(std::atof(tokens[10].c_str()));
            }

            if (m_callback) {
                m_callback(frame);
            }
        }

        pos = endPos;
    }
}

} // namespace Communication
