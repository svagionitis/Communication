# Modern C++17 Cross-Platform Communication Library

A high-performance, lightweight, dual-licensed (GPLv3 / Commercial) C++17 library providing unified interfaces for **TCP**, **UDP**, and **Serial** communication across **Linux** and **Windows**.

Zero third-party network/serial dependencies (uses pure native OS Winsock2 & Win32 Serial APIs on Windows, POSIX Sockets & termios on Linux). Includes a Command Line Interface (CLI) application, an optional Qt6 QML hardware-accelerated GUI client, a Google Test suite, and automated code formatting tools.

---

## Key Features

- **Unified Abstract Interface**: All communication channels inherit from `ICommunication`, enforcing a consistent API contract (`open()`, `connect()`, `close()`, `disconnect()`, `send()`, `registerReceiveCallback()`, `isOpen()`, `isConnected()`).
- **Zero Third-Party Socket/Serial Dependencies**: Pure native OS implementations without Asio, Boost, or libserialport.
- **Connection Statistics & Metrics (`ConnectionStats`)**: Real-time telemetry tracking (`bytesSent`, `bytesReceived`, `packetsSent`, `packetsReceived`, `reconnectCount`, `connectTimestamp`, `sendBytesPerSec`, `rxBytesPerSec`) across all channel types with zero-overhead atomic counters.
- **Lock-Free SPSC Ring Buffer (`LockFreeRingBuffer<T, Capacity>`)**: High-performance, header-only Single-Producer Single-Consumer lock-free ring buffer with 64-byte cache-line alignment to eliminate lock contention, mutex overhead, and false sharing in real-time processing threads.
- **Zero-Copy High-Rate Callbacks**: Non-allocating callback overloads (`DataViewCallback` / `StringViewCallback`) passing `const uint8_t*` and `size_t` / `std::string_view` to eliminate dynamic `std::vector` heap allocations in high-throughput receive loops.
- **Auto-Reconnect Manager (`ReconnectPolicy`)**: Automatic non-blocking connection recovery with configurable exponential backoff (`initialDelayMs`, `maxDelayMs`, `backoffMultiplier`, `maxRetries`) for `TcpClient` and `SerialPort`.
- **Strict RAII Resource Management**: Complete exception safety, automatic thread joining, and resource cleanup on object destruction.
- **TCP Client & Server Keep-Alive**: Native cross-platform TCP Keep-Alive options (`SO_KEEPALIVE`, `TCP_KEEPIDLE`, `TCP_KEEPINTVL`, `TCP_KEEPCNT`, `WSAIoctl`) to detect dead sockets and broken connections across firewalls.
- **Thread-Safe & Async**: Non-blocking background worker threads handle incoming byte streams without locking main application or GUI threads.
- **Full Serial Baud Rate & Flow Control Support**: Full support for standard and custom baud rates ranging from `110` up to `4,000,000` baud, with configurable parity, stop bits, data bits, and flow control (`None`, `Hardware RTS/CTS`, `Software XON/XOFF`).
- **CLI Client**: Built-in interactive command-line tool (`communication_cli`) built by default.
- **Qt6 QML GUI Client**: Modern hardware-accelerated GUI application (`communication_gui`) built optionally with `-DBUILD_GUI=ON`.
- **Quality Assurance & Code Style**:
  - Full Google Test (`gtest`) unit test suite covering `ThreadSafeQueue`, TCP loopback, UDP loopback, and mock serial port operations.
  - WebKit C++ style enforced via `.clang-format` and `.editorconfig`.
  - Built-in CMake `format` and `format-check` targets.
  - Automatic Git pre-commit hook installation upon CMake configuration.
  - `compile_commands.json` generation enabled (`CMAKE_EXPORT_COMPILE_COMMANDS ON`).

---

## Directory Layout

```
.
├── CMakeLists.txt              # Root CMake configuration
├── LICENSE                     # Dual GPLv3 & Commercial License notice
├── LLM.md                      # Architecture and design specifications
├── docs/
│   └── c4_architecture.md      # Detailed C4 Model Architecture (ASCII & Mermaid diagrams)
├── .editorconfig               # EditorConfig rules (WebKit style)
├── .gitignore                  # Git ignore rules for CMake, Qt, & IDEs
├── .clang-format               # WebKit C++ style formatting config
├── scripts/
│   └── pre-commit              # Git pre-commit formatting hook
├── lib/
│   ├── CMakeLists.txt          # Library build targets
│   ├── ICommunication.h        # Baseline polymorphic interface
│   ├── Types.h                 # Config structs, enums (BaudRate, Parity, etc.)
│   ├── ThreadSafeQueue.h       # Thread-safe queue template
│   ├── TcpClient.h / .cpp      # TCP Client implementation
│   ├── TcpServer.h / .cpp      # Multi-client TCP Server implementation
│   ├── UdpSocket.h / .cpp      # UDP Unicast & Broadcast socket
│   ├── SerialPort.h / .cpp     # Native Win32 & termios Serial implementation
│   └── Platform/
│       ├── SocketUtils.h       # Cross-platform socket initialization & handles
│       └── SocketUtils.cpp
├── cli/
│   ├── CMakeLists.txt          # CLI executable build target
│   └── main_cli.cpp            # Command-line application entry point
├── gui/
│   ├── CMakeLists.txt          # Qt6 QML GUI executable target
│   ├── CommunicationBridge.h   # Qt QObject bridge exposing C++ API to QML
│   ├── CommunicationBridge.cpp
│   ├── main_gui.cpp            # Qt application main entry point
│   └── Main.qml                # Dark-mode hardware-accelerated QML layout
└── tests/
    ├── CMakeLists.txt          # Google Test suite build target
    └── unit_tests.cpp          # Unit tests for queue, TCP, UDP, and mock Serial
```

---

## Prerequisites

- **C++17 Compiler**: GCC 7+, Clang 6+, or MSVC 2017+
- **Build System**: CMake (>= 3.16) and Ninja
- **Dependencies**:
  - `glog` (Google Logging)
  - `gtest` (Google Test)
  - *(Optional)* `Qt6` (Quick and Qml modules, required only if `-DBUILD_GUI=ON`)

On Ubuntu/Debian:
```bash
sudo apt update
sudo apt install build-essential cmake ninja-build libgoogle-glog-dev libgtest-dev qt6-base-dev qt6-declarative-dev clang-format
```

---

## Build Options & Sanitizers

| CMake Option | Default | Description |
|---|---|---|
| `BUILD_GUI` | `OFF` | Build the Qt6 QML GUI Client target |
| `BUILD_TESTS` | `ON` | Build Google Test unit test suite |
| `BUILD_BENCHMARKS` | `OFF` | Build SPSC Lock-Free vs Mutex Queue benchmark suite |
| `BUILD_TELEMETRY_LOGGER` | `OFF` | Build high-frequency telemetry SQLite/CSV/Binary logger |
| `BUILD_PROTOCOL_GATEWAY` | `OFF` | Build low-latency full-duplex Serial-to-TCP protocol gateway |
| `WARNINGS_AS_ERRORS` | `OFF` | Treat all compiler warnings as errors (`-Werror` / `/WX`) |
| `ENABLE_HARDENING` | `ON` | Enable security hardening flags (`-fstack-protector-strong`, `_FORTIFY_SOURCE=2`, RELRO, PIE) |
| `ENABLE_ASAN` | `OFF` | Enable AddressSanitizer (ASan) for memory leak & buffer overflow detection |
| `ENABLE_UBSAN` | `OFF` | Enable UndefinedBehaviorSanitizer (UBSan) |
| `ENABLE_TSAN` | `OFF` | Enable ThreadSanitizer (TSan) for data race detection |

Example enabling Warnings as Errors & Sanitizers:
```bash
cmake -B build -GNinja -DWARNINGS_AS_ERRORS=ON -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
ninja -C build
```

---

## Running Applications and Tests

### Run Unit Tests
```bash
ctest --test-dir build --output-on-failure
```

### Run Lock-Free vs Mutex Benchmark Suite
```bash
# Build benchmark suite
cmake -B build -GNinja -DBUILD_BENCHMARKS=ON
ninja -C build lockfree_benchmark

# Run benchmark (2,000,000 iterations by default)
./build/benchmark/lockfree_benchmark --iterations 2000000
```

### Run High-Frequency Telemetry Logger (SQLite / CSV / Binary)
```bash
# Build telemetry logger MVP
cmake -B build -GNinja -DBUILD_TELEMETRY_LOGGER=ON
ninja -C build telemetry_logger

# 1. Run in SQLite format mode (default)
./build/telemetry_logger/telemetry_logger --mode=simulate --format=sqlite --rate=20000 --duration=5 --out=telemetry.db

# 2. Run in CSV text format mode
./build/telemetry_logger/telemetry_logger --mode=simulate --format=csv --rate=20000 --duration=5 --out=telemetry.csv

# 3. Run in Binary packed format mode
./build/telemetry_logger/telemetry_logger --mode=simulate --format=binary --rate=20000 --duration=5 --out=telemetry.bin
```

### Run Full-Duplex Industrial Protocol Gateway (Serial-to-TCP Bridge)
```bash
# Build protocol gateway MVP
cmake -B build -GNinja -DBUILD_PROTOCOL_GATEWAY=ON
ninja -C build protocol_gateway

# Run full-duplex simulation benchmark mode
./build/protocol_gateway/protocol_gateway --mode=simulate --messages=200000

# Run live hardware bridge mode
./build/protocol_gateway/protocol_gateway --mode=bridge --device=/dev/ttyUSB0 --baud=115200 --host=192.168.1.100 --port=502
```

### Run Command Line Client (CLI)
```bash
# TCP Client Mode
./build/cli/communication_cli --mode=tcp --host=127.0.0.1 --port=8080 --interactive

# UDP Socket Mode
./build/cli/communication_cli --mode=udp --host=127.0.0.1 --port=8080 --interactive

# Serial Port Mode
./build/cli/communication_cli --mode=serial --device=/dev/ttyUSB0 --baud=115200 --interactive
```

### Run Qt6 QML GUI Client
```bash
./build_gui/gui/communication_gui
```

---

## Developer Tooling & Formatting

### Apply Clang-Format Code Formatting
```bash
ninja -C build format
```

### Check Clang-Format Compliance
```bash
ninja -C build format-check
```

### Git Pre-Commit Hook
Running `cmake -B build` automatically installs `.git/hooks/pre-commit`. Any staged C++ or QML code that violates WebKit formatting rules will automatically block the commit.

---

## Code Example

```cpp
#include "TcpClient.h"
#include <iostream>
#include <memory>

int main()
{
    Communication::TcpConfig config;
    config.host = "127.0.0.1";
    config.port = 8080;

    std::unique_ptr<Communication::ICommunication> client =
        std::make_unique<Communication::TcpClient>(config);

    // Register async receive callback
    client->registerReceiveCallback([](const std::vector<uint8_t>& data) {
        std::string text(reinterpret_cast<const char*>(data.data()), data.size());
        std::cout << "Received: " << text << std::endl;
    });

    if (client->open()) {
        client->send("Hello, Server!");
        client->close();
    }
    return 0;
}
```

---

## Licensing

This project is dual-licensed under:
1. **GNU General Public License v3.0 (GPLv3)** or later.
2. **Commercial License** for proprietary and non-GPL commercial use.

See the [LICENSE](LICENSE) file for complete details.
