cmake_minimum_required(VERSION 3.16)
project(Communication VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build Options
option(BUILD_GUI "Build the Qt6 QML GUI Client" OFF)
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build lock-free vs mutex queue benchmark suite" OFF)
option(BUILD_TELEMETRY_LOGGER "Build high-frequency telemetry SQLite/CSV/Binary logger" OFF)
option(BUILD_PROTOCOL_GATEWAY "Build low-latency full-duplex serial-to-TCP protocol gateway" OFF)
option(BUILD_TRACKING_HUB "Build AIS and ADS-B telemetry tracking hub" OFF)

# Compiler Flags & Modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerFlags)

# Find Common Dependencies
find_package(glog REQUIRED)
find_package(GTest REQUIRED)

# Core Communication Library
add_subdirectory(lib)

# CLI Client Executable (Built by default)
add_subdirectory(cli)

# Qt6 QML GUI Client (Conditional)
if(BUILD_GUI)
    add_subdirectory(gui)
endif()

# Unit Testing (Conditional)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Benchmark Suite (Conditional)
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()

# Telemetry Logger (Conditional)
if(BUILD_TELEMETRY_LOGGER)
    add_subdirectory(telemetry_logger)
endif()

# Protocol Gateway (Conditional)
if(BUILD_PROTOCOL_GATEWAY)
    add_subdirectory(protocol_gateway)
endif()

# AIS & ADS-B Tracking Hub (Conditional)
if(BUILD_TRACKING_HUB)
    add_subdirectory(tracking_hub)
endif()

# Clang-Format Code Formatting Target
find_program(CLANG_FORMAT_BIN NAMES clang-format clang-format-15 clang-format-14 clang-format-12)
if(CLANG_FORMAT_BIN)
    file(GLOB_RECURSE ALL_FORMAT_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cli/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/*.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/telemetry_logger/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/telemetry_logger/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol_gateway/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol_gateway/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tracking_hub/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/tracking_hub/*.cpp
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_BIN} -i -style=file ${ALL_FORMAT_FILES}
        COMMENT "Applying clang-format (WebKit style) to all C++/QML files..."
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_BIN} --dry-run --Werror -style=file ${ALL_FORMAT_FILES}
        COMMENT "Checking clang-format (WebKit style) compliance for all C++/QML files..."
    )
else()
    message(STATUS "clang-format executable not found. 'format' target disabled.")
endif()

# Automatically install Git pre-commit hook on CMake configuration
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/pre-commit")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/scripts/pre-commit"
        DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/.git/hooks"
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
    message(STATUS "Git pre-commit hook successfully installed to .git/hooks/pre-commit")
endif()
