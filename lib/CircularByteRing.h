/**
 * @file CircularByteRing.h
 * @brief High-performance Single-Producer Single-Consumer (SPSC) Zero-Copy Circular Byte Stream Ring.
 */

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Communication {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier (intentional alignas(64))
#endif

/**
 * @brief Represents a contiguous memory byte region.
 */
struct ByteRegion {
    uint8_t* data {nullptr};
    std::size_t size {0};
};

/**
 * @brief Represents a contiguous const memory byte region.
 */
struct ConstByteRegion {
    const uint8_t* data {nullptr};
    std::size_t size {0};
};

/**
 * @brief Zero-copy write view containing up to 2 contiguous memory blocks.
 */
struct ZeroCopyWriteView {
    ByteRegion first;  // Primary contiguous write block
    ByteRegion second; // Wrap-around write block (if any)

    std::size_t totalSize() const { return first.size + second.size; }
};

/**
 * @brief Zero-copy read view containing up to 2 contiguous const memory blocks.
 */
struct ZeroCopyReadView {
    ConstByteRegion first;  // Primary contiguous read block
    ConstByteRegion second; // Wrap-around read block (if any)

    std::size_t totalSize() const { return first.size + second.size; }
};

/**
 * @brief Single-Producer Single-Consumer (SPSC) Lock-Free Circular Byte Stream Ring.
 * @tparam Capacity Maximum byte capacity (must be a power of 2).
 */
template <std::size_t Capacity = 65536>
class CircularByteRing {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

public:
    static constexpr std::size_t Mask = Capacity - 1;
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    CircularByteRing()
        : m_head(0)
        , m_tail(0)
    {
    }

    ~CircularByteRing() = default;

    CircularByteRing(const CircularByteRing&) = delete;
    CircularByteRing& operator=(const CircularByteRing&) = delete;

    CircularByteRing(CircularByteRing&&) = delete;
    CircularByteRing& operator=(CircularByteRing&&) = delete;

    /**
     * @brief Gets zero-copy write view regions for direct OS socket receive (Producer Thread Only).
     * @return ZeroCopyWriteView with up to 2 contiguous write regions.
     */
    ZeroCopyWriteView getWriteView()
    {
        const auto currentTail = m_tail.load(std::memory_order_relaxed);
        const auto currentHead = m_head.load(std::memory_order_acquire);
        const std::size_t used = currentTail - currentHead;
        const std::size_t freeBytes = Capacity - used;

        ZeroCopyWriteView view {};
        if (freeBytes == 0) {
            return view;
        }

        const std::size_t tailIdx = currentTail & Mask;
        const std::size_t spaceToEnd = Capacity - tailIdx;
        const std::size_t firstSize = (freeBytes < spaceToEnd) ? freeBytes : spaceToEnd;
        const std::size_t secondSize = freeBytes - firstSize;

        view.first = ByteRegion {&m_buffer[tailIdx], firstSize};
        if (secondSize > 0) {
            view.second = ByteRegion {&m_buffer[0], secondSize};
        }

        return view;
    }

    /**
     * @brief Advances write cursor by specified bytes (Producer Thread Only).
     * @param bytes Number of written bytes to commit into buffer.
     */
    void advanceWrite(std::size_t bytes)
    {
        const auto currentTail = m_tail.load(std::memory_order_relaxed);
        m_tail.store(currentTail + bytes, std::memory_order_release);
    }

    /**
     * @brief Gets zero-copy read view regions for direct in-place parsing (Consumer Thread Only).
     * @return ZeroCopyReadView with up to 2 contiguous const read regions.
     */
    ZeroCopyReadView getReadView() const
    {
        const auto currentHead = m_head.load(std::memory_order_relaxed);
        const auto currentTail = m_tail.load(std::memory_order_acquire);
        const std::size_t available = currentTail - currentHead;

        ZeroCopyReadView view {};
        if (available == 0) {
            return view;
        }

        const std::size_t headIdx = currentHead & Mask;
        const std::size_t spaceToEnd = Capacity - headIdx;
        const std::size_t firstSize = (available < spaceToEnd) ? available : spaceToEnd;
        const std::size_t secondSize = available - firstSize;

        view.first = ConstByteRegion {&m_buffer[headIdx], firstSize};
        if (secondSize > 0) {
            view.second = ConstByteRegion {&m_buffer[0], secondSize};
        }

        return view;
    }

    /**
     * @brief Advances read cursor by specified bytes (Consumer Thread Only).
     * @param bytes Number of read bytes to consume from buffer.
     */
    void advanceRead(std::size_t bytes)
    {
        const auto currentHead = m_head.load(std::memory_order_relaxed);
        m_head.store(currentHead + bytes, std::memory_order_release);
    }

    /**
     * @brief Searches for target byte delimiter in readable data (Consumer Thread Only).
     * @param delimiter Byte value to search for (e.g. '\n').
     * @return Offset from head to delimiter, or CircularByteRing::npos if not found.
     */
    std::size_t findByte(uint8_t delimiter) const
    {
        const auto view = getReadView();
        if (view.totalSize() == 0) {
            return npos;
        }

        if (view.first.size > 0) {
            const uint8_t* p = static_cast<const uint8_t*>(std::memchr(view.first.data, delimiter, view.first.size));
            if (p) {
                return static_cast<std::size_t>(p - view.first.data);
            }
        }

        if (view.second.size > 0) {
            const uint8_t* p = static_cast<const uint8_t*>(std::memchr(view.second.data, delimiter, view.second.size));
            if (p) {
                return view.first.size + static_cast<std::size_t>(p - view.second.data);
            }
        }

        return npos;
    }

    /**
     * @brief Copies up to count bytes from readable data without advancing read cursor (Consumer Thread Only).
     * @param dest Destination memory buffer.
     * @param count Number of bytes to peek.
     * @return True if count bytes were peeked successfully.
     */
    bool peekBytes(uint8_t* dest, std::size_t count) const
    {
        if (!dest) {
            return false;
        }

        const auto view = getReadView();
        if (view.totalSize() < count) {
            return false;
        }

        std::size_t copyFirst = (count < view.first.size) ? count : view.first.size;
        std::memcpy(dest, view.first.data, copyFirst);

        if (count > copyFirst) {
            std::size_t copySecond = count - copyFirst;
            std::memcpy(dest + copyFirst, view.second.data, copySecond);
        }

        return true;
    }

    /**
     * @brief Copies exact bytes into buffer (Producer Thread Only).
     * @param src Source memory buffer.
     * @param count Number of bytes to copy.
     * @return True if written successfully, false if space is insufficient.
     */
    bool writeExact(const uint8_t* src, std::size_t count)
    {
        if (!src || count == 0) {
            return false;
        }

        auto view = getWriteView();
        if (view.totalSize() < count) {
            return false;
        }

        std::size_t copyFirst = (count < view.first.size) ? count : view.first.size;
        std::memcpy(view.first.data, src, copyFirst);

        if (count > copyFirst) {
            std::size_t copySecond = count - copyFirst;
            std::memcpy(view.second.data, src + copyFirst, copySecond);
        }

        advanceWrite(count);
        return true;
    }

    /**
     * @brief Copies exact bytes out of buffer and advances read cursor (Consumer Thread Only).
     * @param dest Destination memory buffer.
     * @param count Number of bytes to copy.
     * @return True if read successfully, false if available bytes is insufficient.
     */
    bool readExact(uint8_t* dest, std::size_t count)
    {
        if (!peekBytes(dest, count)) {
            return false;
        }
        advanceRead(count);
        return true;
    }

    /**
     * @brief Returns total readable bytes available in buffer.
     */
    std::size_t availableRead() const
    {
        const auto currentHead = m_head.load(std::memory_order_relaxed);
        const auto currentTail = m_tail.load(std::memory_order_acquire);
        return currentTail - currentHead;
    }

    /**
     * @brief Returns total writable byte capacity remaining in buffer.
     */
    std::size_t availableWrite() const { return Capacity - availableRead(); }

    /**
     * @brief Returns total fixed capacity of the ring buffer.
     */
    constexpr std::size_t capacity() const { return Capacity; }

    /**
     * @brief Clears the ring buffer by resetting head and tail indices.
     */
    void clear()
    {
        m_head.store(0, std::memory_order_relaxed);
        m_tail.store(0, std::memory_order_relaxed);
    }

private:
    alignas(64) std::atomic<std::size_t> m_head;
    alignas(64) std::atomic<std::size_t> m_tail;
    alignas(64) std::array<uint8_t, Capacity> m_buffer;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace Communication
