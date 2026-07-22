/**
 * @file TelemetryTypes.h
 * @brief Common telemetry data structures and packet formats.
 */

#pragma once

#include <cstdint>

namespace Communication {

/**
 * @brief High-frequency sensor sample packet.
 */
struct TelemetryPacket {
    uint64_t timestampNs {0};
    char sensorId[16] {0};
    float accelX {0.0f};
    float accelY {0.0f};
    float accelZ {0.0f};
    float gyroX {0.0f};
    float gyroY {0.0f};
    float gyroZ {0.0f};
    float temperature {0.0f};
};

} // namespace Communication
