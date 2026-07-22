/**
 * @file BinaryWriter.h
 * @brief Binary packed structure file storage backend for telemetry logging.
 */

#pragma once

#include "ITelemetryWriter.h"

#include <fstream>

namespace Communication {

class BinaryWriter : public ITelemetryWriter {
public:
    BinaryWriter() = default;
    ~BinaryWriter() override;

    bool open(const std::string& destination) override;
    std::size_t writeBatch(const std::vector<TelemetryPacket>& packets) override;
    void close() override;

private:
    std::ofstream m_file;
};

} // namespace Communication
