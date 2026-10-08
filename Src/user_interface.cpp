#include "user_interface.hpp"
#include <iostream>
#include <limits>

namespace {
int ReadMenuChoice(const std::string& prompt, int minVal, int maxVal) {
    while (true) {
        std::cout << prompt;
        int choice;
        if (std::cin >> choice && choice >= minVal && choice <= maxVal) return choice;
        std::cout << "  Invalid choice, try again.\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}
}

Protocol UserInterface::GetProtocolPrompt() {
    std::cout << "\nSelect protocol:\n  1) TCP\n  2) UDP\n";
    int choice = ReadMenuChoice("> ", 1, 2);
    return choice == 1 ? Protocol::TCP : Protocol::UDP;
}

PacketType UserInterface::GetPacketTypePrompt() {
    std::cout << "\nSelect packet size:\n"
              << "  1) SMALL  (" << PacketSpecs::SMALL_BYTES << " bytes)\n"
              << "  2) MEDIUM (" << PacketSpecs::MEDIUM_BYTES << " bytes)\n"
              << "  3) LARGE  (" << PacketSpecs::LARGE_BYTES << " bytes)\n"
              << "  4) IMIX   (mixed number of bytes)\n";
    int choice = ReadMenuChoice("> ", 1, 4);
    switch (choice) {
        case 1: return PacketType::SMALL;
        case 2: return PacketType::MEDIUM;
        case 3: return PacketType::LARGE;
        default: return PacketType::IMIX;
    }
}

std::string UserInterface::GetHostPrompt() {
    std::cout << "\nTarget IP address (the Raspberry Pi's ethernet IP) [10.34.26.3]: ";
    std::string host;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::getline(std::cin, host);
    return host.empty() ? DefaultConfigs::DEFAULT_IP : host;
}

uint16_t UserInterface::GetPortPrompt() {
    std::cout << "Port [5000]: ";
    std::string line;
    std::getline(std::cin, line);
    if (line.empty()) return 5000;
    return static_cast<uint16_t>(std::stoi(line));
}

size_t UserInterface::GetCountPrompt() {
    std::cout << "Number of packets to send [" << DefaultConfigs::DEFAULT_PACKET_COUNT << "]";
    std::string line;
    std::getline(std::cin, line);
    if (line.empty()) return DefaultConfigs::DEFAULT_PACKET_COUNT;
    return static_cast<size_t>(std::stoul(line));
}

uint32_t UserInterface::GetPPSPrompt() {
    std::cout << "Packets per second (0 is unlimited, for UDP only) [" << DefaultConfigs::DEFAULT_PPS << "]";
    std::string line;
    std::getline(std::cin, line);
    if (line.empty()) return DefaultConfigs::DEFAULT_PPS;
    return static_cast<uint32_t>(std::stoul(line));
}

TestConfig UserInterface::CreateConfigFromPrompts() const {
    TestConfig cfg;
    cfg.protocol = GetProtocolPrompt();
    cfg.packetType = GetPacketTypePrompt();
    cfg.host = GetHostPrompt();
    cfg.port = GetPortPrompt();
    cfg.packetCount = GetCountPrompt();
    cfg.packetsPerSecond = GetPPSPrompt();
    return cfg;
}

/*void UserInterface::Export2CSV(const Measurement& measurement) const {
    std::cout << "\nExport raw samples to CSV? (y/N): ";
    std::string answer;
    std::getline(std::cin, answer);
    if (!answer.empty() && (answer[0] == 'y' || answer[0] == 'Y')) {
        std::cout << "File path [results.csv]: ";
        std::string path;
        std::getline(std::cin, path);
        if (path.empty()) path = "results.csv";
        if (measurement.exportCsv(path)) {
            std::cout << "Saved to " << path << "\n";
        } else {
            std::cout << "Failed to write " << path << "\n";
        }
    }
}*/

void UserInterface::Run() {
    std::cout << "=== Network Test Client ===\n";
    while (true) {
        TestConfig cfg = CreateConfigFromPrompts();

        TestOrchestrator orchestrator(cfg);
        if (!orchestrator.Run()) {
            std::cout << "Scenario failed to run (see errors above).\n";
        } else {
            std::string label = ToString(cfg.protocol) + " / " + PacketSpecs::GetName(cfg.packetType);
            orchestrator.measurement().PrintSummary(label);
            //aybeExportCsv(orchestrator.measurement());
        }

        std::cout << "\nRun another scenario? (Y/n): ";
        std::string again;
        std::getline(std::cin, again);
        if (!again.empty() && (again[0] == 'n' || again[0] == 'N')) break;
    }
    std::cout << "Goodbye.\n";
}
