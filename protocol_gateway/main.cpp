/**
 * @file main.cpp
 * @brief Entry point for Low-Latency Industrial Protocol Gateway (Serial-to-TCP Bridge) CLI.
 */

#include "DuplexBridge.h"

#include <cstdlib>
#include <glog/logging.h>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1;

    std::string mode = "simulate";
    std::string serialDevice = "/dev/ttyUSB0";
    uint32_t baudRate = 115200;
    std::string tcpHost = "127.0.0.1";
    uint16_t tcpPort = 8080;
    std::size_t messageCount = 200000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--mode=") == 0) {
            mode = arg.substr(7);
        } else if (arg.find("--device=") == 0) {
            serialDevice = arg.substr(9);
        } else if (arg.find("--baud=") == 0) {
            baudRate = static_cast<uint32_t>(std::atoi(arg.substr(7).c_str()));
        } else if (arg.find("--host=") == 0) {
            tcpHost = arg.substr(7);
        } else if (arg.find("--port=") == 0) {
            tcpPort = static_cast<uint16_t>(std::atoi(arg.substr(7).c_str()));
        } else if (arg.find("--messages=") == 0) {
            messageCount = static_cast<std::size_t>(std::atoll(arg.substr(11).c_str()));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: protocol_gateway [OPTIONS]\n"
                      << "Options:\n"
                      << "  --mode=bridge|simulate  Gateway operating mode (default: simulate)\n"
                      << "  --device=<PATH>         Serial device node (default: /dev/ttyUSB0)\n"
                      << "  --baud=<RATE>           Serial baud rate (default: 115200)\n"
                      << "  --host=<HOST>           Remote TCP server IP/host (default: 127.0.0.1)\n"
                      << "  --port=<PORT>           Remote TCP server port (default: 8080)\n"
                      << "  --messages=<COUNT>      Simulated message count per direction (default: 200,000)\n"
                      << "  -h, --help              Show this help message\n";
            return 0;
        }
    }

    std::cout << "\n=========================================================\n";
    std::cout << "  FULL-DUPLEX INDUSTRIAL PROTOCOL GATEWAY BRIDGE         \n";
    std::cout << "=========================================================\n";
    std::cout << " Operating Mode:  " << mode << "\n";
    if (mode == "bridge") {
        std::cout << " Serial Device:   " << serialDevice << " (" << baudRate << " baud)\n";
        std::cout << " Remote TCP:      " << tcpHost << ":" << tcpPort << "\n";
    }
    std::cout << "=========================================================\n\n";

    Communication::SerialConfig serialConfig {};
    serialConfig.portName = serialDevice;
    serialConfig.baudRate = static_cast<Communication::BaudRate>(baudRate);

    Communication::TcpConfig tcpConfig {};
    tcpConfig.host = tcpHost;
    tcpConfig.port = tcpPort;

    Communication::DuplexBridge gateway(serialConfig, tcpConfig);

    if (mode == "simulate") {
        gateway.runSimulation(messageCount);
    } else {
        if (!gateway.start()) {
            LOG(ERROR) << "Failed to start full-duplex gateway bridge.";
            return 1;
        }

        std::cout << "[INFO] Gateway bridge active. Press Enter to stop...\n";
        std::cin.get();

        gateway.stop();

        auto s = gateway.stats();
        std::cout << "\n=========================================================\n";
        std::cout << "                LIVE GATEWAY SESSION STATS               \n";
        std::cout << "=========================================================\n";
        std::cout << " Serial -> TCP: " << s.serialToTcpPackets << " frames (" << s.serialToTcpBytes << " bytes, "
                  << s.droppedSerialToTcp << " dropped)\n";
        std::cout << " TCP -> Serial: " << s.tcpToSerialPackets << " frames (" << s.tcpToSerialBytes << " bytes, "
                  << s.droppedTcpToSerial << " dropped)\n";
        std::cout << "=========================================================\n\n";
    }

    return 0;
}
