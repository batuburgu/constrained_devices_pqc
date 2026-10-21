#include "user_interface.hpp"
#include "kem.hpp"
#include <iostream>
#include <limits>
#include <algorithm>   // std::find, std::sort
#include <iomanip>     // std::setw, std::setprecision
#include <numeric>     // std::accumulate
#include <stdexcept>   // std::runtime_error
#include <string>
#include <vector>

namespace {
    int ReadMenuChoice(const std::string& prompt, int minVal, int maxVal) {
        while (true) {
            std::cout << prompt;
            int choice;
            if (std::cin >> choice && choice >= minVal && choice <= maxVal) {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');  // drop the rest of the line
                return choice;
            }
            if (std::cin.eof()) throw std::runtime_error("input closed");  // otherwise this would loop forever
            std::cout << "  Invalid choice, try again.\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }
}

// Reads a line; an empty line keeps the default. Invalid or out-of-range input also keeps the default
// and says so, instead of throwing like std::stoi did.
static unsigned long ReadNumber(const std::string& prompt, unsigned long def, unsigned long minVal, unsigned long maxVal) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    if (line.empty()) return def;
    try {
        size_t used = 0;
        const unsigned long n = std::stoul(line, &used);
        if (used == line.size() && n >= minVal && n <= maxVal) return n;
    } catch (const std::exception&) {
    }
    std::cout << "  Invalid number, using " << def << ".\n";
    return def;
}

// A short list of common algorithms plus "Other", validated against what this build supports.
static std::string ChooseAlgorithm(const std::string& title, const std::vector<std::string>& common, const std::vector<std::string>& enabled) {
    std::cout << "\n" << title << ":\n";
    for (size_t i = 0; i < common.size(); ++i) std::cout << "  " << i + 1 << ") " << common[i] << "\n";
    const int other = static_cast<int>(common.size()) + 1;
    std::cout << "  " << other << ") Other (type a name)\n";

    const int choice = ReadMenuChoice("> ", 1, other);
    if (choice < other) return common[static_cast<size_t>(choice) - 1];

    while (true) {
        std::cout << "Algorithm name (or 'list' to see all available): ";
        std::string name;
        if (!std::getline(std::cin, name)) throw std::runtime_error("input closed");
        if (name == "list") {
            for (const auto& n : enabled) std::cout << "  " << n << "\n";
        } 
        else if (std::find(enabled.begin(), enabled.end(), name) != enabled.end()) {
            return name;
        } 
        else {
            std::cout << "  Not available in this build.\n";
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

UserInterface::Steps UserInterface::GetStepsPrompt() {
    while (true) {
        std::cout << "\nWhat do you want to run? (several allowed, e.g. 1,3)\n"
                  << "  1) KEM key exchange\n"
                  << "  2) Signature exchange\n"
                  << "  3) Echo test\n"
                  << "  4) Everything (KEM + signature + echo)\n> ";
        std::string line;
        if (!std::getline(std::cin, line)) throw std::runtime_error("input closed");

        Steps steps;
        bool valid = false;
        bool bad = false;
        for (char c : line) {
            switch (c) {
                case '1': steps.kem = true; valid = true; break;
                //case '2': steps.sig = true; valid = true; break;
                case '3': steps.echo = true; valid = true; break;
                case '4': steps.kem = /*steps.sig =*/ steps.echo = true; valid = true; break;
                case ' ':
                case ',': break;
                default: bad = true;
            }
        }
        if (valid && !bad) return steps;
        std::cout << "  Invalid choice, try again.\n";
    }
}

std::string UserInterface::GetKemPrompt() {
    return ChooseAlgorithm("KEM algorithm", {"ML-KEM-512", "ML-KEM-768", "ML-KEM-1024"}, pqc::KEM::GetEnabledAlgorithms());
}

/*std::string UserInterface::GetSigPrompt() {
    return ChooseAlgorithm("Signature algorithm", {"ML-DSA-44", "ML-DSA-65", "ML-DSA-87", "Falcon-512"},
                           pqc::Signature::GetEnabledAlgorithms());
}*/

size_t UserInterface::GetRepetitionsPrompt() {
    return static_cast<size_t>(ReadNumber("Repetitions of the exchange [1]: ", 1, 1, 100000));
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
    cfg.protocol.value() = GetProtocolPrompt();

    const Steps steps = GetStepsPrompt();
    cfg.runEcho.value() = steps.echo;
    if (steps.kem) cfg.kemAlgorithm.value() = GetKemPrompt();
    //if (steps.sig) cfg.sigAlgorithm = GetSigPrompt();
    if (steps.kem /*|| steps.sig*/) {
        cfg.excRepeatNumber.value() = GetRepetitionsPrompt();
        if (cfg.protocol.value() == Protocol::UDP)
            std::cout << "  Note: UDP exchanges have no retransmission; a lost datagram stalls the exchange until it times out.\n";
    }

    if (steps.echo) {  // echo-only questions are skipped when echo is not selected
        cfg.packetType.value() = GetPacketTypePrompt();
        cfg.packetCount.value() = GetCountPrompt();
        cfg.packetsPerSecond.value() = GetPPSPrompt();
    }

    cfg.host.value() = GetHostPrompt();
    cfg.port.value() = GetPortPrompt();
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
            std::string label = ToString(cfg.protocol.value()) + " / " + PacketSpecs::GetName(cfg.packetType.value());
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
