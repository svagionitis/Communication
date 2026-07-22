/**
 * @file TrackingDatabase.h
 * @brief SQLite3 spatial telemetry trajectory storage backend for AIS & ADS-B targets.
 */

#pragma once

#include "TargetTypes.h"

#include <cstddef>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace Communication {

class TrackingDatabase {
public:
    TrackingDatabase() = default;
    ~TrackingDatabase();

    bool open(const std::string& dbPath);
    std::size_t insertBatch(const std::vector<TargetFrame>& frames);
    void close();

private:
    sqlite3* m_db {nullptr};
    sqlite3_stmt* m_insertStmt {nullptr};
};

} // namespace Communication
