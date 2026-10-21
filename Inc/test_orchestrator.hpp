#pragma once
#include "packet_types.hpp"
#include "measurement.hpp"

#define ECHO_REQ()                                      ((config_.runEcho.value()) && (!config_.kemAlgorithm.has_value()))
#define KEM_REQ()                                       ((!config_.runEcho.has_value()) && (config_.kemAlgorithm.has_value()))

#define ML_KEM_512                                      "ML-KEM-512"
#define ML_KEM_768                                      "ML-KEM-768"
#define ML_KEM_1024                                     "ML-KEM-1024"

/*#define ML_DSA_44                                       "ML-DSA-44"
#define ML_DSA_65                                       "ML-DSA-65"
#define ML_DSA_87                                       "ML-DSA-87"
#define FALCON_512                                      "Falcon-512"*/

namespace DefaultConfigs {
    static constexpr bool DEFAULT_ECHO_STATE            = false;
    static constexpr PacketType DEFAULT_PACKET_SIZE     = PacketType::SMALL;
    static inline const std::string DEFAULT_IP          = "10.34.26.3";
    static constexpr uint16_t DEFAULT_PORT              = 5000;
    static constexpr size_t DEFAULT_EXCHANGE_REPEAT_NUM = 1;
    static constexpr size_t DEFAULT_PACKET_COUNT        = 10000;
    static constexpr int DEFAULT_TIMEOUT_MS             = 1000;
    static constexpr uint32_t DEFAULT_PPS               = 0; 
};

struct TestConfig {
    std::optional<bool>             runEcho             = DefaultConfigs::DEFAULT_ECHO_STATE;
    std::optional<size_t>           excRepeatNumber     = DefaultConfigs::DEFAULT_EXCHANGE_REPEAT_NUM;
    std::optional<std::string>      kemAlgorithm;
    //std::optional<std::string>    signAlgorithm;
    std::optional<Protocol>         protocol;
    std::optional<PacketType>       packetType          = DefaultConfigs::DEFAULT_PACKET_SIZE;
    std::optional<std::string>      host                = DefaultConfigs::DEFAULT_IP;
    std::optional<uint16_t>         port                = DefaultConfigs::DEFAULT_PORT;
    std::optional<size_t>           packetCount         = DefaultConfigs::DEFAULT_PACKET_COUNT;
    std::optional<int>              timeoutMs           = DefaultConfigs::DEFAULT_TIMEOUT_MS; 
    std::optional<uint32_t>         packetsPerSecond    = DefaultConfigs::DEFAULT_PPS;

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
