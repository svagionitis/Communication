/**
 * @file TrackingDatabase.cpp
 * @brief High-throughput SQLite3 database implementation for target position records.
 */

#include "TrackingDatabase.h"

#include <glog/logging.h>

namespace Communication {

TrackingDatabase::~TrackingDatabase()
{
    close();
}

bool TrackingDatabase::open(const std::string& dbPath)
{
    close();

    if (sqlite3_open(dbPath.c_str(), &m_db) != SQLITE_OK) {
        LOG(ERROR) << "Failed to open SQLite database: " << (m_db ? sqlite3_errmsg(m_db) : "Unknown error");
        return false;
    }

    char* errMsgs = nullptr;
    const char* pragmaQuery = "PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL;";
    if (sqlite3_exec(m_db, pragmaQuery, nullptr, nullptr, &errMsgs) != SQLITE_OK) {
        LOG(WARNING) << "Failed to set SQLite PRAGMA journal_mode=WAL: " << (errMsgs ? errMsgs : "");
        sqlite3_free(errMsgs);
    }

    const char* createTableQuery = R"(
        CREATE TABLE IF NOT EXISTS target_positions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp_ns INTEGER NOT NULL,
            domain TEXT NOT NULL,
            target_id TEXT NOT NULL,
            name_callsign TEXT NOT NULL,
            latitude REAL NOT NULL,
            longitude REAL NOT NULL,
            altitude_ft REAL NOT NULL,
            speed_kts REAL NOT NULL,
            heading_deg REAL NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_target_id ON target_positions(target_id);
        CREATE INDEX IF NOT EXISTS idx_domain ON target_positions(domain);
    )";

    if (sqlite3_exec(m_db, createTableQuery, nullptr, nullptr, &errMsgs) != SQLITE_OK) {
        LOG(ERROR) << "Failed to create target_positions table: " << (errMsgs ? errMsgs : "");
        sqlite3_free(errMsgs);
        close();
        return false;
    }

    const char* insertSql = R"(
        INSERT INTO target_positions (
            timestamp_ns, domain, target_id, name_callsign, latitude, longitude, altitude_ft, speed_kts, heading_deg
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

    if (sqlite3_prepare_v2(m_db, insertSql, -1, &m_insertStmt, nullptr) != SQLITE_OK) {
        LOG(ERROR) << "Failed to prepare SQLite target insert statement: " << sqlite3_errmsg(m_db);
        close();
        return false;
    }

    LOG(INFO) << "Opened AIS/ADS-B spatial target database at: " << dbPath;
    return true;
}

std::size_t TrackingDatabase::insertBatch(const std::vector<TargetFrame>& frames)
{
    if (!m_db || !m_insertStmt || frames.empty()) {
        return 0;
    }

    char* errMsgs = nullptr;
    if (sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsgs) != SQLITE_OK) {
        LOG(ERROR) << "Failed to begin SQLite transaction: " << (errMsgs ? errMsgs : "");
        sqlite3_free(errMsgs);
        return 0;
    }

    std::size_t count = 0;
    for (const auto& f : frames) {
        sqlite3_reset(m_insertStmt);
        sqlite3_clear_bindings(m_insertStmt);

        const char* domainStr = (f.domain == DomainType::Maritime_AIS) ? "MARITIME_AIS" : "AVIATION_ADSB";

        sqlite3_bind_int64(m_insertStmt, 1, static_cast<sqlite3_int64>(f.timestampNs));
        sqlite3_bind_text(m_insertStmt, 2, domainStr, -1, SQLITE_STATIC);
        sqlite3_bind_text(m_insertStmt, 3, f.targetId, -1, SQLITE_STATIC);
        sqlite3_bind_text(m_insertStmt, 4, f.nameCallsign, -1, SQLITE_STATIC);
        sqlite3_bind_double(m_insertStmt, 5, f.latitude);
        sqlite3_bind_double(m_insertStmt, 6, f.longitude);
        sqlite3_bind_double(m_insertStmt, 7, f.altitudeFeet);
        sqlite3_bind_double(m_insertStmt, 8, static_cast<double>(f.speedKnots));
        sqlite3_bind_double(m_insertStmt, 9, static_cast<double>(f.headingDegrees));

        if (sqlite3_step(m_insertStmt) == SQLITE_DONE) {
            ++count;
        } else {
            LOG(WARNING) << "SQLite step failed: " << sqlite3_errmsg(m_db);
        }
    }

    if (sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, &errMsgs) != SQLITE_OK) {
        LOG(ERROR) << "Failed to commit SQLite transaction: " << (errMsgs ? errMsgs : "");
        sqlite3_free(errMsgs);
    }

    return count;
}

void TrackingDatabase::close()
{
    if (m_insertStmt) {
        sqlite3_finalize(m_insertStmt);
        m_insertStmt = nullptr;
    }
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

} // namespace Communication
