/**
 * @file main.cpp
 * @brief Main entry point for AIS & ADS-B Tracking Hub daemon CLI.
 */

#include "AisStreamProvider.h"
#include "OpenSkyProvider.h"
#include "SimulatorProvider.h"
#include "TrackingEngine.h"

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
    std::string dbPath = "tracking.db";
    std::size_t ratePerSec = 10000;
    double durationSec = 5.0;
    double openSkyIntervalSec = 5.0;
    std::string aisApiKey = "";
    std::size_t batchSize = 500;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--mode=") == 0) {
            mode = arg.substr(7);
        } else if (arg.find("--db=") == 0) {
            dbPath = arg.substr(5);
        } else if (arg.find("--rate=") == 0) {
            ratePerSec = static_cast<std::size_t>(std::atoll(arg.substr(7).c_str()));
        } else if (arg.find("--duration=") == 0) {
            durationSec = std::atof(arg.substr(11).c_str());
        } else if (arg.find("--opensky-interval=") == 0) {
            openSkyIntervalSec = std::atof(arg.substr(18).c_str());
        } else if (arg.find("--ais-key=") == 0) {
            aisApiKey = arg.substr(10);
        } else if (arg.find("--batch-size=") == 0) {
            batchSize = static_cast<std::size_t>(std::atoll(arg.substr(13).c_str()));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: tracking_hub [OPTIONS]\n"
                      << "Options:\n"
                      << "  --mode=simulate|opensky|aisstream|all Operating provider mode (default: simulate)\n"
                      << "  --db=<PATH>                           SQLite database output path (default: tracking.db)\n"
                      << "  --rate=<RATE>                         Simulation update rate (default: 10,000)\n"
                      << "  --opensky-interval=<SECONDS>          OpenSky REST API polling interval (default: 5.0s)\n"
                      << "  --ais-key=<KEY>                       AISStream API key\n"
                      << "  --duration=<SECONDS>                  Execution duration in seconds (default: 5.0)\n"
                      << "  --batch-size=<COUNT>                  Database batch size (default: 500)\n"
                      << "  -h, --help                            Show this help message\n";
            return 0;
        }
    }

    std::cout << "\n=========================================================\n";
    std::cout << "     AIS & ADS-B TELEMETRY TRACKING HUB Daemon          \n";
    std::cout << "=========================================================\n";
    std::cout << " Operating Mode: " << mode << "\n";
    std::cout << " Database Path:  " << dbPath << "\n";
    std::cout << " Batch Size:     " << batchSize << " records/batch\n";
    std::cout << " Duration:       " << durationSec << " seconds\n";
    std::cout << "=========================================================\n\n";

    Communication::TrackingEngine engine(batchSize);

    if (!engine.openDatabase(dbPath)) {
        LOG(ERROR) << "Failed to open spatial database at " << dbPath;
        return 1;
    }

    if (mode == "opensky") {
        engine.addProvider(std::make_unique<Communication::OpenSkyProvider>(openSkyIntervalSec));
    } else if (mode == "aisstream") {
        engine.addProvider(std::make_unique<Communication::AisStreamProvider>(aisApiKey));
    } else if (mode == "all") {
        engine.addProvider(std::make_unique<Communication::OpenSkyProvider>(openSkyIntervalSec));
        engine.addProvider(std::make_unique<Communication::AisStreamProvider>(aisApiKey));
        engine.addProvider(std::make_unique<Communication::SimulatorProvider>(ratePerSec));
    } else {
        engine.addProvider(std::make_unique<Communication::SimulatorProvider>(ratePerSec));
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    engine.start();

    std::this_thread::sleep_for(std::chrono::duration<double>(durationSec + 0.2));

    engine.stop();

    auto endTime = std::chrono::high_resolution_clock::now();
    double totalSec = std::chrono::duration<double>(endTime - startTime).count();

    std::cout << "\n=========================================================\n";
    std::cout << "                 TRACKING SESSION SUMMARY                \n";
    std::cout << "=========================================================\n";
    std::cout << " Total Time:       " << std::fixed << std::setprecision(3) << totalSec << " s\n";
    std::cout << " Total Ingested:   " << engine.totalIngested() << " frames\n";
    std::cout << " Total Written:    " << engine.totalWritten() << " records\n";
    std::cout << " Total Dropped:    " << engine.totalDropped() << " frames\n";
    if (totalSec > 0.0) {
        std::cout << " Ingest Rate:      " << std::fixed << std::setprecision(1)
                  << (static_cast<double>(engine.totalIngested()) / totalSec) << " frames/sec\n";
        std::cout << " DB Write Rate:    " << std::fixed << std::setprecision(1)
                  << (static_cast<double>(engine.totalWritten()) / totalSec) << " records/sec\n";
    }
    std::cout << "=========================================================\n\n";

    return 0;
}
