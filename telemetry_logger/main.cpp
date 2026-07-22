/**
 * @file main.cpp
 * @brief High-frequency telemetry ingestion daemon CLI with SQLite, CSV, and Binary output format support.
 */

#include "BinaryWriter.h"
#include "CsvWriter.h"
#include "SqliteWriter.h"
#include "TelemetryIngestor.h"

#include <cstdlib>
#include <glog/logging.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char* argv[])
{
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1;

    std::string mode = "simulate";
    std::string format = "sqlite";
    std::string outFile = "";
    std::size_t ratePerSec = 10000;
    double durationSec = 5.0;
    uint16_t udpPort = 9000;
    std::size_t batchSize = 500;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--mode=") == 0) {
            mode = arg.substr(7);
        } else if (arg.find("--format=") == 0) {
            format = arg.substr(9);
        } else if (arg.find("--out=") == 0) {
            outFile = arg.substr(6);
        } else if (arg.find("--rate=") == 0) {
            ratePerSec = static_cast<std::size_t>(std::atoll(arg.substr(7).c_str()));
        } else if (arg.find("--duration=") == 0) {
            durationSec = std::atof(arg.substr(11).c_str());
        } else if (arg.find("--port=") == 0) {
            udpPort = static_cast<uint16_t>(std::atoi(arg.substr(7).c_str()));
        } else if (arg.find("--batch-size=") == 0) {
            batchSize = static_cast<std::size_t>(std::atoll(arg.substr(13).c_str()));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: telemetry_logger [OPTIONS]\n"
                      << "Options:\n"
                      << "  --mode=simulate|udp     Ingestion mode (default: simulate)\n"
                      << "  --format=sqlite|csv|binary Storage backend format (default: sqlite)\n"
                      << "  --out=<PATH>            Output file path\n"
                      << "  --rate=<RATE>           Packets per second for simulation (default: 10,000)\n"
                      << "  --duration=<SECONDS>    Logging duration in seconds (default: 5.0)\n"
                      << "  --port=<PORT>           UDP listening port (default: 9000)\n"
                      << "  --batch-size=<COUNT>    Batch write size (default: 500)\n"
                      << "  -h, --help              Show this help message\n";
            return 0;
        }
    }

    if (outFile.empty()) {
        if (format == "sqlite") {
            outFile = "telemetry_log.db";
        } else if (format == "csv") {
            outFile = "telemetry_log.csv";
        } else if (format == "binary") {
            outFile = "telemetry_log.bin";
        } else {
            outFile = "telemetry_log.out";
        }
    }

    std::unique_ptr<Communication::ITelemetryWriter> writer;
    if (format == "sqlite") {
        writer = std::make_unique<Communication::SqliteWriter>();
    } else if (format == "csv") {
        writer = std::make_unique<Communication::CsvWriter>();
    } else if (format == "binary") {
        writer = std::make_unique<Communication::BinaryWriter>();
    } else {
        LOG(ERROR) << "Unknown output format: " << format << ". Supported formats: sqlite, csv, binary";
        return 1;
    }

    if (!writer->open(outFile)) {
        LOG(ERROR) << "Failed to initialize telemetry writer for destination: " << outFile;
        return 1;
    }

    std::cout << "\n=========================================================\n";
    std::cout << "          HIGH-FREQUENCY TELEMETRY LOGGER Daemon         \n";
    std::cout << "=========================================================\n";
    std::cout << " Ingestion Mode: " << mode << "\n";
    std::cout << " Storage Backend: " << format << "\n";
    std::cout << " Output File:     " << outFile << "\n";
    std::cout << " Target Rate:     " << ratePerSec << " pkts/sec\n";
    std::cout << " Batch Size:      " << batchSize << " pkts/batch\n";
    std::cout << " Duration:        " << durationSec << " seconds\n";
    std::cout << "=========================================================\n\n";

    Communication::TelemetryIngestor ingestor(std::move(writer), batchSize);

    auto startTime = std::chrono::high_resolution_clock::now();

    if (mode == "udp") {
        ingestor.startUdp(udpPort, durationSec);
    } else {
        ingestor.startSimulation(ratePerSec, durationSec);
    }

    std::this_thread::sleep_for(std::chrono::duration<double>(durationSec + 0.2));

    ingestor.stop();

    auto endTime = std::chrono::high_resolution_clock::now();
    double totalSec = std::chrono::duration<double>(endTime - startTime).count();

    std::cout << "\n=========================================================\n";
    std::cout << "                    EXECUTION SUMMARY                    \n";
    std::cout << "=========================================================\n";
    std::cout << " Total Time:       " << std::fixed << std::setprecision(3) << totalSec << " s\n";
    std::cout << " Total Ingested:   " << ingestor.totalIngested() << " packets\n";
    std::cout << " Total Written:    " << ingestor.totalWritten() << " packets\n";
    std::cout << " Total Dropped:    " << ingestor.totalDropped() << " packets\n";
    if (totalSec > 0.0) {
        std::cout << " Ingest Rate:      " << std::fixed << std::setprecision(1)
                  << (static_cast<double>(ingestor.totalIngested()) / totalSec) << " pkts/sec\n";
        std::cout << " Disk Write Rate:  " << std::fixed << std::setprecision(1)
                  << (static_cast<double>(ingestor.totalWritten()) / totalSec) << " pkts/sec\n";
    }
    std::cout << "=========================================================\n\n";

    return 0;
}
