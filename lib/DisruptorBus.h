/**
 * @file DisruptorBus.h
 * @brief High-performance Single-Producer Multi-Consumer (SPMC) Lock-Free Event Disruptor / Broadcast Bus.
 */

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace Communication {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier (intentional alignas(64))
#endif

/**
 * @brief Single-Producer Multi-Consumer (SPMC) Lock-Free Event Disruptor / Broadcast Bus.
 * Based on the LMAX Disruptor pattern.
 * @tparam T Type of event stored in the bus.
 * @tparam Capacity Maximum capacity of the ring buffer (must be a power of 2).
 * @tparam ConsumerCount Maximum number of concurrent subscriber consumers.
 */
template <typename T, std::size_t Capacity = 1024, std::size_t ConsumerCount = 4>
class DisruptorBus {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
    static_assert(ConsumerCount >= 1, "ConsumerCount must be at least 1");

public:
    static constexpr std::size_t Mask = Capacity - 1;

    DisruptorBus()
        : m_producerTail(0)
    {
        for (std::size_t i = 0; i < ConsumerCount; ++i) {
            m_consumerHeads[i].head.store(0, std::memory_order_relaxed);
        }
    }

    ~DisruptorBus() = default;

    DisruptorBus(const DisruptorBus&) = delete;
    DisruptorBus& operator=(const DisruptorBus&) = delete;

    DisruptorBus(DisruptorBus&&) = delete;
    DisruptorBus& operator=(DisruptorBus&&) = delete;

    /**
     * @brief Calculates minimum read sequence head among all active consumers.
     */
    std::size_t minConsumerHead() const
    {
        std::size_t minHead = m_consumerHeads[0].head.load(std::memory_order_acquire);
        for (std::size_t i = 1; i < ConsumerCount; ++i) {
            std::size_t h = m_consumerHeads[i].head.load(std::memory_order_acquire);
            if (h < minHead) {
                minHead = h;
            }
        }
        return minHead;
    }

    /**
     * @brief Publishes an event to the broadcast bus (Producer Thread Only).
     * @param event Event to copy into the buffer.
     * @return True if published successfully, false if gated by slowest consumer.
     */
    bool publish(const T& event)
    {
        const auto currentTail = m_producerTail.load(std::memory_order_relaxed);
        const auto slowestHead = minConsumerHead();

        if (currentTail - slowestHead >= Capacity) {
            return false; // Gated by slowest consumer
        }

        m_buffer[currentTail & Mask] = event;
        m_producerTail.store(currentTail + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Publishes an event via move semantics (Producer Thread Only).
     * @param event Event to move into the buffer.
     * @return True if published successfully, false if gated by slowest consumer.
     */
    bool publish(T&& event)
    {
        const auto currentTail = m_producerTail.load(std::memory_order_relaxed);
        const auto slowestHead = minConsumerHead();

        if (currentTail - slowestHead >= Capacity) {
            return false; // Gated by slowest consumer
        }

        m_buffer[currentTail & Mask] = std::move(event);
        m_producerTail.store(currentTail + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Consumes an event for a specific consumer ID (Consumer Thread Only).
     * @param consumerId Consumer index (0 <= consumerId < ConsumerCount).
     * @param outEvent Reference to receive consumed event.
     * @return True if an event was available and consumed.
     */
    bool consume(std::size_t consumerId, T& outEvent)
    {
        if (consumerId >= ConsumerCount) {
            return false;
        }

        const auto currentHead = m_consumerHeads[consumerId].head.load(std::memory_order_relaxed);
        const auto currentTail = m_producerTail.load(std::memory_order_acquire);

        if (currentHead >= currentTail) {
            return false; // No events available
        }

        outEvent = m_buffer[currentHead & Mask];
        m_consumerHeads[consumerId].head.store(currentHead + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Peeks at the next available event without advancing consumer head.
     * @param consumerId Consumer index.
     * @return Pointer to available event or nullptr if no event available.
     */
    const T* peek(std::size_t consumerId) const
    {
        if (consumerId >= ConsumerCount) {
            return nullptr;
        }

        const auto currentHead = m_consumerHeads[consumerId].head.load(std::memory_order_relaxed);
        const auto currentTail = m_producerTail.load(std::memory_order_acquire);

        if (currentHead >= currentTail) {
            return nullptr;
        }

        return &m_buffer[currentHead & Mask];
    }

    /**
     * @brief Advances consumer sequence head by specified count.
     * @param consumerId Consumer index.
     * @param count Number of events to advance.
     */
    void advance(std::size_t consumerId, std::size_t count = 1)
    {
        if (consumerId >= ConsumerCount) {
            return;
        }

        const auto currentHead = m_consumerHeads[consumerId].head.load(std::memory_order_relaxed);
        m_consumerHeads[consumerId].head.store(currentHead + count, std::memory_order_release);
    }

    /**
     * @brief Returns available unread event count for specified consumer.
     */
    std::size_t available(std::size_t consumerId) const
    {
        if (consumerId >= ConsumerCount) {
            return 0;
        }

        const auto currentHead = m_consumerHeads[consumerId].head.load(std::memory_order_relaxed);
        const auto currentTail = m_producerTail.load(std::memory_order_acquire);

        return (currentTail > currentHead) ? (currentTail - currentHead) : 0;
    }

    /**
     * @brief Resets sequence head for a consumer (e.g. catch up to current producer tail).
     */
    void resetConsumer(std::size_t consumerId)
    {
        if (consumerId < ConsumerCount) {
            const auto currentTail = m_producerTail.load(std::memory_order_acquire);
            m_consumerHeads[consumerId].head.store(currentTail, std::memory_order_release);
        }
    }

    constexpr std::size_t capacity() const { return Capacity; }
    constexpr std::size_t consumerCount() const { return ConsumerCount; }

private:
    struct alignas(64) ConsumerSequence {
        std::atomic<std::size_t> head {0};
    };

    alignas(64) std::atomic<std::size_t> m_producerTail;
    alignas(64) std::array<ConsumerSequence, ConsumerCount> m_consumerHeads;
    alignas(64) std::array<T, Capacity> m_buffer;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace Communication
