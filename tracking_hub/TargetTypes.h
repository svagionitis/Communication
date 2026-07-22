/**
 * @file TargetTypes.h
 * @brief Common telemetry schemas for AIS (Maritime) and ADS-B (Aviation) targets.
 */

#pragma once

#include <cstdint>

namespace Communication {

/**
 * @brief Telemetry domain category.
 */
enum class DomainType { Maritime_AIS, Aviation_ADSB };

/**
 * @brief Decoded unified spatial target position frame.
 */
struct TargetFrame {
    DomainType domain {DomainType::Maritime_AIS};
    uint64_t timestampNs {0};
    char targetId[16] {0};     // MMSI for ships ("211281000") or ICAO Hex for aircraft ("4B1800")
    char nameCallsign[32] {0}; // Vessel Name or Flight Callsign ("DLH412")
    double latitude {0.0};
    double longitude {0.0};
    double altitudeFeet {0.0};   // 0 for ships
    float speedKnots {0.0f};     // SOG or Airspeed
    float headingDegrees {0.0f}; // COG or Heading
};

} // namespace Communication
