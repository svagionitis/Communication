/**
 * @file BenchmarkMetrics.h
 * @brief Latency and throughput metric collection for queue benchmarking.
 */

#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

namespace Communication {

/**
 * @brief Payload structure transferred across queues during benchmark.
 */
struct BenchmarkPacket {
    uint64_t sequenceNumber {0};
    std::chrono::high_resolution_clock::time_point timestamp {};
    uint8_t payload[64] {0};
};

/**
 * @brief Statistical latency summary in nanoseconds.
 */
struct LatencyStats {
    double minNs {0.0};
    double maxNs {0.0};
    double meanNs {0.0};
    double p50Ns {0.0};
    double p90Ns {0.0};
    double p99Ns {0.0};
    double p999Ns {0.0};
    double stdDevNs {0.0};
};

/**
 * @brief Comprehensive benchmark test results summary.
 */
struct BenchmarkResult {
    std::string name;
    std::size_t totalMessagesProcessed {0};
    std::size_t totalMessagesDropped {0};
    double totalTimeSeconds {0.0};
    double throughputMsgsPerSec {0.0};
    double throughputMBPerSec {0.0};
    LatencyStats latency;
};

/**
 * @brief High-precision latency histogram collector.
 */
class LatencyCollector {
public:
    void record(double latencyNs) { m_latencies.push_back(latencyNs); }

    void reserve(std::size_t count) { m_latencies.reserve(count); }

    LatencyStats computeStats()
    {
        LatencyStats stats {};
        if (m_latencies.empty()) {
            return stats;
        }

        std::sort(m_latencies.begin(), m_latencies.end());

        stats.minNs = m_latencies.front();
        stats.maxNs = m_latencies.back();

        double sum = std::accumulate(m_latencies.begin(), m_latencies.end(), 0.0);
        stats.meanNs = sum / static_cast<double>(m_latencies.size());

        auto getPercentile = [this](double percentile) -> double {
            std::size_t idx =
                static_cast<std::size_t>(std::round(percentile * static_cast<double>(m_latencies.size() - 1)));
            if (idx >= m_latencies.size()) {
                idx = m_latencies.size() - 1;
            }
            return m_latencies[idx];
        };

        stats.p50Ns = getPercentile(0.50);
        stats.p90Ns = getPercentile(0.90);
        stats.p99Ns = getPercentile(0.99);
        stats.p999Ns = getPercentile(0.999);

        double varianceSum = 0.0;
        for (double val : m_latencies) {
            double diff = val - stats.meanNs;
            varianceSum += diff * diff;
        }
        stats.stdDevNs = std::sqrt(varianceSum / static_cast<double>(m_latencies.size()));

        return stats;
    }

private:
    std::vector<double> m_latencies;
};

} // namespace Communication
