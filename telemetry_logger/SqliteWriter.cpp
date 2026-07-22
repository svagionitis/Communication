/**
 * @file SqliteWriter.cpp
 * @brief High-throughput SQLite3 storage backend with WAL mode and transaction batching.
 */

#include "SqliteWriter.h"

#include <glog/logging.h>

namespace Communication {

SqliteWriter::~SqliteWriter()
{
    close();
}

bool SqliteWriter::open(const std::string& destination)
{
    close();

    if (sqlite3_open(destination.c_str(), &m_db) != SQLITE_OK) {
        LOG(ERROR) << "Failed to open SQLite database: " << (m_db ? sqlite3_errmsg(m_db) : "Unknown error");
        return false;
    }

    // Enable WAL mode and synchronous = NORMAL for high-performance concurrent writes
    char* errMsgs = nullptr;
    const char* pragmaQuery = "PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL;";
    if (sqlite3_exec(m_db, pragmaQuery, nullptr, nullptr, &errMsgs) != SQLITE_OK) {
        LOG(WARNING) << "Failed to set SQLite PRAGMA journal_mode=WAL: " << (errMsgs ? errMsgs : "");
        sqlite3_free(errMsgs);
    }

    const char* createTableQuery = R"(
        CREATE TABLE IF NOT EXISTS sensor_telemetry (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp_ns INTEGER NOT NULL,
            sensor_id TEXT NOT NULL,
            accel_x REAL NOT NULL,
            accel_y REAL NOT NULL,
            accel_z REAL NOT NULL,
            gyro_x REAL NOT NULL,
            gyro_y REAL NOT NULL,
            gyro_z REAL NOT NULL,
            temperature REAL NOT NULL
        );
    )";

    if (sqlite3_exec(m_db, createTableQuery, nullptr, nullptr, &errMsgs) != SQLITE_OK) {
        LOG(ERROR) << "Failed to create SQLite table: " << (errMsgs ? errMsgs : "");
        sqlite3_free(errMsgs);
        close();
        return false;
    }

    const char* insertSql = R"(
        INSERT INTO sensor_telemetry (
            timestamp_ns, sensor_id, accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z, temperature
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

    if (sqlite3_prepare_v2(m_db, insertSql, -1, &m_insertStmt, nullptr) != SQLITE_OK) {
        LOG(ERROR) << "Failed to prepare SQLite insert statement: " << sqlite3_errmsg(m_db);
        close();
        return false;
    }

    LOG(INFO) << "Opened SQLite telemetry database at: " << destination;
    return true;
}

std::size_t SqliteWriter::writeBatch(const std::vector<TelemetryPacket>& packets)
{
    if (!m_db || !m_insertStmt || packets.empty()) {
        return 0;
    }

    char* errMsgs = nullptr;
    if (sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsgs) != SQLITE_OK) {
        LOG(ERROR) << "Failed to begin SQLite transaction: " << (errMsgs ? errMsgs : "");
        sqlite3_free(errMsgs);
        return 0;
    }

    std::size_t count = 0;
    for (const auto& packet : packets) {
        sqlite3_reset(m_insertStmt);
        sqlite3_clear_bindings(m_insertStmt);

        sqlite3_bind_int64(m_insertStmt, 1, static_cast<sqlite3_int64>(packet.timestampNs));
        sqlite3_bind_text(m_insertStmt, 2, packet.sensorId, -1, SQLITE_STATIC);
        sqlite3_bind_double(m_insertStmt, 3, static_cast<double>(packet.accelX));
        sqlite3_bind_double(m_insertStmt, 4, static_cast<double>(packet.accelY));
        sqlite3_bind_double(m_insertStmt, 5, static_cast<double>(packet.accelZ));
        sqlite3_bind_double(m_insertStmt, 6, static_cast<double>(packet.gyroX));
        sqlite3_bind_double(m_insertStmt, 7, static_cast<double>(packet.gyroY));
        sqlite3_bind_double(m_insertStmt, 8, static_cast<double>(packet.gyroZ));
        sqlite3_bind_double(m_insertStmt, 9, static_cast<double>(packet.temperature));

        if (sqlite3_step(m_insertStmt) == SQLITE_DONE) {
            ++count;
        } else {
            LOG(WARNING) << "SQLite insert step failed: " << sqlite3_errmsg(m_db);
        }
    }

    if (sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, &errMsgs) != SQLITE_OK) {
        LOG(ERROR) << "Failed to commit SQLite transaction: " << (errMsgs ? errMsgs : "");
        sqlite3_free(errMsgs);
    }

    return count;
}

void SqliteWriter::close()
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
