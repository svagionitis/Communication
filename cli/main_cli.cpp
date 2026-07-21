/**
 * @file main_cli.cpp
 * @brief High-performance Command Line Client for TCP, UDP, and Serial communication testing.
 */

#include "ICommunication.h"
#include "SerialPort.h"
#include "TcpClient.h"
#include "UdpSocket.h"
#include <glog/logging.h>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

static void printUsage(const char* progName)
{
    std::cout << "Usage: " << progName << " --mode=<tcp|udp|serial> [options]\n"
              << "Options:\n"
              << "  --host=<IP/Domain>    (TCP/UDP remote host, default: 127.0.0.1)\n"
              << "  --port=<port>         (TCP/UDP port, default: 8080)\n"
              << "  --device=<port_name>  (Serial device, e.g., /dev/ttyUSB0 or COM1)\n"
              << "  --baud=<baud_rate>    (Serial baud rate, default: 115200)\n"
              << "  --interactive         (Interactive messaging prompt)\n"
              << "  --help                (Show this help message)\n";
}

int main(int argc, char* argv[])
{
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1;

    std::string mode = "tcp";
    std::string host = "127.0.0.1";
    uint16_t port = 8080;
    std::string device = "/dev/ttyUSB0";
    uint32_t baud = 115200;
    bool interactive = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--mode=", 0) == 0) {
            mode = arg.substr(7);
        } else if (arg.rfind("--host=", 0) == 0) {
            host = arg.substr(7);
        } else if (arg.rfind("--port=", 0) == 0) {
            port = static_cast<uint16_t>(std::stoi(arg.substr(7)));
        } else if (arg.rfind("--device=", 0) == 0) {
            device = arg.substr(9);
        } else if (arg.rfind("--baud=", 0) == 0) {
            baud = static_cast<uint32_t>(std::stoul(arg.substr(7)));
        } else if (arg == "--interactive") {
            interactive = true;
        } else if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
    }

    std::unique_ptr<Communication::ICommunication> channel;

    if (mode == "tcp") {
        Communication::TcpConfig config;
        config.host = host;
        config.port = port;
        channel = std::make_unique<Communication::TcpClient>(config);
        std::cout << "[CLI] Initialized TCP client targeting " << host << ":" << port << "\n";
    } else if (mode == "udp") {
        Communication::UdpConfig config;
        config.remoteHost = host;
        config.remotePort = port;
        channel = std::make_unique<Communication::UdpSocket>(config);
        std::cout << "[CLI] Initialized UDP socket targeting " << host << ":" << port << "\n";
    } else if (mode == "serial") {
        Communication::SerialConfig config;
        config.portName = device;
        config.baudRate = static_cast<Communication::BaudRate>(baud);
        channel = std::make_unique<Communication::SerialPort>(config);
        std::cout << "[CLI] Initialized Serial port on " << device << " @ " << baud << " baud\n";
    } else {
        std::cerr << "Unknown mode: " << mode << "\n";
        printUsage(argv[0]);
        return 1;
    }

    channel->registerReceiveCallback([](const std::vector<uint8_t>& data) {
        std::string text(reinterpret_cast<const char*>(data.data()), data.size());
        std::cout << "\n[Received " << data.size() << " bytes]: " << text << std::endl;
    });

    std::cout << "[CLI] Opening connection...\n";
    if (!channel->open()) {
        std::cerr << "[CLI] Failed to open channel.\n";
        return 1;
    }

    std::cout << "[CLI] Channel opened successfully.\n";

    if (interactive) {
        std::cout << "Type messages to send (or 'exit' to quit):\n> ";
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line == "exit" || line == "quit") {
                break;
            }
            if (!line.empty()) {
                channel->send(line);
            }
            std::cout << "> ";
        }
    } else {
        std::cout << "[CLI] Sending test heartbeat payload...\n";
        channel->send("Communication Library CLI Heartbeat Payload\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "[CLI] Closing channel...\n";
    channel->close();
    std::cout << "[CLI] Shutdown complete.\n";

    return 0;
}
