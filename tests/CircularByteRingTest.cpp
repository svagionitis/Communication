/**
 * @file CircularByteRingTest.cpp
 * @brief Unit tests for Single-Producer Single-Consumer Zero-Copy CircularByteRing.
 */

#include "CircularByteRing.h"

#include <atomic>
#include <cstring>
#include <gtest/gtest.h>
#include <numeric>
#include <thread>

using namespace Communication;

TEST(CircularByteRingTest, BasicReadWriteExact)
{
    CircularByteRing<1024> ring;
    EXPECT_EQ(ring.availableRead(), 0u);
    EXPECT_EQ(ring.availableWrite(), 1024u);

    const uint8_t input[] = "HELLO_WORLD_12345";
    std::size_t inputLen = std::strlen(reinterpret_cast<const char*>(input));

    EXPECT_TRUE(ring.writeExact(input, inputLen));
    EXPECT_EQ(ring.availableRead(), inputLen);
    EXPECT_EQ(ring.availableWrite(), 1024u - inputLen);

    uint8_t output[64] = {0};
    EXPECT_TRUE(ring.readExact(output, inputLen));
    EXPECT_STREQ(reinterpret_cast<const char*>(output), "HELLO_WORLD_12345");
    EXPECT_EQ(ring.availableRead(), 0u);
}

TEST(CircularByteRingTest, ZeroCopyWriteViewAndAdvance)
{
    CircularByteRing<16> ring;
    auto writeView = ring.getWriteView();

    EXPECT_NE(writeView.first.data, nullptr);
    EXPECT_EQ(writeView.first.size, 16u);
    EXPECT_EQ(writeView.second.size, 0u);

    std::memcpy(writeView.first.data, "0123456789", 10);
    ring.advanceWrite(10);

    EXPECT_EQ(ring.availableRead(), 10u);

    uint8_t readBuf[16] = {0};
    EXPECT_TRUE(ring.readExact(readBuf, 10));
    EXPECT_EQ(std::memcmp(readBuf, "0123456789", 10), 0);
}

TEST(CircularByteRingTest, WrapAroundZeroCopyViews)
{
    CircularByteRing<16> ring;

    // Advance tail and head near end of ring buffer
    const uint8_t dummy[12] = {0};
    EXPECT_TRUE(ring.writeExact(dummy, 12));
    uint8_t readDummy[12];
    EXPECT_TRUE(ring.readExact(readDummy, 12));

    // Tail is at offset 12. Total free = 16. Space to end = 4.
    auto writeView = ring.getWriteView();
    EXPECT_EQ(writeView.first.size, 4u);
    EXPECT_EQ(writeView.second.size, 12u);

    std::memcpy(writeView.first.data, "ABCD", 4);
    std::memcpy(writeView.second.data, "EFGHIJKL", 8);
    ring.advanceWrite(12);

    EXPECT_EQ(ring.availableRead(), 12u);

    auto readView = ring.getReadView();
    EXPECT_EQ(readView.first.size, 4u);
    EXPECT_EQ(readView.second.size, 8u);

    EXPECT_EQ(std::memcmp(readView.first.data, "ABCD", 4), 0);
    EXPECT_EQ(std::memcmp(readView.second.data, "EFGHIJKL", 8), 0);

    uint8_t fullRead[16] = {0};
    EXPECT_TRUE(ring.readExact(fullRead, 12));
    EXPECT_EQ(std::memcmp(fullRead, "ABCDEFGHIJKL", 12), 0);
}

TEST(CircularByteRingTest, FindByteDelimiter)
{
    CircularByteRing<32> ring;

    const uint8_t line[] = "NMEA_AIVDM_FRAME\nNEXT_LINE";
    EXPECT_TRUE(ring.writeExact(line, std::strlen(reinterpret_cast<const char*>(line))));

    std::size_t offset = ring.findByte('\n');
    EXPECT_NE(offset, CircularByteRing<32>::npos);
    EXPECT_EQ(offset, 16u);

    ring.advanceRead(offset + 1); // Skip line and \n
    std::size_t nextOffset = ring.findByte('\n');
    EXPECT_EQ(nextOffset, CircularByteRing<32>::npos);
}

TEST(CircularByteRingTest, MultiThreadedSpscStressTest)
{
    constexpr std::size_t TotalBytes = 10'000'000;
    CircularByteRing<8192> ring;

    std::atomic<bool> producerDone {false};
    std::atomic<uint64_t> totalBytesRead {0};
    std::atomic<uint64_t> checksumProducer {0};
    std::atomic<uint64_t> checksumConsumer {0};

    std::thread producer([&]() {
        uint64_t localSum = 0;
        std::size_t bytesSent = 0;
        uint8_t val = 1;

        while (bytesSent < TotalBytes) {
            auto writeView = ring.getWriteView();
            if (writeView.first.size > 0) {
                std::size_t toWrite = std::min(writeView.first.size, TotalBytes - bytesSent);
                for (std::size_t i = 0; i < toWrite; ++i) {
                    writeView.first.data[i] = val;
                    localSum += val;
                    val = static_cast<uint8_t>(val + 1);
                }
                ring.advanceWrite(toWrite);
                bytesSent += toWrite;
            } else {
                std::this_thread::yield();
            }
        }
        checksumProducer.store(localSum);
        producerDone.store(true);
    });

    std::thread consumer([&]() {
        uint64_t localSum = 0;
        uint64_t count = 0;

        while (!producerDone.load() || ring.availableRead() > 0) {
            auto readView = ring.getReadView();
            if (readView.first.size > 0) {
                for (std::size_t i = 0; i < readView.first.size; ++i) {
                    localSum += readView.first.data[i];
                }
                count += readView.first.size;
                ring.advanceRead(readView.first.size);
            } else {
                std::this_thread::yield();
            }
        }
        checksumConsumer.store(localSum);
        totalBytesRead.store(count);
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(totalBytesRead.load(), TotalBytes);
    EXPECT_EQ(checksumProducer.load(), checksumConsumer.load());
}
