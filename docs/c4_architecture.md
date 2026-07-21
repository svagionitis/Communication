# C4 Architecture Model - Modern C++17 Communication Library

This document outlines the software architecture for the cross-platform **Communication Library** using the **C4 Model** (Context, Containers, Components, and Code). It includes both **ASCII** and **Mermaid** diagrams for each architectural level.

---

## 1. System Context Diagram (Level 1)

The System Context diagram illustrates how human actors and external hardware/network entities interact with the Communication System.

### ASCII Diagram

```
+-------------------+        +-------------------+        +---------------------+
|   System Operator |        |   CLI / GUI User  |        | Hardware Technician |
+-------------------+        +-------------------+        +---------------------+
          |                            |                             |
          +----------------------------+-----------------------------+
                                       |
                                       v
                     +-----------------------------------+
                     |   Communication System            |
                     |   (C++17 TCP/UDP/Serial Lib)      |
                     +-----------------------------------+
                                       |
       +-------------------------------+-------------------------------+
       |                               |                               |
       v                               v                               v
+------------------+          +------------------+          +------------------+
|  Remote TCP Peer |          |  Remote UDP Peer |          | Serial Port Device|
|  (Server/Client) |          | (Unicast/Bcast)  |          | (RS232/RS485 TTY)|
+------------------+          +------------------+          +------------------+
```

### Mermaid Diagram

```mermaid
graph TD
    User["User / System Operator"]
    CommSystem["Communication System<br/>(C++17 TCP/UDP/Serial Suite)"]
    TcpPeer["Remote TCP Endpoint<br/>(Server / Client Socket)"]
    UdpPeer["Remote UDP Endpoint<br/>(Unicast / Broadcast)"]
    SerialDev["Serial Port Hardware<br/>(RS-232 / RS-485 / USB TTY)"]

    User -->|"Interacts via CLI / QML GUI"| CommSystem
    CommSystem -->|"TCP Streams (Winsock/POSIX)"| TcpPeer
    CommSystem -->|"UDP Datagrams (Winsock/POSIX)"| UdpPeer
    CommSystem -->|"Serial I/O (Win32 COMM / termios)"| SerialDev
```

---

## 2. Container Diagram (Level 2)

The Container diagram shows the high-level executables and static library targets produced by the CMake build system.

### ASCII Diagram

```
+-----------------------------------------------------------------------------------+
| Communication Repository Boundary                                                 |
|                                                                                   |
|  +---------------------------+                +---------------------------------+  |
|  | CLI Client                |                | Qt6 QML GUI Client              |  |
|  | (communication_cli)       |                | (communication_gui)             |  |
|  +---------------------------+                +---------------------------------+  |
|                |                                              |                   |
|                +-----------------------+----------------------+                   |
|                                        |                                          |
|                                        v                                          |
|                       +---------------------------------+                         |
|                       | Core Static Library             |                         |
|                       | (libCommunication.a)            |                         |
|                       +---------------------------------+                         |
|                                        ^                                          |
|                                        |                                          |
|                       +---------------------------------+                         |
|                       | Google Test Suite               |                         |
|                       | (communication_tests)           |                         |
|                       +---------------------------------+                         |
+-----------------------------------------------------------------------------------+
```

### Mermaid Diagram

```mermaid
graph TD
    subgraph "Communication System Boundary"
        CLI["CLI Executable<br/>(communication_cli)"]
        GUI["Qt6 QML GUI Executable<br/>(communication_gui)"]
        Tests["Google Test Suite<br/>(communication_tests)"]
        CoreLib["Core Static Library<br/>(libCommunication.a)"]
    end

    CLI -->|"Links & calls ICommunication API"| CoreLib
    GUI -->|"Uses CommunicationBridge QObject"| CoreLib
    Tests -->|"Stress tests & validates polymorphism"| CoreLib
```

---

## 3. Component Diagram (Level 3)

The Component diagram reveals the internal software components inside `libCommunication.a` and how they interface with the operating system kernel.

### ASCII Diagram

```
+---------------------------------------------------------------------------------------+
| Core Static Library (libCommunication.a)                                             |
|                                                                                       |
|      +-------------------------------------------------------------------------+      |
|      |                        ICommunication (Abstract API)                    |      |
|      +-------------------------------------------------------------------------+      |
|           ^                         ^                        ^                        |
|           |                         |                        |                        |
|  +------------------+      +------------------+     +------------------+              |
|  |    TcpClient     |      |    UdpSocket     |     |    SerialPort    |              |
|  |    TcpServer     |      |                  |     |                  |              |
|  +------------------+      +------------------+     +------------------+              |
|           |                         |                        |                        |
|           |  +-------------------+  |                        |                        |
|           +->|  ThreadSafeQueue  |<-+                        |                        |
|              +-------------------+                           v                        |
|                        |                             +------------------+             |
|                        v                             | Native termios   |             |
|              +-------------------+                   | & Win32 COMM API |             |
|              |    SocketUtils    |                   +------------------+             |
|              |  (Winsock/POSIX)  |                            |                       |
|              +-------------------+                            |                       |
+------------------------|--------------------------------------|-----------------------+
                         v                                      v
          +-------------------------------+    +----------------------------------+
          | OS Network Stack (TCP/IP)     |    | OS Serial Driver (/dev/tty, COM) |
          +-------------------------------+    +----------------------------------+
```

### Mermaid Diagram

```mermaid
graph TD
    subgraph "Core Library (libCommunication.a)"
        IComm["ICommunication Interface<br/>(Pure Virtual API Contract)"]
        TCP["TcpClient / TcpServer<br/>(Thread-Safe TCP Sockets)"]
        UDP["UdpSocket<br/>(Thread-Safe UDP Sockets)"]
        Serial["SerialPort<br/>(Native Serial Communication)"]
        TSQueue["ThreadSafeQueue<T><br/>(Mutex & CondVar Protected Buffer)"]
        SockUtils["Platform::SocketUtils<br/>(Winsock2 & POSIX Abstraction)"]
    end

    subgraph "Operating System Kernel"
        OSNetwork["OS Network Stack<br/>(sys/socket.h / winsock2.h)"]
        OSSerial["OS Serial Driver<br/>(termios.h / win32 comm)"]
    end

    TCP -.->|"Implements"| IComm
    UDP -.->|"Implements"| IComm
    Serial -.->|"Implements"| IComm

    TCP -->|"Buffers incoming/outgoing frames"| TSQueue
    UDP -->|"Buffers incoming/outgoing frames"| TSQueue

    TCP -->|"Cross-Platform Socket Calls"| SockUtils
    UDP -->|"Cross-Platform Socket Calls"| SockUtils

    SockUtils -->|"Direct OS Kernel Sockets"| OSNetwork
    Serial -->|"Direct OS Kernel Serial I/O"| OSSerial
```

---

## 4. Code & Class Architecture (Level 4)

Level 4 detailed class interactions and memory management paradigms.

### ASCII Class Hierarchy

```
                      +-------------------+
                      |   ICommunication  |
                      +-------------------+
                      | + open() : bool   |
                      | + close() : void  |
                      | + send() : bool   |
                      | + isOpen(): bool  |
                      +-------------------+
                                ^
        +-----------------------+-----------------------+
        |                       |                       |
+---------------+       +---------------+       +---------------+
|   TcpClient   |       |   UdpSocket   |       |   SerialPort  |
+---------------+       +---------------+       +---------------+
| - m_socket    |       | - m_socket    |       | - m_handle    |
| - m_thread    |       | - m_thread    |       | - m_thread    |
+---------------+       +---------------+       +---------------+
```

### Mermaid Threading & Data Flow Diagram

```mermaid
sequenceDiagram
    autonumber
    participant App as Application / GUI Thread
    participant Comm as TcpClient / UdpSocket / SerialPort
    participant Worker as Background Receive Thread
    participant OS as OS Kernel API

    App->>Comm: open()
    Comm->>OS: socket() / open()
    Comm->>Worker: std::thread(receiveLoop)
    App->>Comm: registerReceiveCallback(cb)
    App->>Comm: send(data)
    Comm->>OS: send() / write()
    Worker->>OS: recv() / read() (blocking/timeout)
    OS-->>Worker: Data Bytes Received
    Worker->>App: cb(data) via thread-safe dispatch
    App->>Comm: close() / Destructor (RAII)
    Comm->>Worker: join() & terminate
    Comm->>OS: closeSocket() / CloseHandle()
```

---

## Architectural Principles & Design Guarantees

1. **RAII & Exception Safety**: All OS handles (`SocketHandle`, `SerialHandle`) and worker threads (`std::thread`) are bound to object lifetimes. Cleanup is guaranteed on object destruction.
2. **WebKit Code Style**: Enforced by `.clang-format` and `.editorconfig`.
3. **Thread Safety**: All public methods (`open`, `close`, `send`, `registerReceiveCallback`) use lightweight standard library primitives (`std::mutex`, `std::atomic`, `std::unique_lock`).
4. **Zero Communication Dependencies**: Pure native system calls with no external network/serial dependencies.
