# C4 Architecture Model — Low-Latency Protocol Gateway

This document outlines the software architecture for the high-performance, full-duplex **Serial-to-TCP Industrial Protocol Gateway** (`protocol_gateway`) using the **C4 Model** (Context, Containers, Components, and Code). It includes both **ASCII** and **Mermaid** diagrams for each architectural level.

---

## 1. System Context Diagram (Level 1)

The System Context diagram illustrates how SCADA operators, field serial devices, and remote TCP servers interact with the Protocol Gateway System.

### ASCII Diagram

```
+------------------------+                             +------------------------+
| Industrial Operator /  |                             | Remote SCADA / Cloud   |
| Field Technician       |                             | Central Control Server |
+------------------------+                             +------------------------+
            │                                                      │
            │ Configures & Monitors                                │ Communicates over TCP
            ▼                                                      ▼
+---------------------------------------------------------------------------------------+
| Low-Latency Full-Duplex Protocol Gateway (protocol_gateway)                           |
+---------------------------------------------------------------------------------------+
            │
            │ Communicates over Serial (RS-232 / RS-485 / Modbus RTU)
            ▼
+---------------------------------------------------------------------------------------+
| Industrial Field Device (PLC, Sensor Array, Microcontroller, RTU)                     |
+---------------------------------------------------------------------------------------+
```

### Mermaid Diagram

```mermaid
graph TD
    Operator["Industrial / SCADA Operator"]
    Gateway["Protocol Gateway System<br/>(protocol_gateway Executable)"]
    SerialDevice["Industrial Serial Hardware<br/>(PLC / RS-485 Modbus RTU / Sensor)"]
    TcpServer["Remote TCP Server / Cloud Host<br/>(Modbus TCP / Telemetry Server)"]

    Operator -->|"Monitors bridge stats & configures"| Gateway
    Gateway <-->|"Full-Duplex Serial Stream (termios / Win32)"| SerialDevice
    Gateway <-->|"Full-Duplex TCP Socket Stream (Winsock / POSIX)"| TcpServer
```

---

## 2. Container Diagram (Level 2)

The Container diagram illustrates the high-level process boundary, library components, hardware devices, and network sockets comprising the Gateway subsystem.

### ASCII Diagram

```
+-----------------------------------------------------------------------------------------+
| Protocol Gateway Subsystem Boundary                                                     |
|                                                                                         |
|  +-----------------------------------------------------------------------------------+  |
|  | Protocol Gateway Executable (protocol_gateway)                                    |  |
|  |                                                                                   |  |
|  |  +─────────────────────────+                   +───────────────────────────────+  |  |
|  |  | serialToTcpWorker       |                   | tcpToSerialWorker             |  |  |
|  |  | (Thread 1)              |                   | (Thread 2)                    |  |  |
|  |  +─────────────────────────+                   +───────────────────────────────+  |  |
|  |               │                                                │                  |  |
|  |               ▼                                                ▼                  |  |
|  |  +─────────────────────────+                   +───────────────────────────────+  |  |
|  |  | m_serialToTcpRing       |                   | m_tcpToSerialRing             |  |  |
|  |  | (LockFreeRingBuffer)    |                   | (LockFreeRingBuffer)          |  |  |
|  |  +─────────────────────────+                   +───────────────────────────────+  |  |
|  |                                                                                   |  |
|  +────────────────────────────────────────┼──────────────────────────────────────────+  |
|                                           │                                             |
|                    ┌──────────────────────┴──────────────────────┐                      |
|                    ▼                                             ▼                      |
|         +─────────────────────+                       +─────────────────────+           |
|         | SerialPort Hardware |                       | Remote TCP Server   |           |
|         | (/dev/ttyUSB0)      |                       | (192.168.1.100:502) |           |
|         +─────────────────────+                       +─────────────────────+           |
+-----------------------------------------------------------------------------------------+
```

### Mermaid Diagram

```mermaid
graph TD
    subgraph "Protocol Gateway Process Boundary"
        App["Gateway Main Application<br/>(main.cpp)"]
        Bridge["DuplexBridge Engine<br/>(DuplexBridge.cpp)"]
        
        subgraph "Directional Lock-Free Pipelines"
            S2T_Ring["m_serialToTcpRing<br/>(LockFreeRingBuffer<GatewayFrame, 4096>)"]
            T2S_Ring["m_tcpToSerialRing<br/>(LockFreeRingBuffer<GatewayFrame, 4096>)"]
        end
        
        subgraph "Communication Channels"
            Serial["SerialPort Module<br/>(termios / Win32 Serial API)"]
            TCP["TcpClient Module<br/>(POSIX / Winsock2 Sockets)"]
        end
    end

    SerialHW["Hardware Serial Device<br/>(RS-232 / RS-485 / TTY)"]
    TcpServer["Remote TCP Host<br/>(Modbus TCP Server)"]

    App --> Bridge
    Bridge --> S2T_Ring
    Bridge --> T2S_Ring
    Bridge --> Serial
    Bridge --> TCP
    Serial <-->|"TTY / COM I/O"| SerialHW
    TCP <-->|"TCP Socket Connection"| TcpServer
```

---

## 3. Component Diagram (Level 3)

The Component diagram shows the internal C++ class components, lock-free queues, and worker threads within `protocol_gateway`.

### ASCII Diagram

```
+-----------------------------------------------------------------------------------+
| protocol_gateway Component Architecture                                           |
|                                                                                   |
|                   +------------------------------------------+                    |
|                   | DuplexBridge Engine                      |                    |
|                   +------------------------------------------+                    |
|                        │                                │                         |
|      ┌─────────────────┘                                └─────────────────┐       |
|      ▼                                                                    ▼       |
| +─────────────────────────+                            +────────────────────────+ |
| | SerialPort              |                            | TcpClient              | |
| +─────────────────────────+                            +────────────────────────+ |
|      │                                                                    │       |
|      ▼                                                                    ▼       |
| +─────────────────────────+                            +────────────────────────+ |
| | serialToTcpWorker       |                            | tcpToSerialWorker      | |
| | (Serial -> TCP Thread)  |                            | (TCP -> Serial Thread) | |
| +─────────────────────────+                            +────────────────────────+ |
|      │                                                                    │       |
|      ▼                                                                    ▼       |
| +─────────────────────────+                            +────────────────────────+ |
| | m_serialToTcpRing       |                            | m_tcpToSerialRing      | |
| | (LockFreeRingBuffer)    |                            | (LockFreeRingBuffer)   | |
| +─────────────────────────+                            +────────────────────────+ |
+-----------------------------------------------------------------------------------+
```

### Mermaid Diagram

```mermaid
graph TD
    subgraph "DuplexBridge Internal Component Suite"
        Bridge["DuplexBridge Engine<br/>(Orchestrator)"]
        
        subgraph "Worker Threads"
            S2T_Worker["serialToTcpWorker()<br/>(Serial Receive Thread)"]
            T2S_Worker["tcpToSerialWorker()<br/>(TCP Receive Thread)"]
        end

        subgraph "Lock-Free SPSC Ring Buffers"
            S2T_Ring["m_serialToTcpRing<br/>(Capacity = 4096 Frames)"]
            T2S_Ring["m_tcpToSerialRing<br/>(Capacity = 4096 Frames)"]
        end

        subgraph "Hardware & Network Drivers"
            Serial["SerialPort<br/>(Native Hardware Driver)"]
            TCP["TcpClient<br/>(Native Socket Driver)"]
        end

        Stats["BridgeStats & Atomic Counters<br/>(Packets, Bytes, Dropped)"]
    end

    Bridge --> S2T_Worker
    Bridge --> T2S_Worker
    S2T_Worker --> Serial
    S2T_Worker --> S2T_Ring
    S2T_Worker --> TCP
    T2S_Worker --> TCP
    T2S_Worker --> T2S_Ring
    T2S_Worker --> Serial
    Bridge --> Stats
```

---

## 4. Code & Data Model Diagram (Level 4)

The Code diagram details data structures, configuration options, and class relationships.

### Data Structures (`GatewayFrame.h` & `DuplexBridge.h`)

```cpp
struct GatewayFrame {
    std::size_t size {0};
    uint8_t data[1024] {0}; // Fixed 1KB payload chunk
};

struct BridgeStats {
    uint64_t serialToTcpBytes {0};
    uint64_t serialToTcpPackets {0};
    uint64_t tcpToSerialBytes {0};
    uint64_t tcpToSerialPackets {0};
    uint64_t droppedSerialToTcp {0};
    uint64_t droppedTcpToSerial {0};
};
```

### Class Inheritance & Data Flow

```mermaid
classDiagram
    class DuplexBridge {
        -SerialConfig m_serialConfig
        -TcpConfig m_tcpConfig
        -SerialPort m_serialPort
        -TcpClient m_tcpClient
        -unique_ptr~LockFreeRingBuffer~ m_serialToTcpRing
        -unique_ptr~LockFreeRingBuffer~ m_tcpToSerialRing
        -atomic~bool~ m_running
        -thread m_serialToTcpThread
        -thread m_tcpToSerialThread
        +start() bool
        +stop() void
        +runSimulation(messageCount) BridgeStats
        +stats() BridgeStats
        -serialToTcpWorker() void
        -tcpToSerialWorker() void
    }

    class GatewayFrame {
        +size_t size
        +uint8_t data[1024]
    }

    class BridgeStats {
        +uint64_t serialToTcpBytes
        +uint64_t serialToTcpPackets
        +uint64_t tcpToSerialBytes
        +uint64_t tcpToSerialPackets
        +uint64_t droppedSerialToTcp
        +uint64_t droppedTcpToSerial
    }

    DuplexBridge *-- GatewayFrame : buffers
    DuplexBridge --> BridgeStats : produces
```

---

## 5. File References 🔗

- Main Executable CLI Entry: [`protocol_gateway/main.cpp`](../protocol_gateway/main.cpp)
- Bridge Header: [`protocol_gateway/DuplexBridge.h`](../protocol_gateway/DuplexBridge.h)
- Bridge Implementation: [`protocol_gateway/DuplexBridge.cpp`](../protocol_gateway/DuplexBridge.cpp)
- Payload Structure: [`protocol_gateway/GatewayFrame.h`](../protocol_gateway/GatewayFrame.h)
- Serial Driver: [`lib/SerialPort.h`](../lib/SerialPort.h)
- TCP Driver: [`lib/TcpClient.h`](../lib/TcpClient.h)
- Lock-Free Ring Buffer: [`lib/LockFreeRingBuffer.h`](../lib/LockFreeRingBuffer.h)
