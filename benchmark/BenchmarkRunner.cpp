/**
 * @file BenchmarkRunner.cpp
 * @brief Multi-threaded benchmark implementation for queue jitter and throughput visualization.
 */

#include "BenchmarkRunner.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <thread>

namespace Communication {

BenchmarkRunner::BenchmarkRunner(std::size_t messageCount)
    : m_messageCount(messageCount)
{
}

BenchmarkResult BenchmarkRunner::runLockFreeSPSC()
{
    LockFreeRingBuffer<BenchmarkPacket, 4096> ringBuffer;
    LatencyCollector collector;
    collector.reserve(m_messageCount);

    std::atomic<bool> startFlag {false};
    std::atomic<std::size_t> droppedCount {0};

    auto producerFunc = [&]() {
        while (!startFlag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        for (uint64_t i = 0; i < m_messageCount; ++i) {
            BenchmarkPacket packet;
            packet.sequenceNumber = i;
            packet.timestamp = std::chrono::high_resolution_clock::now();

            while (!ringBuffer.push(packet)) {
                std::this_thread::yield();
            }
        }
    };

    auto consumerFunc = [&]() {
        while (!startFlag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        BenchmarkPacket packet;
        for (uint64_t i = 0; i < m_messageCount; ++i) {
            while (!ringBuffer.pop(packet)) {
                std::this_thread::yield();
            }
            auto now = std::chrono::high_resolution_clock::now();
            double latencyNs = static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now - packet.timestamp).count());
            collector.record(latencyNs);
        }
    };

    std::thread producer(producerFunc);
    std::thread consumer(consumerFunc);

    auto startTime = std::chrono::high_resolution_clock::now();
    startFlag.store(true, std::memory_order_release);

    producer.join();
    consumer.join();
    auto endTime = std::chrono::high_resolution_clock::now();

    double totalTimeSec = std::chrono::duration<double>(endTime - startTime).count();

    BenchmarkResult result {};
    result.name = "LockFreeRingBuffer (SPSC 4096)";
    result.totalMessagesProcessed = m_messageCount;
    result.totalMessagesDropped = droppedCount.load();
    result.totalTimeSeconds = totalTimeSec;
    result.throughputMsgsPerSec = static_cast<double>(m_messageCount) / totalTimeSec;
    result.throughputMBPerSec =
        (static_cast<double>(m_messageCount * sizeof(BenchmarkPacket)) / (1024.0 * 1024.0)) / totalTimeSec;
    result.latency = collector.computeStats();

    return result;
}

BenchmarkResult BenchmarkRunner::runThreadSafeQueue()
{
    ThreadSafeQueue<BenchmarkPacket> queue;
    LatencyCollector collector;
    collector.reserve(m_messageCount);

    std::atomic<bool> startFlag {false};

    auto producerFunc = [&]() {
        while (!startFlag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        for (uint64_t i = 0; i < m_messageCount; ++i) {
            BenchmarkPacket packet;
            packet.sequenceNumber = i;
            packet.timestamp = std::chrono::high_resolution_clock::now();
            queue.push(packet);
        }
    };

    auto consumerFunc = [&]() {
        while (!startFlag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        for (uint64_t i = 0; i < m_messageCount; ++i) {
            BenchmarkPacket packet = queue.pop();
            auto now = std::chrono::high_resolution_clock::now();
            double latencyNs = static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now - packet.timestamp).count());
            collector.record(latencyNs);
        }
    };

    std::thread producer(producerFunc);
    std::thread consumer(consumerFunc);

    auto startTime = std::chrono::high_resolution_clock::now();
    startFlag.store(true, std::memory_order_release);

    producer.join();
    consumer.join();
    auto endTime = std::chrono::high_resolution_clock::now();

    double totalTimeSec = std::chrono::duration<double>(endTime - startTime).count();

    BenchmarkResult result {};
    result.name = "ThreadSafeQueue (Mutex Deque)";
    result.totalMessagesProcessed = m_messageCount;
    result.totalMessagesDropped = 0;
    result.totalTimeSeconds = totalTimeSec;
    result.throughputMsgsPerSec = static_cast<double>(m_messageCount) / totalTimeSec;
    result.throughputMBPerSec =
        (static_cast<double>(m_messageCount * sizeof(BenchmarkPacket)) / (1024.0 * 1024.0)) / totalTimeSec;
    result.latency = collector.computeStats();

    return result;
}

BenchmarkResult BenchmarkRunner::runMutexRingBuffer()
{
    MutexRingBuffer<BenchmarkPacket, 4096> mutexRing;
    LatencyCollector collector;
    collector.reserve(m_messageCount);

    std::atomic<bool> startFlag {false};

    auto producerFunc = [&]() {
        while (!startFlag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        for (uint64_t i = 0; i < m_messageCount; ++i) {
            BenchmarkPacket packet;
            packet.sequenceNumber = i;
            packet.timestamp = std::chrono::high_resolution_clock::now();

            while (!mutexRing.push(packet)) {
                std::this_thread::yield();
            }
        }
    };

    auto consumerFunc = [&]() {
        while (!startFlag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        BenchmarkPacket packet;
        for (uint64_t i = 0; i < m_messageCount; ++i) {
            while (!mutexRing.pop(packet)) {
                std::this_thread::yield();
            }
            auto now = std::chrono::high_resolution_clock::now();
            double latencyNs = static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now - packet.timestamp).count());
            collector.record(latencyNs);
        }
    };

    std::thread producer(producerFunc);
    std::thread consumer(consumerFunc);

    auto startTime = std::chrono::high_resolution_clock::now();
    startFlag.store(true, std::memory_order_release);

    producer.join();
    consumer.join();
    auto endTime = std::chrono::high_resolution_clock::now();

    double totalTimeSec = std::chrono::duration<double>(endTime - startTime).count();

    BenchmarkResult result {};
    result.name = "MutexRingBuffer (Mutex 4096)";
    result.totalMessagesProcessed = m_messageCount;
    result.totalMessagesDropped = 0;
    result.totalTimeSeconds = totalTimeSec;
    result.throughputMsgsPerSec = static_cast<double>(m_messageCount) / totalTimeSec;
    result.throughputMBPerSec =
        (static_cast<double>(m_messageCount * sizeof(BenchmarkPacket)) / (1024.0 * 1024.0)) / totalTimeSec;
    result.latency = collector.computeStats();

    return result;
}

void BenchmarkRunner::printResultsTable(const std::vector<BenchmarkResult>& results) const
{
    std::cout << "\n==================================================================================================="
                 "======================\n";
    std::cout << "                                  LOCK-FREE VS MUTEX QUEUE BENCHMARK RESULTS                         "
                 "                    \n";
    std::cout << "====================================================================================================="
                 "====================\n";
    std::cout << std::left << std::setw(32) << "Implementation" << std::setw(16) << "Throughput(m/s)" << std::setw(12)
              << "Bandwidth" << std::setw(10) << "Min(ns)" << std::setw(10) << "Mean(ns)" << std::setw(10) << "P50(ns)"
              << std::setw(10) << "P99(ns)" << std::setw(10) << "P99.9(ns)" << std::setw(10) << "StdDev(ns)"
              << "\n";
    std::cout << "-----------------------------------------------------------------------------------------------------"
                 "--------------------\n";

    for (const auto& res : results) {
        std::stringstream bandwidthStr;
        bandwidthStr << std::fixed << std::setprecision(1) << res.throughputMBPerSec << " MB/s";

        std::cout << std::left << std::setw(32) << res.name << std::setw(16)
                  << static_cast<uint64_t>(res.throughputMsgsPerSec) << std::setw(12) << bandwidthStr.str()
                  << std::setw(10) << std::fixed << std::setprecision(0) << res.latency.minNs << std::setw(10)
                  << std::fixed << std::setprecision(0) << res.latency.meanNs << std::setw(10) << std::fixed
                  << std::setprecision(0) << res.latency.p50Ns << std::setw(10) << std::fixed << std::setprecision(0)
                  << res.latency.p99Ns << std::setw(10) << std::fixed << std::setprecision(0) << res.latency.p999Ns
                  << std::setw(10) << std::fixed << std::setprecision(0) << res.latency.stdDevNs << "\n";
    }
    std::cout << "====================================================================================================="
                 "====================\n\n";
}

} // namespace Communication
