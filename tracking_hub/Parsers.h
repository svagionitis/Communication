/**
 * @file Parsers.h
 * @brief Parsers for AIS NMEA AIVDM sentences and ADS-B SBS-1 telemetry streams.
 */

#pragma once

#include "TargetTypes.h"

#include <string_view>

namespace Communication {

class Parsers {
public:
    /**
     * @brief Parses an AIS NMEA sentence (!AIVDM / !AIVDO).
     * @param sentence NMEA string view.
     * @param outFrame Populated TargetFrame upon successful parse.
     * @return True if frame was successfully parsed.
     */
    static bool parseAisNmea(std::string_view sentence, TargetFrame& outFrame);

    /**
     * @brief Parses an ADS-B SBS-1 text CSV position line.
     * @param line CSV line string view.
     * @param outFrame Populated TargetFrame upon successful parse.
     * @return True if frame was successfully parsed.
     */
    static bool parseAdsbSbs(std::string_view line, TargetFrame& outFrame);
};

} // namespace Communication
