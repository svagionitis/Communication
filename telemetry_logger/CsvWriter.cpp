/**
 * @file CsvWriter.cpp
 * @brief CSV text file output writer with buffered stream formatting.
 */

#include "CsvWriter.h"

#include <glog/logging.h>

namespace Communication {

CsvWriter::~CsvWriter()
{
    close();
}

bool CsvWriter::open(const std::string& destination)
{
    close();

    m_file.open(destination, std::ios::out | std::ios::trunc);
    if (!m_file.is_open()) {
        LOG(ERROR) << "Failed to open CSV output file: " << destination;
        return false;
    }

    m_file << "timestamp_ns,sensor_id,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,temperature\n";
    LOG(INFO) << "Opened CSV telemetry log at: " << destination;
    return true;
}

std::size_t CsvWriter::writeBatch(const std::vector<TelemetryPacket>& packets)
{
    if (!m_file.is_open() || packets.empty()) {
        return 0;
    }

    std::size_t count = 0;
    for (const auto& packet : packets) {
        m_file << packet.timestampNs << "," << packet.sensorId << "," << packet.accelX << "," << packet.accelY << ","
               << packet.accelZ << "," << packet.gyroX << "," << packet.gyroY << "," << packet.gyroZ << ","
               << packet.temperature << "\n";
        ++count;
    }
    m_file.flush();
    return count;
}

void CsvWriter::close()
{
    if (m_file.is_open()) {
        m_file.flush();
        m_file.close();
    }
}

} // namespace Communication
