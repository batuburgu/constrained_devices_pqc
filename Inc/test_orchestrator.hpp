#pragma once
#include "packet_types.hpp"
#include "measurement.hpp"

namespace DefaultConfigs {
    static constexpr Protocol DEFAULT_PROTOCOL          = Protocol::TCP;
    static constexpr PacketType DEFAULT_PACKET_SIZE     = PacketType::SMALL;
    static inline const std::string DEFAULT_IP          = "10.34.26.3";
    static constexpr uint16_t DEFAULT_PORT              = 5000;
    static constexpr size_t DEFAULT_PACKET_COUNT        = 10000;
    static constexpr int DEFAULT_TIMEOUT_MS             = 1000;
    static constexpr uint32_t DEFAULT_PPS               = 0; 
};

struct TestConfig {
    Protocol    protocol            = DefaultConfigs::DEFAULT_PROTOCOL;
    PacketType  packetType          = DefaultConfigs::DEFAULT_PACKET_SIZE;
    std::string host                = DefaultConfigs::DEFAULT_IP;
    uint16_t    port                = DefaultConfigs::DEFAULT_PORT;
    size_t      packetCount         = DefaultConfigs::DEFAULT_PACKET_COUNT;
    int         timeoutMs           = DefaultConfigs::DEFAULT_TIMEOUT_MS; 
    uint32_t    packetsPerSecond    = DefaultConfigs::DEFAULT_PPS;
};

// ---------------------------------------------------------------------------
// TestOrchestrator is the client-side conductor: for each configured packet
// size it builds a packet, times a send/receive round trip through the
// active IProtocol, runs the payload through CryptoManager, and records the
// outcome in Measurement. It depends only on the interfaces (IProtocol,
// CryptoManager, Measurement), not on TCP/UDP specifics or on any one
// cipher -- new transports or ciphers plug in without touching this class.
// ---------------------------------------------------------------------------
class TestOrchestrator {
public:
    explicit TestOrchestrator(TestConfig config);

    bool Run();

    const TestConfig& config() const { return config_; }
    const Measurement& measurement() const { return measurement_; }
    
private:
    TestConfig config_;
    Measurement measurement_;
};
