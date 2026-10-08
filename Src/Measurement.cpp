#include "Measurement.hpp"
#include <chrono>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>

uint64_t Measurement::GetTimeinNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void Measurement::Reset(size_t expectedSamples) {
    samples_.clear();
    samples_.reserve(expectedSamples);
    startTimeInNs_.reset();
    endTimeInNs_.reset();
}

void Measurement::MarkStart() { startTimeInNs_ = GetTimeinNs(); }
void Measurement::MarkEnd() { endTimeInNs_ = GetTimeinNs(); }

void Measurement::RecordReceived(uint64_t sequenceId, size_t payloadBytes, double rttMs) {
    samples_.push_back(Sample{sequenceId, payloadBytes, rttMs, true});
}

void Measurement::RecordLost(uint64_t sequenceId, size_t payloadBytes) {
    samples_.push_back(Sample{sequenceId, payloadBytes, 0.0, false});
}

Statistics Measurement::ComputeStatistics() const {
    Statistics stats;
    stats.sent = samples_.size();

    std::vector<double> rtts;
    rtts.reserve(samples_.size());

    for (const auto& sample : samples_) {
        stats.totalBytes += static_cast<double>(sample.payloadBytes);
        if (sample.received) {
            rtts.push_back(sample.rttMs);
        } 
        else {
            stats.lost++;
        }
    }    
    
    stats.received = stats.sent - stats.lost;
    stats.lossPercent = stats.sent > 0 ? (100.0 * static_cast<double>(stats.lost) / static_cast<double>(stats.sent)) : 0.0;

    if (!rtts.empty()) {
        stats.minRttMs = *std::min_element(rtts.begin(), rtts.end());
        stats.maxRttMs = *std::max_element(rtts.begin(), rtts.end());
        double sumRtt = std::accumulate(rtts.begin(), rtts.end(), 0.0);
        stats.avgRttMs = sumRtt / static_cast<double>(rtts.size());

        double sqSum = 0.0;
        for (double v : rtts) sqSum += (v - stats.avgRttMs) * (v - stats.avgRttMs);
        stats.stddevRttMs = rtts.size() > 1 ? std::sqrt(sqSum / static_cast<double>(rtts.size() - 1)) : 0.0;

        if (rtts.size() > 1) {
            double accumulatedJitter = 0.0;
            for (size_t i = 1; i < rtts.size(); ++i) accumulatedJitter += std::fabs(rtts[i] - rtts[i - 1]);
            stats.jitterMs = accumulatedJitter / static_cast<double>(rtts.size() - 1);
        }
    }

    if (startTimeInNs_ && endTimeInNs_ && *endTimeInNs_ > *startTimeInNs_) {
        stats.durationSeconds = static_cast<double>(*endTimeInNs_ - *startTimeInNs_) / 1e9;
        if (stats.durationSeconds > 0.0) {
            double bits = stats.totalBytes * 8.0;
            stats.throughputMbps = (bits / stats.durationSeconds) / 1e6;
        }
    }

    return stats;
}

void Measurement::PrintSummary(const std::string& scenarioLabel) const {
    Statistics stats = ComputeStatistics();

    std::cout << "\n===== Results";
    if (!scenarioLabel.empty()) std::cout << ": " << scenarioLabel;
    std::cout << " =====\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  Packets sent      : " << stats.sent << "\n";
    std::cout << "  Packets received  : " << stats.received << "\n";
    std::cout << "  Packet loss       : " << stats.lossPercent << " %\n";
    std::cout << "  RTT min / avg / max : " << stats.minRttMs << " / " << stats.avgRttMs << " / " << stats.maxRttMs << " ms\n";
    std::cout << "  RTT stddev        : " << stats.stddevRttMs << " ms\n";
    std::cout << "  Jitter (mean |Δ|) : " << stats.jitterMs << " ms\n";
    std::cout << "  Duration          : " << stats.durationSeconds << " s\n";   
    std::cout << "  Bytes sent        : " << static_cast<uint64_t>(stats.totalBytes / 1024) << " KB (" << stats.totalBytes / (1024*1024) << " MB)\n";
    std::cout << "  Throughput        : " << stats.throughputMbps << " Mbps\n";
    std::cout << "=========================================\n";
}

/*bool Measurement::Export2CSV(const std::string& path) const {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << "sequence_id,payload_bytes,received,rtt_ms\n";
    for (const auto& sample : samples_) {
        out << sample.sequenceId << ","
            << sample.payloadBytes << ","
            << (sample.received ? 1 : 0) << ","
            << (sample.received ? std::to_string(sample.rttMs) : std::string("")) << "\n";
    }
    return true;
}*/

