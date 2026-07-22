/**
 * @file BinaryWriter.cpp
 * @brief Packed binary telemetry file writer backend.
 */

#include "BinaryWriter.h"

#include <glog/logging.h>

namespace Communication {

struct BinaryHeader {
    char magic[4] {'T', 'L', 'O', 'G'};
    uint32_t version {1};
    uint32_t packetSize {sizeof(TelemetryPacket)};
    uint32_t reserved {0};
};

BinaryWriter::~BinaryWriter()
{
    close();
}

bool BinaryWriter::open(const std::string& destination)
{
    close();

    m_file.open(destination, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!m_file.is_open()) {
        LOG(ERROR) << "Failed to open binary output file: " << destination;
        return false;
    }

    BinaryHeader header {};
    m_file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    LOG(INFO) << "Opened binary telemetry log at: " << destination;
    return true;
}

std::size_t BinaryWriter::writeBatch(const std::vector<TelemetryPacket>& packets)
{
    if (!m_file.is_open() || packets.empty()) {
        return 0;
    }

    m_file.write(reinterpret_cast<const char*>(packets.data()), packets.size() * sizeof(TelemetryPacket));
    m_file.flush();
    return packets.size();
}

void BinaryWriter::close()
{
    if (m_file.is_open()) {
        m_file.flush();
        m_file.close();
    }
}

} // namespace Communication
