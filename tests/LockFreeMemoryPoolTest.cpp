/**
 * @file LockFreeMemoryPoolTest.cpp
 * @brief Unit tests for LockFreeMemoryPool.
 */

#include "LockFreeMemoryPool.h"

#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace Communication;

struct SamplePacket {
    uint64_t id {0};
    char data[64] {0};

    SamplePacket(uint64_t i = 0)
        : id(i)
    {
    }
};

struct TrackedObject {
    static std::atomic<int> destructorCount;
    int value {0};

    explicit TrackedObject(int v)
        : value(v)
    {
    }
    ~TrackedObject() { destructorCount.fetch_add(1, std::memory_order_relaxed); }
};
std::atomic<int> TrackedObject::destructorCount {0};

TEST(LockFreeMemoryPoolTest, BasicAllocateDeallocate)
{
    LockFreeMemoryPool<SamplePacket, 16> pool;
    EXPECT_EQ(pool.capacity(), 16u);
    EXPECT_EQ(pool.availableBlocks(), 16u);
    EXPECT_EQ(pool.allocatedBlocks(), 0u);

    SamplePacket* p1 = pool.allocate();
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(pool.allocatedBlocks(), 1u);
    EXPECT_EQ(pool.availableBlocks(), 15u);

    p1->id = 42;
    EXPECT_EQ(p1->id, 42u);

    pool.deallocate(p1);
    EXPECT_EQ(pool.allocatedBlocks(), 0u);
    EXPECT_EQ(pool.availableBlocks(), 16u);
}

TEST(LockFreeMemoryPoolTest, ConstructAndDestroy)
{
    TrackedObject::destructorCount.store(0);
    LockFreeMemoryPool<TrackedObject, 8> pool;

    TrackedObject* obj = pool.construct(100);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->value, 100);

    pool.destroy(obj);
    EXPECT_EQ(TrackedObject::destructorCount.load(), 1);
    EXPECT_EQ(pool.availableBlocks(), 8u);
}

TEST(LockFreeMemoryPoolTest, PoolExhaustionBoundary)
{
    LockFreeMemoryPool<int, 4> pool;
    int* p1 = pool.allocate();
    int* p2 = pool.allocate();
    int* p3 = pool.allocate();
    int* p4 = pool.allocate();

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p3, nullptr);
    ASSERT_NE(p4, nullptr);
    EXPECT_EQ(pool.availableBlocks(), 0u);

    // 5th allocation must fail
    int* p5 = pool.allocate();
    EXPECT_EQ(p5, nullptr);

    // Return 1 block
    pool.deallocate(p1);
    EXPECT_EQ(pool.availableBlocks(), 1u);

    p5 = pool.allocate();
    EXPECT_NE(p5, nullptr);
}

TEST(LockFreeMemoryPoolTest, MultiThreadedConcurrentStressTest)
{
    constexpr std::size_t TotalOpsPerThread = 100'000;
    constexpr std::size_t NumThreads = 4;

    LockFreeMemoryPool<SamplePacket, 1024> pool;

    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < NumThreads; ++i) {
        threads.emplace_back([&pool, i]() {
            for (std::size_t op = 0; op < TotalOpsPerThread; ++op) {
                SamplePacket* pkt = nullptr;
                while ((pkt = pool.allocate()) == nullptr) {
                    std::this_thread::yield();
                }
                pkt->id = op + i;
                std::this_thread::yield();
                pool.deallocate(pkt);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(pool.allocatedBlocks(), 0u);
    EXPECT_EQ(pool.availableBlocks(), 1024u);
}
