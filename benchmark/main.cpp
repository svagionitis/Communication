/**
 * @file main.cpp
 * @brief Entry point for Lock-Free vs Mutex Queue Benchmark & Jitter Visualizer CLI.
 */

#include "BenchmarkRunner.h"

#include <cstdlib>
#include <glog/logging.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1;

    std::size_t messageCount = 2000000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--iterations" || arg == "-n") && i + 1 < argc) {
            messageCount = static_cast<std::size_t>(std::atoll(argv[++i]));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: lockfree_benchmark [OPTIONS]\n"
                      << "Options:\n"
                      << "  -n, --iterations <COUNT>   Number of messages to benchmark (default: 2,000,000)\n"
                      << "  -h, --help                 Show this help message\n";
            return 0;
        }
    }

    std::cout << "\n[INFO] Starting SPSC Lock-Free vs. Mutex Queue Benchmark Suite...\n";
    std::cout << "[INFO] Total Benchmark Iterations: " << messageCount << " messages per queue implementation\n";

    Communication::BenchmarkRunner runner(messageCount);

    std::cout << "[INFO] Running LockFreeRingBuffer (SPSC 4096)..." << std::flush;
    auto lockFreeRes = runner.runLockFreeSPSC();
    std::cout << " Done.\n";

    std::cout << "[INFO] Running DisruptorBus (SPMC Broadcast x3)..." << std::flush;
    auto disruptorRes = runner.runDisruptorBus();
    std::cout << " Done.\n";

    std::cout << "[INFO] Running CircularByteRing (Zero-Copy 64KB)..." << std::flush;
    auto byteRingRes = runner.runCircularByteRing();
    std::cout << " Done.\n";

    std::cout << "[INFO] Running MutexRingBuffer (Mutex 4096)..." << std::flush;
    auto mutexRingRes = runner.runMutexRingBuffer();
    std::cout << " Done.\n";

    std::cout << "[INFO] Running ThreadSafeQueue (Mutex Deque)..." << std::flush;
    auto threadSafeRes = runner.runThreadSafeQueue();
    std::cout << " Done.\n";

    std::vector<Communication::BenchmarkResult> results = {lockFreeRes, disruptorRes, byteRingRes, mutexRingRes,
                                                           threadSafeRes};
    runner.printResultsTable(results);

    if (mutexRingRes.throughputMsgsPerSec > 0.0) {
        double speedupVsMutexRing = lockFreeRes.throughputMsgsPerSec / mutexRingRes.throughputMsgsPerSec;
        double p99ReductionVsMutexRing =
            ((mutexRingRes.latency.p99Ns - lockFreeRes.latency.p99Ns) / mutexRingRes.latency.p99Ns) * 100.0;

        std::cout << "Key Insights:\n";
        std::cout << "  - LockFreeRingBuffer achieved " << std::fixed << std::setprecision(2) << speedupVsMutexRing
                  << "x throughput compared to MutexRingBuffer.\n";
        if (p99ReductionVsMutexRing > 0.0) {
            std::cout << "  - LockFreeRingBuffer reduced P99 latency jitter by " << std::fixed << std::setprecision(1)
                      << p99ReductionVsMutexRing << "% compared to MutexRingBuffer.\n";
        }
        std::cout << "\n";
    }

    return 0;
}
