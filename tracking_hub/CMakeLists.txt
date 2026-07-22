find_package(SQLite3 REQUIRED)

add_executable(tracking_hub
    HttpClient.cpp
    OpenSkyProvider.cpp
    AisStreamProvider.cpp
    SimulatorProvider.cpp
    ProviderRegistry.cpp
    Parsers.cpp
    TrackingDatabase.cpp
    TrackingEngine.cpp
    main.cpp
)

target_link_libraries(tracking_hub PRIVATE Communication SQLite::SQLite3 glog::glog Threads::Threads)
apply_compiler_flags(tracking_hub)
