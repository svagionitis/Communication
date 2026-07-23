/**
 * @file LockFreeRingBuffer.h
 * @brief High-performance Single-Producer Single-Consumer (SPSC) Lock-Free Ring Buffer.
 */

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace Communication {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324) // structure was padded due to alignment specifier (intentional alignas(64))
#endif

/**
 * @brief Single-Producer Single-Consumer (SPSC) Lock-Free Ring Buffer.
 * @tparam T Type of elements stored in the ring buffer.
 * @tparam Capacity Maximum capacity of the buffer (must be a power of 2).
 */
template <typename T, std::size_t Capacity = 1024>
class LockFreeRingBuffer {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

public:
    LockFreeRingBuffer()
        : m_head(0)
        , m_tail(0)
    {
    }

    ~LockFreeRingBuffer() = default;

    LockFreeRingBuffer(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer& operator=(const LockFreeRingBuffer&) = delete;

    LockFreeRingBuffer(LockFreeRingBuffer&&) = delete;
    LockFreeRingBuffer& operator=(LockFreeRingBuffer&&) = delete;

    /**
     * @brief Pushes an item into the ring buffer (Producer Thread Only).
     * @param item Item to copy into the buffer.
     * @return True if pushed successfully, false if buffer is full.
     */
    bool push(const T& item)
    {
        const auto currentTail = m_tail.load(std::memory_order_relaxed);
        const auto currentHead = m_head.load(std::memory_order_acquire);

        if (currentTail - currentHead >= Capacity) {
            return false; // Buffer full
        }

        m_buffer[currentTail & Mask] = item;
        m_tail.store(currentTail + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pushes an item into the ring buffer via move semantics (Producer Thread Only).
     * @param item Item to move into the buffer.
     * @return True if pushed successfully, false if buffer is full.
     */
    bool push(T&& item)
    {
        const auto currentTail = m_tail.load(std::memory_order_relaxed);
        const auto currentHead = m_head.load(std::memory_order_acquire);

        if (currentTail - currentHead >= Capacity) {
            return false; // Buffer full
        }

        m_buffer[currentTail & Mask] = std::move(item);
        m_tail.store(currentTail + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pops an item from the ring buffer (Consumer Thread Only).
     * @param item Reference to store the popped item.
     * @return True if popped successfully, false if buffer is empty.
     */
    bool pop(T& item)
    {
        const auto currentHead = m_head.load(std::memory_order_relaxed);
        const auto currentTail = m_tail.load(std::memory_order_acquire);

        if (currentHead == currentTail) {
            return false; // Buffer empty
        }

        item = std::move(m_buffer[currentHead & Mask]);
        m_head.store(currentHead + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Returns approximate current element count.
     * @return Number of elements currently in the buffer.
     */
    std::size_t size() const
    {
        const auto head = m_head.load(std::memory_order_relaxed);
        const auto tail = m_tail.load(std::memory_order_relaxed);
        return (tail >= head) ? (tail - head) : 0;
    }

    /**
     * @brief Checks if buffer is empty.
     * @return True if empty.
     */
    bool empty() const { return m_head.load(std::memory_order_relaxed) == m_tail.load(std::memory_order_relaxed); }

    /**
     * @brief Checks if buffer is full.
     * @return True if full.
     */
    bool full() const { return size() >= Capacity; }

    /**
     * @brief Returns maximum capacity of the ring buffer.
     * @return Configured capacity.
     */
    constexpr std::size_t capacity() const { return Capacity; }

private:
    static constexpr std::size_t Mask = Capacity - 1;

#ifdef __cpp_lib_hardware_interference_size
    static constexpr std::size_t CacheLineSize = std::hardware_destructive_interference_size;
#else
    static constexpr std::size_t CacheLineSize = 64;
#endif

    alignas(CacheLineSize) std::atomic<std::size_t> m_head;
    alignas(CacheLineSize) std::atomic<std::size_t> m_tail;
    alignas(CacheLineSize) std::array<T, Capacity> m_buffer;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace Communication
