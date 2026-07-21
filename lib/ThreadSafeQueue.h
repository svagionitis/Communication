/**
 * @file ThreadSafeQueue.h
 * @brief Lightweight, thread-safe queue implementation for async data buffers.
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

namespace Communication {

/**
 * @brief Thread-safe template queue backing incoming and outgoing data buffers.
 */
template <typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;
    ~ThreadSafeQueue() = default;

    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    /**
     * @brief Pushes an item onto the queue and notifies one waiting thread.
     * @param item The value to push.
     */
    void push(T item)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push_back(std::move(item));
        }
        m_condVar.notify_one();
    }

    /**
     * @brief Blocks until an item is available and pops it.
     * @return The popped item.
     */
    T pop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condVar.wait(lock, [this] { return !m_queue.empty(); });
        T value = std::move(m_queue.front());
        m_queue.pop_front();
        return value;
    }

    /**
     * @brief Attempts to pop an item without blocking.
     * @return std::optional containing item if available, std::nullopt otherwise.
     */
    std::optional<T> tryPop()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return std::nullopt;
        }
        T value = std::move(m_queue.front());
        m_queue.pop_front();
        return value;
    }

    /**
     * @brief Blocks with timeout until an item is available and pops it.
     * @param timeoutDuration Maximum time duration to wait.
     * @return std::optional containing item if popped, std::nullopt on timeout.
     */
    template <typename Rep, typename Period>
    std::optional<T> popFor(const std::chrono::duration<Rep, Period>& timeoutDuration)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_condVar.wait_for(lock, timeoutDuration, [this] { return !m_queue.empty(); })) {
            return std::nullopt;
        }
        T value = std::move(m_queue.front());
        m_queue.pop_front();
        return value;
    }

    /**
     * @brief Clears all elements from the queue.
     */
    void clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.clear();
    }

    /**
     * @brief Returns current count of queued elements.
     * @return Queue size.
     */
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    /**
     * @brief Checks if queue is empty.
     * @return True if empty, false otherwise.
     */
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_condVar;
    std::deque<T> m_queue;
};

} // namespace Communication
