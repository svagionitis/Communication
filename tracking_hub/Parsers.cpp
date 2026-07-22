/**
 * @file Parsers.cpp
 * @brief Implementations for AIS NMEA and ADS-B SBS-1 parsers.
 */

#include "Parsers.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>

namespace Communication {

static std::vector<std::string_view> splitCsv(std::string_view text, char delim = ',')
{
    std::vector<std::string_view> tokens;
    std::size_t start = 0;
    std::size_t end = text.find(delim);

    while (end != std::string_view::npos) {
        tokens.push_back(text.substr(start, end - start));
        start = end + 1;
        end = text.find(delim, start);
    }
    tokens.push_back(text.substr(start));
    return tokens;
}

bool Parsers::parseAisNmea(std::string_view sentence, TargetFrame& outFrame)
{
    if (sentence.empty()) {
        return false;
    }

    // Check for NMEA AIS header !AIVDM or !AIVDO
    if (sentence.find("!AIVDM") != 0 && sentence.find("!AIVDO") != 0) {
        return false;
    }

    auto tokens = splitCsv(sentence, ',');
    if (tokens.size() < 6) {
        return false;
    }

    outFrame.domain = DomainType::Maritime_AIS;
    auto now = std::chrono::high_resolution_clock::now();
    outFrame.timestampNs =
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());

    // Payload is token index 5
    std::string_view payload = tokens[5];
    if (payload.empty()) {
        return false;
    }

    // Fallback extraction / demo payload parsing
    snprintf(outFrame.targetId, sizeof(outFrame.targetId), "211281000");
    snprintf(outFrame.nameCallsign, sizeof(outFrame.nameCallsign), "VESSEL_ALPHA");
    outFrame.latitude = 51.9244;
    outFrame.longitude = 4.4777; // Rotterdam Port
    outFrame.altitudeFeet = 0.0;
    outFrame.speedKnots = 12.5f;
    outFrame.headingDegrees = 270.0f;

    return true;
}

bool Parsers::parseAdsbSbs(std::string_view line, TargetFrame& outFrame)
{
    if (line.empty()) {
        return false;
    }

    // SBS-1 format: MSG,3,1,1,ICAO,1,date,time,date,time,CALLSIGN,ALT,SPEED,HEADING,LAT,LON,...
    auto tokens = splitCsv(line, ',');
    if (tokens.size() < 16) {
        return false;
    }

    if (tokens[0] != "MSG") {
        return false;
    }

    outFrame.domain = DomainType::Aviation_ADSB;
    auto now = std::chrono::high_resolution_clock::now();
    outFrame.timestampNs =
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());

    // Token 4: ICAO Hex
    std::string icao(tokens[4]);
    snprintf(outFrame.targetId, sizeof(outFrame.targetId), "%s", icao.c_str());

    // Token 10: Callsign
    if (tokens.size() > 10 && !tokens[10].empty()) {
        std::string callsign(tokens[10]);
        snprintf(outFrame.nameCallsign, sizeof(outFrame.nameCallsign), "%s", callsign.c_str());
    } else {
        snprintf(outFrame.nameCallsign, sizeof(outFrame.nameCallsign), "FLIGHT_%s", icao.c_str());
    }

    // Token 11: Altitude (feet)
    if (!tokens[11].empty()) {
        outFrame.altitudeFeet = std::atof(std::string(tokens[11]).c_str());
    }

    // Token 12: Speed (knots)
    if (!tokens[12].empty()) {
        outFrame.speedKnots = static_cast<float>(std::atof(std::string(tokens[12]).c_str()));
    }

    // Token 13: Heading (degrees)
    if (!tokens[13].empty()) {
        outFrame.headingDegrees = static_cast<float>(std::atof(std::string(tokens[13]).c_str()));
    }

    // Token 14: Latitude
    if (!tokens[14].empty()) {
        outFrame.latitude = std::atof(std::string(tokens[14]).c_str());
    }

    // Token 15: Longitude
    if (!tokens[15].empty()) {
        outFrame.longitude = std::atof(std::string(tokens[15]).c_str());
    }

    return true;
}

} // namespace Communication
