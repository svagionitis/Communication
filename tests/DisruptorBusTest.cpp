/**
 * @file DisruptorBusTest.cpp
 * @brief Unit tests for Single-Producer Multi-Consumer Lock-Free DisruptorBus.
 */

#include "DisruptorBus.h"

#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace Communication;

TEST(DisruptorBusTest, BasicPublishConsume)
{
    DisruptorBus<int, 16, 2> bus;
    EXPECT_EQ(bus.capacity(), 16u);
    EXPECT_EQ(bus.consumerCount(), 2u);

    EXPECT_EQ(bus.available(0), 0u);
    EXPECT_EQ(bus.available(1), 0u);

    EXPECT_TRUE(bus.publish(100));
    EXPECT_TRUE(bus.publish(200));

    EXPECT_EQ(bus.available(0), 2u);
    EXPECT_EQ(bus.available(1), 2u);

    int val0 = 0, val1 = 0;
    EXPECT_TRUE(bus.consume(0, val0));
    EXPECT_EQ(val0, 100);

    EXPECT_TRUE(bus.consume(1, val1));
    EXPECT_EQ(val1, 100);

    EXPECT_EQ(bus.available(0), 1u);
    EXPECT_EQ(bus.available(1), 1u);
}

TEST(DisruptorBusTest, MultiConsumerBroadcast)
{
    DisruptorBus<std::size_t, 64, 3> bus;
    constexpr std::size_t count = 50;

    for (std::size_t i = 0; i < count; ++i) {
        EXPECT_TRUE(bus.publish(i));
    }

    for (std::size_t consumerId = 0; consumerId < 3; ++consumerId) {
        EXPECT_EQ(bus.available(consumerId), count);
        for (std::size_t i = 0; i < count; ++i) {
            std::size_t val = 0;
            EXPECT_TRUE(bus.consume(consumerId, val));
            EXPECT_EQ(val, i);
        }
        EXPECT_EQ(bus.available(consumerId), 0u);
    }
}

TEST(DisruptorBusTest, SlowConsumerGating)
{
    DisruptorBus<int, 4, 2> bus;

    // Fill capacity for both consumers
    EXPECT_TRUE(bus.publish(1));
    EXPECT_TRUE(bus.publish(2));
    EXPECT_TRUE(bus.publish(3));
    EXPECT_TRUE(bus.publish(4));

    // Buffer is now full (4 items)
    EXPECT_FALSE(bus.publish(5));

    // Fast consumer 0 consumes 2 items
    int val = 0;
    EXPECT_TRUE(bus.consume(0, val));
    EXPECT_TRUE(bus.consume(0, val));

    // Producer still gated because slow consumer 1 has NOT consumed anything!
    EXPECT_FALSE(bus.publish(5));

    // Slow consumer 1 consumes 1 item
    EXPECT_TRUE(bus.consume(1, val));

    // Now producer can publish 1 item!
    EXPECT_TRUE(bus.publish(5));
}

TEST(DisruptorBusTest, PeekAndAdvance)
{
    DisruptorBus<std::string, 16, 1> bus;

    bus.publish("EVENT_ALPHA");
    bus.publish("EVENT_BETA");

    const std::string* peekVal = bus.peek(0);
    ASSERT_NE(peekVal, nullptr);
    EXPECT_EQ(*peekVal, "EVENT_ALPHA");

    bus.advance(0, 1);

    peekVal = bus.peek(0);
    ASSERT_NE(peekVal, nullptr);
    EXPECT_EQ(*peekVal, "EVENT_BETA");
}

TEST(DisruptorBusTest, MultiThreadedSpmcStressTest)
{
    constexpr std::size_t TotalEvents = 1'000'000;
    constexpr std::size_t NumConsumers = 3;

    DisruptorBus<uint64_t, 4096, NumConsumers> bus;

    std::atomic<bool> producerDone {false};
    std::atomic<uint64_t> producerChecksum {0};
    std::array<std::atomic<uint64_t>, NumConsumers> consumerChecksums {};
    std::array<std::atomic<uint64_t>, NumConsumers> consumerCounts {};

    for (std::size_t i = 0; i < NumConsumers; ++i) {
        consumerChecksums[i].store(0);
        consumerCounts[i].store(0);
    }

    std::thread producer([&]() {
        uint64_t sum = 0;
        for (uint64_t i = 1; i <= TotalEvents; ++i) {
            while (!bus.publish(i)) {
                std::this_thread::yield();
            }
            sum += i;
        }
        producerChecksum.store(sum);
        producerDone.store(true);
    });

    std::vector<std::thread> consumers;
    for (std::size_t id = 0; id < NumConsumers; ++id) {
        consumers.emplace_back([&, id]() {
            uint64_t localSum = 0;
            uint64_t count = 0;

            while (!producerDone.load(std::memory_order_acquire) || bus.available(id) > 0) {
                uint64_t val = 0;
                if (bus.consume(id, val)) {
                    localSum += val;
                    count++;
                } else {
                    std::this_thread::yield();
                }
            }

            consumerChecksums[id].store(localSum);
            consumerCounts[id].store(count);
        });
    }

    producer.join();
    for (auto& t : consumers) {
        t.join();
    }

    for (std::size_t id = 0; id < NumConsumers; ++id) {
        EXPECT_EQ(consumerCounts[id].load(), TotalEvents);
        EXPECT_EQ(consumerChecksums[id].load(), producerChecksum.load());
    }
}
