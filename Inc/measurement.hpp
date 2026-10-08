#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <optional>

struct Sample {
    uint64_t sequenceId;
    size_t   payloadBytes;
    double   rttMs;      // only meaningful if received == true
    bool     received;
};

struct Statistics {
    size_t sent             = 0;
    size_t received         = 0;
    size_t lost             = 0;
    double lossPercent      = 0.0;

    double minRttMs         = 0.0;
    double maxRttMs         = 0.0;
    double avgRttMs         = 0.0;
    double jitterMs         = 0.0; // mean absolute deviation between consecutive RTTs
    double stddevRttMs      = 0.0;

    double totalBytes       = 0.0;
    double durationSeconds  = 0.0;
    double throughputMbps   = 0.0; // application-layer payload throughput
};

// ---------------------------------------------------------------------------
// Measurement is transport-agnostic and crypto-agnostic: it just collects
// (sequenceId, size, rtt, received) samples handed to it by whoever is
// running the test (TestOrchestrator) and turns them into statistics /
// a CSV export. This keeps it reusable across TCP, UDP, and any future
// encrypted variants without modification.
// ---------------------------------------------------------------------------
class Measurement {
public:
    void Reset(size_t expectedSamples);
    void MarkStart();
    void MarkEnd();

    void RecordReceived(uint64_t sequenceId, size_t payloadBytes, double rttMs);
    void RecordLost(uint64_t sequenceId, size_t payloadBytes);

    Statistics ComputeStatistics() const;
    const std::vector<Sample>& samples() const { return samples_; }

    void PrintSummary(const std::string& scenarioLabel = "") const;
    bool Export2CSV(const std::string& path) const;

private:
    std::vector<Sample> samples_;
    std::optional<uint64_t> startTimeInNs_;
    std::optional<uint64_t> endTimeInNs_;

    static uint64_t GetTimeinNs();
};
