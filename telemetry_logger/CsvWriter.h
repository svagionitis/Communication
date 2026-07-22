/**
 * @file CsvWriter.h
 * @brief CSV text file storage backend for telemetry logging.
 */

#pragma once

#include "ITelemetryWriter.h"

#include <fstream>

namespace Communication {

class CsvWriter : public ITelemetryWriter {
public:
    CsvWriter() = default;
    ~CsvWriter() override;

    bool open(const std::string& destination) override;
    std::size_t writeBatch(const std::vector<TelemetryPacket>& packets) override;
    void close() override;

private:
    std::ofstream m_file;
};

} // namespace Communication
