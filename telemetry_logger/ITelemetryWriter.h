/**
 * @file ITelemetryWriter.h
 * @brief Abstract interface for telemetry storage writers (SQLite, CSV, Binary).
 */

#pragma once

#include "TelemetryTypes.h"

#include <string>
#include <vector>

namespace Communication {

/**
 * @brief Polymorphic backend interface for telemetry persistence.
 */
class ITelemetryWriter {
public:
    virtual ~ITelemetryWriter() = default;

    /**
     * @brief Opens the output storage destination.
     * @param destination File path or connection string.
     * @return True if opened successfully.
     */
    virtual bool open(const std::string& destination) = 0;

    /**
     * @brief Writes a batch of telemetry packets to disk/database.
     * @param packets Vector of telemetry samples.
     * @return Number of packets successfully written.
     */
    virtual std::size_t writeBatch(const std::vector<TelemetryPacket>& packets) = 0;

    /**
     * @brief Flushes and closes storage handle.
     */
    virtual void close() = 0;
};

} // namespace Communication
