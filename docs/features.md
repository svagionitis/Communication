# Communication Library Feature Suite & Concurrency Primitives

Here is a comprehensive breakdown of implemented performance primitives, architectural features, and advanced concurrency primitives that elevate this codebase into a production-grade embedded, networking, and real-time telemetry suite.

---

## 1. Core Concurrency & Memory Primitives

### Implemented Primitives (`lib/`)

- **`LockFreeRingBuffer<T, Capacity>` (SPSC Queue)**:
  - Single-Producer Single-Consumer lock-free ring buffer for structured objects.
  - Features `alignas(64)` cache line separation for atomic `head` and `tail` cursors to eliminate CPU cache false sharing.
- **`CircularByteRing<Capacity>` (Zero-Copy Stream Ring)**:
  - SPSC lock-free circular byte stream ring for direct OS socket I/O (`recv()`, `readv()`) and zero-copy in-place protocol parsing (NMEA 0183, Modbus TCP, HTTP headers).
  - Provides `getWriteView()` (up to 2 contiguous write regions), `getReadView()` (in-place const inspection), and `findByte()` (fast `memchr` delimiter search).
- **`DisruptorBus<T, Capacity, ConsumerCount>` (SPMC Event Broadcast Bus)**:
  - LMAX Disruptor-pattern single-producer multi-consumer lock-free broadcast bus.
  - Broadcasts event frames to up to `ConsumerCount` independent subscriber threads without memory duplication.
  - Includes slowest-consumer gating (`minConsumerHead()`) to guarantee zero event loss and 64-byte `alignas(64)` cache line isolation across subscriber sequence cursors.
- **`LockFreeMemoryPool<T, BlockCount>` (MPMC Fixed-Block Memory Allocator)**:
  - Lock-free fixed-size block memory allocator bypassing kernel `malloc`/`free` locks, heap fragmentation, and thread contention in high-frequency network callbacks.
  - Index-based atomic CAS (`compare_exchange_weak`) freelist stack with $O(1)$ index offset computation, placement-new construction (`construct`), and explicit destructors (`destroy`). Achieves **29 ns** mean allocation latency.
- **`ThreadSafeQueue<T>` (Mutex Queue)**:
  - Thread-safe queue backed by `std::mutex` and `std::condition_variable` with `try_pop` and `pop_for` timeouts.

---

## 2. Advanced Low-Latency Concurrency Primitives Roadmap

The following advanced concurrency primitives are identified for real-time, low-latency, and high-frequency communication architectures:

### A. Multi-Producer Multi-Consumer (MPMC) Bounded Ring Buffer
- **Algorithm**: Vyukov MPMC Bounded Queue (used in `folly::MPMCQueue`, `boost::lockfree::queue`, and Go channels).
- **Mechanism**: Array of fixed slots where each slot contains an atomic sequence turn counter (`std::atomic<std::size_t> sequence`). Producers atomic-increment `tail` via CAS, and consumers atomic-increment `head` via CAS.
- **Use Case**: Multiple producer threads pushing data concurrently into a shared pool of consumer worker threads without mutex contention.

### B. Lock-Free Hazard Pointers & Epoch-Based Reclamation (EBR)
- **Algorithm**: Michael Hazard Pointers / Linux Read-Copy-Update (RCU).
- **Mechanism**: Readers access shared data structures (e.g. routing tables, client maps, configuration state) with **zero atomic write operations** (`0 ns` sync cost). Writers copy the data structure, mutate it, swap the atomic pointer, and defer freeing old memory until all hazard pointers / epoch readers pass.
- **Use Case**: Read-mostly lookup tables, target tracking maps, and runtime configuration switches.

### C. Lock-Free Priority Queue / Concurrent SkipList
- **Algorithm**: Herlihy-Lev-Moir Concurrent SkipList.
- **Mechanism**: Multi-level linked list utilizing atomic CAS pointer swaps for insertion and deletion. Maintains ordered elements in $O(\log N)$ expected time without taking locks.
- **Use Case**: Priority-based packet processing (e.g. urgent safety shutdown frames jumping ahead of routine telemetry updates) and real-time event schedulers.

### D. Lock-Free Hashed Timing Wheel (`TimingWheel`)
- **Algorithm**: Varghese & Lauck Hashed Timing Wheel.
- **Mechanism**: Circular array of buckets indexed by tick modulus ($O(1)$ add, $O(1)$ cancel, $O(1)$ tick expiry).
- **Use Case**: Managing hundreds of thousands of concurrent timers (e.g., protocol keep-alive deadlines, TCP retransmission timeouts, serial response deadlines) without $O(\log N)$ tree overhead.

### E. Lock-Free Work-Stealing Deque
- **Algorithm**: Chase-Lev Work-Stealing Deque.
- **Mechanism**: Double-ended queue where the worker thread pushes/pops from the **bottom** (LIFO for CPU L1/L2 cache locality), while idle "thief" threads steal work items from the **top** (FIFO) using lock-free CAS.
- **Use Case**: Thread pools and multi-threaded task schedulers for automatic CPU core load balancing.

### F. Queue-Based Spinlock (MCS Lock)
- **Algorithm**: Mellor-Crummey & Scott (MCS) Lock.
- **Mechanism**: Builds a temporary linked list of waiting threads where each thread spins **only on its own local cache line**, eliminating CPU cache invalidation storms under high lock contention.
- **Use Case**: Sub-microsecond critical sections where kernel `std::mutex` context switches are prohibitively expensive.

---

## 3. Comparative Benchmark Summary

Measured using the repository benchmark suite (`./build_bench/benchmark/lockfree_benchmark --iterations 2000000`):

| Implementation | Throughput (msgs/sec) | Bandwidth (MB/s) | Mean Latency | P99 Latency | StdDev (Jitter) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`DisruptorBus` (SPMC x3)** | **23.69 Million** | **1,808.0 MB/s** | ~43.7 µs | 465.7 µs | 105.7 µs |
| **`LockFreeMemoryPool` (Alloc)** | **12.85 Million** | **980.9 MB/s** | **29 ns** | **34 ns** | **120 ns** |
| **`CircularByteRing` (Stream)** | **10.74 Million** | **819.7 MB/s** | ~49.9 µs | 110.0 µs | 33.4 µs |
| **`LockFreeRingBuffer` (SPSC)** | **9.92 Million** | **757.2 MB/s** | ~406.7 µs | 505.6 µs | 53.2 µs |
| **`MutexRingBuffer` (Mutex)** | 3.42 Million | 261.2 MB/s | ~1.13 ms | 1.42 ms | 258.1 µs |
| **`ThreadSafeQueue` (Deque)** | 3.18 Million | 243.0 MB/s | ~164.1 ms | 277.4 ms | 72.8 ms |

---

## 4. Embedded & Networking Features

### Robustness & Network Features
- **Auto-Reconnect Manager (`ReconnectPolicy`)**: Configurable exponential backoff (`initialDelayMs`, `maxDelayMs`, `maxRetries`) for `TcpClient` and `SerialPort`.
- **Packet Framer / Protocol Parsing (`IFrameParser`)**: Reusable frame parsers (`DelimiterFrameParser`, `LengthHeaderFrameParser`, `COBSFrameParser`).
- **TCP Keep-Alive Settings**: Native TCP keep-alives (`SO_KEEPALIVE`, `TCP_KEEPIDLE`, `TCP_KEEPINTVL`, `TCP_KEEPCNT`).
- **Serial Flow Control**: Hardware (`RTS/CTS`) and Software (`XON/XOFF`) flow control options.

### Diagnostics & Telemetry
- **Connection Statistics & Metrics (`ConnectionStats`)**: Tracking `bytesSent`, `bytesReceived`, `packetsSent`, `packetsReceived`, `connectTimestamp`, `reconnectCount`, and throughput rate.
- **Packet Hex Dump Logger**: Configurable Hex Dump utility (`dumpHex(data)`) for raw binary protocol frame debugging.