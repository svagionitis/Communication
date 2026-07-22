/**
 * @file LockFreeMemoryPool.h
 * @brief High-performance Lock-Free Fixed-Block Memory Allocator.
 */

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace Communication {

/**
 * @brief Lock-Free Fixed-Block Memory Pool Allocator.
 * @tparam T Type of object stored in the memory pool.
 * @tparam BlockCount Number of fixed blocks in the pre-allocated pool.
 */
template <typename T, std::size_t BlockCount = 1024>
class LockFreeMemoryPool {
    static_assert(BlockCount >= 1, "BlockCount must be at least 1");

public:
    static constexpr std::size_t invalidIndex = static_cast<std::size_t>(-1);

    LockFreeMemoryPool()
        : m_freeListHead(0)
        , m_allocatedCount(0)
    {
        for (std::size_t i = 0; i < BlockCount - 1; ++i) {
            m_pool[i].nextIndex = i + 1;
        }
        m_pool[BlockCount - 1].nextIndex = invalidIndex;
    }

    ~LockFreeMemoryPool() = default;

    LockFreeMemoryPool(const LockFreeMemoryPool&) = delete;
    LockFreeMemoryPool& operator=(const LockFreeMemoryPool&) = delete;

    LockFreeMemoryPool(LockFreeMemoryPool&&) = delete;
    LockFreeMemoryPool& operator=(LockFreeMemoryPool&&) = delete;

    /**
     * @brief Allocates an uninitialized memory block for T (Thread-Safe & Lock-Free).
     * @return Pointer to allocated block memory for T, or nullptr if pool is exhausted.
     */
    T* allocate()
    {
        std::size_t head = m_freeListHead.load(std::memory_order_acquire);
        while (head != invalidIndex) {
            std::size_t next = m_pool[head].nextIndex;
            if (m_freeListHead.compare_exchange_weak(head, next, std::memory_order_acquire,
                                                     std::memory_order_relaxed)) {
                m_allocatedCount.fetch_add(1, std::memory_order_relaxed);
                return reinterpret_cast<T*>(m_pool[head].storage);
            }
        }
        return nullptr; // Pool exhausted
    }

    /**
     * @brief Deallocates a memory block back to the pool freelist (Thread-Safe & Lock-Free).
     * @param ptr Pointer to memory block to return to pool.
     */
    void deallocate(T* ptr)
    {
        if (!ptr) {
            return;
        }

        const uint8_t* bytePtr = reinterpret_cast<const uint8_t*>(ptr);
        const uint8_t* poolStart = reinterpret_cast<const uint8_t*>(m_pool.data());
        std::size_t index = static_cast<std::size_t>(bytePtr - poolStart) / sizeof(BlockNode);

        if (index >= BlockCount) {
            return;
        }

        std::size_t head = m_freeListHead.load(std::memory_order_relaxed);
        do {
            m_pool[index].nextIndex = head;
        } while (
            !m_freeListHead.compare_exchange_weak(head, index, std::memory_order_release, std::memory_order_relaxed));

        m_allocatedCount.fetch_sub(1, std::memory_order_relaxed);
    }

    /**
     * @brief Allocates block and constructs T in place via placement-new.
     * @tparam Args Argument types for T's constructor.
     * @param args Constructor arguments forwarded to T.
     * @return Pointer to constructed object T, or nullptr if pool is exhausted.
     */
    template <typename... Args>
    T* construct(Args&&... args)
    {
        T* ptr = allocate();
        if (ptr) {
            new (ptr) T(std::forward<Args>(args)...);
        }
        return ptr;
    }

    /**
     * @brief Destroys constructed object T by invoking destructor and deallocating block.
     * @param ptr Pointer to object to destroy and return to pool.
     */
    void destroy(T* ptr)
    {
        if (ptr) {
            ptr->~T();
            deallocate(ptr);
        }
    }

    /**
     * @brief Returns current number of allocated active blocks.
     */
    std::size_t allocatedBlocks() const { return m_allocatedCount.load(std::memory_order_relaxed); }

    /**
     * @brief Returns current number of available free blocks in pool.
     */
    std::size_t availableBlocks() const
    {
        std::size_t allocated = allocatedBlocks();
        return (BlockCount > allocated) ? (BlockCount - allocated) : 0;
    }

    /**
     * @brief Returns total block capacity of pool.
     */
    constexpr std::size_t capacity() const { return BlockCount; }

private:
    struct BlockNode {
        std::size_t nextIndex {invalidIndex};
        alignas(alignof(T)) uint8_t storage[sizeof(T)];
    };

    alignas(64) std::atomic<std::size_t> m_freeListHead;
    alignas(64) std::atomic<std::size_t> m_allocatedCount;
    alignas(64) std::array<BlockNode, BlockCount> m_pool;
};

} // namespace Communication
