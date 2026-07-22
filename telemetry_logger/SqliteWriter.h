/**
 * @file SqliteWriter.h
 * @brief SQLite3 transactional batch writer backend for telemetry data.
 */

#pragma once

#include "ITelemetryWriter.h"

#include <sqlite3.h>

namespace Communication {

class SqliteWriter : public ITelemetryWriter {
public:
    SqliteWriter() = default;
    ~SqliteWriter() override;

    bool open(const std::string& destination) override;
    std::size_t writeBatch(const std::vector<TelemetryPacket>& packets) override;
    void close() override;

private:
    sqlite3* m_db {nullptr};
    sqlite3_stmt* m_insertStmt {nullptr};
};

} // namespace Communication
