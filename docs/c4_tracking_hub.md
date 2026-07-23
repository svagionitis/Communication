# C4 Architecture Model — AIS & ADS-B Telemetry Tracking Hub

This document details the software architecture for the high-frequency **AIS (Maritime) & ADS-B (Aviation) Telemetry Tracking Hub** (`tracking_hub`) using the **C4 Model** (Context, Containers, Components, and Code). It provides both **ASCII** and **Mermaid** diagrams for each architectural level.

---

## 1. System Context Diagram (Level 1)

The System Context diagram shows how external telemetry providers, human operators, and storage sinks interact with the Tracking Hub system.

### ASCII Diagram

```
+---------------------+        +--------------------+        +-----------------------+
|  Maritime Operator  |        | Aviation Observer  |        | System Administrator  |
+---------------------+        +--------------------+        +-----------------------+
           │                             │                              │
           └─────────────────────────────┼──────────────────────────────┘
                                         │
                                         ▼
                 +-----------------------------------------------+
                 | AIS & ADS-B Telemetry Tracking Hub            |
                 | (tracking_hub Executable)                     |
                 +-----------------------------------------------+
                                         │
        ┌────────────────────────────────┼────────────────────────────────┐
        │                                │                                │
        ▼                                ▼                                ▼
+──────────────────+           +──────────────────+           +──────────────────+
| OpenSky Network  |           | AISStream Feed   |           | Persistent DB    |
| (ADS-B REST API) |           | (Maritime NMEA)  |           | (SQLite Storage) |
+──────────────────+           +──────────────────+           +──────────────────+
```

### Mermaid Diagram

```mermaid
graph TD
    User["Maritime & Aviation Operators"]
    TrackingHub["AIS & ADS-B Tracking Hub<br/>(tracking_hub)"]
    OpenSky["OpenSky Network REST API<br/>(ADS-B Flight Telemetry)"]
    AisFeed["AISStream / Serial Feed<br/>(Maritime AIS NMEA Streams)"]
    SQLite["SQLite Database<br/>(tracking.db Persistent Storage)"]

    User -->|"Monitors target locations & history"| TrackingHub
    TrackingHub -->|"Fetches aircraft vectors (HTTP GET)"| OpenSky
    TrackingHub -->|"Ingests vessel coordinates"| AisFeed
    TrackingHub -->|"Persists target track records"| SQLite
```

---

## 2. Container Diagram (Level 2)

The Container diagram illustrates the runtime executables, libraries, data stores, and external protocol interfaces comprising the Tracking Hub subsystem.

### ASCII Diagram

```
+-----------------------------------------------------------------------------------------+
| Tracking Hub Subsystem Boundary                                                         |
|                                                                                         |
|  +-----------------------------------------------------------------------------------+  |
|  | Tracking Hub CLI Application (tracking_hub)                                       |  |
|  |                                                                                   |  |
|  |  +-----------------------+  +------------------------+  +----------------------+  |  |
|  |  | OpenSkyProvider       |  | AisStreamProvider      |  | SimulatorProvider    |  |  |
|  |  +-----------------------+  +------------------------+  +----------------------+  |  |
|  |              │                          │                          │              |  |
|  |              └──────────────────────────┼──────────────────────────┘              |  |
|  |                                         ▼                                         |  |
|  |                             +-----------------------+                             |  |
|  |                             | TrackingEngine        |                             |  |
|  |                             +-----------------------+                             |  |
|  |                                         │                                         |  |
|  +─────────────────────────────────────────┼─────────────────────────────────────────+  |
|                                            │                                            |
|                                            ▼                                            |
|                                +───────────────────────+                                |
|                                | SQLite Database       |                                |
|                                | (tracking.db)         |                                |
|                                +───────────────────────+                                |
+-----------------------------------------------------------------------------------------+
```

### Mermaid Diagram

```mermaid
graph TD
    subgraph "Tracking Hub System Boundary"
        App["Tracking Hub Main CLI<br/>(tracking_hub)"]
        Engine["TrackingEngine Core<br/>(Aggregation & Dispatch)"]
        OpenSky["OpenSkyProvider<br/>(REST HTTP Poller)"]
        AisStream["AisStreamProvider<br/>(AIS Message Ingestor)"]
        Sim["SimulatorProvider<br/>(Synthetic Target Gen)"]
        Registry["ProviderRegistry<br/>(Provider Lifecycle Manager)"]
        DB["TrackingDatabase<br/>(SQLite Data Access Layer)"]
    end

    ExternalAPI["OpenSky REST Service<br/>(opensky-network.org)"]
    Storage["SQLite File<br/>(tracking.db)"]

    App --> Registry
    Registry --> OpenSky
    Registry --> AisStream
    Registry --> Sim
    OpenSky -->|"HTTP REST Requests"| ExternalAPI
    OpenSky --> Engine
    AisStream --> Engine
    Sim --> Engine
    Engine --> DB
    DB --> Storage
```

---

## 3. Component Diagram (Level 3)

The Component diagram reveals the internal C++ class components within the `tracking_hub` executable.

### ASCII Diagram

```
+-----------------------------------------------------------------------------------+
| tracking_hub Executable Internal Architecture                                     |
|                                                                                   |
|  +----------------------+      +----------------------+     +------------------+  |
|  | HttpClient           | ───> | OpenSkyProvider      | ──┐ | SimulatorProvider|  |
|  +----------------------+      +----------------------+   │ +------------------+  |
|                                                           │          │            |
|  +----------------------+      +----------------------+   │          │            |
|  | Parsers              | ───> | AisStreamProvider    | ──┼──────────┘            |
|  +----------------------+      +----------------------+   │                       |
|                                                           ▼                       |
|                                                +──────────────────────+           |
|                                                | ITrackingProvider    |           |
|                                                +──────────────────────+           |
|                                                           │                       |
|                                                           ▼                       |
|                                                +──────────────────────+           |
|                                                | ProviderRegistry     |           |
|                                                +──────────────────────+           |
|                                                           │                       |
|                                                           ▼                       |
|                                                +──────────────────────+           |
|                                                | TrackingEngine       |           |
|                                                +──────────────────────+           |
|                                                           │                       |
|                                                           ▼                       |
|                                                +──────────────────────+           |
|                                                | TrackingDatabase     |           |
|                                                +──────────────────────+           |
+-----------------------------------------------------------------------------------+
```

### Mermaid Diagram

```mermaid
graph TD
    subgraph "tracking_hub Component Architecture"
        Interface["ITrackingProvider Interface<br/>(Abstract Base Class)"]
        
        subgraph "Telemetry Providers"
            OpenSky["OpenSkyProvider<br/>(REST Aircraft Poller)"]
            AIS["AisStreamProvider<br/>(Maritime AIS Ingestor)"]
            Sim["SimulatorProvider<br/>(Synthetic Target Generator)"]
        end
        
        subgraph "Support Utilities"
            Http["HttpClient<br/>(POSIX/Winsock REST Client)"]
            Parser["Parsers<br/>(JSON & NMEA Decoders)"]
        end
        
        subgraph "Core Aggregation & Storage"
            Reg["ProviderRegistry<br/>(Lifecycle Manager)"]
            Engine["TrackingEngine<br/>(Target Deduplication & Callbacks)"]
            DB["TrackingDatabase<br/>(SQLite persistence engine)"]
        end
    end

    OpenSky -.->|"Implements"| Interface
    AIS -.->|"Implements"| Interface
    Sim -.->|"Implements"| Interface

    Http --> OpenSky
    Parser --> OpenSky
    Parser --> AIS

    Reg --> Interface
    OpenSky -->|"Publishes TargetUpdate"| Engine
    AIS -->|"Publishes TargetUpdate"| Engine
    Sim -->|"Publishes TargetUpdate"| Engine
    Engine -->|"Persists Targets"| DB
```

---

## 4. Code & Data Model Diagram (Level 4)

The Code diagram details the core data structures and C++ class contracts driving target processing.

### Data Model & Structs (`TargetTypes.h`)

```cpp
enum class TargetType {
    AisTarget,   // Maritime vessel (MMSI)
    AdsbTarget   // Aircraft (ICAO 24-bit transponder)
};

struct TrackingTarget {
    std::string mmsiOrIcao;  // Unique identifier
    TargetType type;         // Maritime or Aviation
    double latitude;         // WGS84 Latitude
    double longitude;        // WGS84 Longitude
    double speed;            // Knots or Airspeed
    double heading;          // True Heading (Degrees)
    double altitude;         // Feet (ADS-B) or 0 (AIS)
    uint64_t timestamp;      // Epoch Milliseconds
    std::string callsign;    // Vessel Name or Flight Callsign
};
```

### Class Inheritance & Relationships

```mermaid
classDiagram
    class ITrackingProvider {
        <<interface>>
        +start() bool*
        +stop() void*
        +registerCallback(callback) void*
        +name() string*
    }

    class OpenSkyProvider {
        -HttpClient m_httpClient
        -uint32_t m_pollIntervalSec
        +start() bool
        +stop() void
        +pollNow() void
    }

    class AisStreamProvider {
        +start() bool
        +stop() void
        +parseNmea(string) void
    }

    class SimulatorProvider {
        -double m_simSpeed
        +start() bool
        +stop() void
        +generateTargets() void
    }

    class TrackingEngine {
        -TrackingDatabase& m_db
        -std::vector<TargetCallback> m_callbacks
        +onTargetUpdate(TrackingTarget) void
        +registerCallback(callback) void
    }

    class TrackingDatabase {
        -sqlite3* m_dbHandle
        +saveTarget(TrackingTarget) bool
        +queryHistory(mmsiOrIcao) vector~TrackingTarget~
    }

    ITrackingProvider <|-- OpenSkyProvider
    ITrackingProvider <|-- AisStreamProvider
    ITrackingProvider <|-- SimulatorProvider
    TrackingEngine --> TrackingDatabase
```

---

## 5. File References 🔗

- Header Interface: [`tracking_hub/ITrackingProvider.h`](../tracking_hub/ITrackingProvider.h)
- Data Structures: [`tracking_hub/TargetTypes.h`](../tracking_hub/TargetTypes.h)
- OpenSky Provider: [`tracking_hub/OpenSkyProvider.h`](../tracking_hub/OpenSkyProvider.h)
- AIS Provider: [`tracking_hub/AisStreamProvider.h`](../tracking_hub/AisStreamProvider.h)
- Simulator Provider: [`tracking_hub/SimulatorProvider.h`](../tracking_hub/SimulatorProvider.h)
- Registry: [`tracking_hub/ProviderRegistry.h`](../tracking_hub/ProviderRegistry.h)
- Tracking Engine: [`tracking_hub/TrackingEngine.h`](../tracking_hub/TrackingEngine.h)
- Database Persistence: [`tracking_hub/TrackingDatabase.h`](../tracking_hub/TrackingDatabase.h)
- Executable Main Entry: [`tracking_hub/main.cpp`](../tracking_hub/main.cpp)
