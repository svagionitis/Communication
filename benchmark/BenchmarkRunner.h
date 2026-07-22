/**
 * @file BenchmarkRunner.h
 * @brief Benchmark harness for comparing SPSC Lock-Free Ring Buffer against Mutex queues.
 */

#pragma once

#include "BenchmarkMetrics.h"
#include "CircularByteRing.h"
#include "LockFreeRingBuffer.h"
#include "ThreadSafeQueue.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Communication {

/**
 * @brief Fixed-capacity thread-safe queue backed by std::mutex for apples-to-apples comparison.
 */
template <typename T, std::size_t Capacity = 4096>
class MutexRingBuffer {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

public:
    bool push(T item)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_size >= Capacity) {
            return false;
        }
        m_buffer[m_tail] = std::move(item);
        m_tail = (m_tail + 1) & Mask;
        ++m_size;
        return true;
    }

    bool pop(T& item)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_size == 0) {
            return false;
        }
        item = std::move(m_buffer[m_head]);
        m_head = (m_head + 1) & Mask;
        --m_size;
        return true;
    }

    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_size;
    }

private:
    static constexpr std::size_t Mask = Capacity - 1;
    mutable std::mutex m_mutex;
    std::array<T, Capacity> m_buffer;
    std::size_t m_head {0};
    std::size_t m_tail {0};
    std::size_t m_size {0};
};

/**
 * @brief Multi-threaded benchmark executor.
 */
class BenchmarkRunner {
public:
    explicit BenchmarkRunner(std::size_t messageCount = 1000000);

    BenchmarkResult runLockFreeSPSC();
    BenchmarkResult runThreadSafeQueue();
    BenchmarkResult runMutexRingBuffer();
    BenchmarkResult runCircularByteRing();

    void printResultsTable(const std::vector<BenchmarkResult>& results) const;

private:
    std::size_t m_messageCount;
};

} // namespace Communication
