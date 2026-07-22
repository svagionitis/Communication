find_package(SQLite3 REQUIRED)

add_executable(telemetry_logger
    SqliteWriter.cpp
    CsvWriter.cpp
    BinaryWriter.cpp
    TelemetryIngestor.cpp
    main.cpp
)

target_link_libraries(telemetry_logger PRIVATE Communication SQLite::SQLite3 glog::glog Threads::Threads)
apply_compiler_flags(telemetry_logger)
