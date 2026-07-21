# Role and Objective
You are an expert C++ embedded systems engineer specializing in high-performance, cross-platform networking, and hardware communication. Your goal is to write a clean, robust, and highly efficient C++17 library for TCP, UDP, and Serial communication (Windows/Linux), along with a CLI client and a high-performance Qt6 QML GUI client.

# Licensing Requirements (Dual License)
- **Licensing Model**: The entire codebase must be dual-licensed under the **GNU General Public License v3 (GPLv3)** and a **Commercial License**.
- **File Headers**: Every single source file (`.h`, `.cpp`, `.qml`, `CMakeLists.txt`) must begin with a clear, professional Doxygen/SPDX comment block stating this dual licensing structure (e.g., using `SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial`).

# Project Configuration & Version Control Files
- **EditorConfig**: Provide a production-ready `.editorconfig` file to enforce consistent coding styles (spaces vs tabs, line endings, charset) across different IDEs, matching the WebKit style guidelines.
- **Gitignore**: Provide a comprehensive `.gitignore` file tailored for modern C++ development using CMake, Ninja, Qt6, and popular IDEs (VS Code, Visual Studio, CLion), ensuring build artifacts and cache files are ignored.

# Architectural Requirement: Unified Abstract Interface
- **Abstract Base Class**: Define a pure virtual interface class named `ICommunication` within the core namespace.
- **Unified API Contract**: This interface must dictate the baseline behavior for all communication channels. It must include pure virtual methods for:
  - `open()` / `connect()` (with configuration parameter flexibility, using a common config variant or string/struct setup).
  - `close()` / `disconnect()`.
  - `send(const std::vector<uint8_t>& data)` / `send(std::string_view data)`.
  - `registerReceiveCallback(std::function<void(const std::vector<uint8_t>&)> callback)`.
  - `isOpen()` const / `isConnected()` const.
- **Polymorphic Implementations**: The specific communication modules (`TcpClient`, `UdpSocket`, and `SerialPort`) must publicly inherit from `ICommunication` and provide highly optimized, thread-safe implementations of these virtual methods.

# Resource Management & Documentation Requirements (Strict)
- **Strict RAII Pattern**: All system resources—including sockets (Winsock/POSIX), file descriptors, OS device handles, and background worker threads—must be strictly managed using Resource Acquisition Is Initialization (RAII). 
  - Resource allocation must happen during object construction.
  - Resource cleanup and graceful closure (e.g., closing sockets, releasing COM ports, joining threads) must happen automatically during object destruction.
  - Moving objects must transfer ownership cleanly, while copying must be explicitly disabled (`= delete`) where resource duplication is invalid.
- **Doxygen Documentation**: Every class, struct, enum, public/private method, and class member must be fully documented using standard **Doxygen comments** (using `/** ... */` style blocks). 
  - Use specific tags such as `@brief`, `@param`, `@return`, `@throws`, and `@note` where applicable to ensure professional-grade API documentation can be generated.

# Dependency Constraints (Strict)
- **Zero External Library Dependencies**: The core communication library must not use any third-party libraries for network or serial communication (e.g., No ASIO, No Boost, No libserialport). 
- **Allowed Frameworks**: The only permitted external dependencies are **Google Logging (glog)** for logging, **Google Test (gtest)** for unit tests, and the **Qt6 (Qt Quick/QML)** core modules required exclusively for the GUI client application.
- **Native OS API Implementation**: You must implement communication using purely native operating system APIs:
  - Windows: Winsock2 (`winsock2.h`, `ws2tcpip.h`) and Win32 Serial API (`windows.h`).
  - Linux: POSIX Sockets (`sys/socket.h`, `netinet/in.h`) and standard termios (`termios.h`, `fcntl.h`).

# Thread-Safety & Concurrency Requirements (Strict)
- **Library Thread-Safety**: All communication modules (`TcpServer`, `TcpClient`, `UdpSocket`, `SerialPort`) must be entirely thread-safe. Concurrent calls to send, receive, configure, or close connections from different threads must not cause race conditions or data corruption.
- **Internal Synchronization**: Use lightweight standard library synchronization primitives (`std::mutex`, `std::unique_lock`, `std::lock_guard`, `std::atomic`). Avoid heavy synchronization overhead to maintain high performance.
- **Thread-Safe Buffers**: Implement or use a lock-free or mutex-protected thread-safe ring buffer / queue (`std::vector` or `std::array` backed) for incoming and outgoing data packages.
- **Asynchronous Processing**: Reading and writing operations must run on dedicated background worker threads without blocking the main application or GUI thread.
- **Safe Callbacks / Observers**: Event notification callbacks (e.g., `onDataReceived`, `onDisconnected`) must be executed safely across thread boundaries, ensuring that any user-registered callback does not block the internal read loops.

# Technical & Performance Requirements (Embedded Focus)
- Language Standard: Strict C++17 (no C++20 features).
- Embedded Optimization: Code must be lightweight, minimize dynamic memory allocations (zero-copy paradigms where possible), and avoid high CPU overhead.

# Coding Style (WebKit Style)
- Style Guide: Follow the **WebKit Code Style** strictly.
- Key WebKit rules to apply: 
  - Use camelCase for variables and functions (e.g., `isDeviceReady`, `bytesReceived`).
  - Use PascalCase for class names and enums (e.g., `TcpClient`, `BaudRate`).
  - Braces match the statement line for `if`/`while`/`for`, but functions have braces on a new line.
  - Smart pointers and types must align with WebKit's clean formatting principles.

# Build System, Logging & Quality Assurance
- Build System: **CMake** configured for the **Ninja** generator. Ensure separate optimization flags for embedded build targets.
- **Conditional Compilation (Compilation Flags)**:
  - The Command Line Client (CLI) target must be enabled and built **by default**.
  - The Qt6 QML GUI Client target must be **optional** and gated behind a CMake option (e.g., `option(BUILD_GUI "Build the Qt6 QML GUI Client" OFF)`). Qt6 packages should only be searched and loaded if this flag is explicitly set to `ON`.
- Logging: Integrated **Google Logging (glog)**. Use optimized conditional logging to prevent logging overhead from hurting real-time communication loops.
- Unit Testing: Comprehensive test suite using **Google Test (gtest)**. Include multi-threaded stress tests to verify thread safety and polymorphic behavior.
- Git Automation: Provide a production-ready **pre-commit git hook** script. This hook must automatically run `clang-format` (configured to WebKit style) on staged files and block the commit if code formatting rules are violated.

# Client Applications to Implement
1. **Command Line Client (CLI)**: A lightweight, low-overhead console utility using native `argv` parsing for testing throughput. Built by default.
2. **High-Performance Qt6 QML GUI Client** (Optional Build): 
   - Framework: **Qt6 UI using QML / Qt Quick** (optimized for hardware-accelerated embedded displays).
   - C++ Integration: Create a clean C++ backend class (e.g., `CommunicationBridge`) exposed to QML via `Q_PROPERTY` or `QML_ELEMENT`. It should internally hold a pointer/variant to the `ICommunication` interface to demonstrate polymorphism.
   - Thread Integration: Ensure thread-safe communication between the background C++ communication threads and the Qt Main/QML Thread using Qt's `QMetaObject::invokeMethod` or safe signal-slot connections across threads to prevent GUI freezing.
   - Features: Modern QML layout with input fields (IP, Port, Baud Rate), a visually responsive Connect/Disconnect button, a real-time data log display area optimized for high-throughput text streaming, and a payload input field.

# Deliverable Format
Generate the project in a structured format:
1. **CMakeLists.txt**: Configured for C++17, Ninja, finding `glog` and `gtest`. Includes the `BUILD_GUI` option (default `OFF`) to conditionally find `Qt6` modules and build the GUI target.
2. **.editorconfig**: Full file specifying workspace and layout preferences.
3. **.gitignore**: Full file containing multi-platform and CMake/Qt6 ignores.
4. **.clang-format**: A configuration file explicitly defining the WebKit style setup.
5. **pre-commit**: A bash script ready to be placed inside `.git/hooks/`.
6. Library Headers & Sources (Common, `ICommunication` Interface, ThreadSafeQueue, TCP, UDP, Serial) complete with Doxygen comments, RAII structures, and Dual-License header notices.
7. **CLI Client Source (`main_cli.cpp`)**
8. **Qt6 QML Client C++ Backend (`CommunicationBridge.h`, `CommunicationBridge.cpp`, `main_gui.cpp`)** (Conditional on `BUILD_GUI`)
9. **Qt6 QML Client UI (`Main.qml`)** (Conditional on `BUILD_GUI`)
10. **Unit Tests File (`tests.cpp`)** including concurrency/stress tests.

