#include "packet_types.hpp"
#include <stdexcept>
#include <algorithm>
#include <cctype>

static std::string MakeUpperCase(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
    return s;
}

size_t PacketSpecs::GetPacketType(PacketType size) {
    switch (size) {
        case PacketType::SMALL:  return SMALL_BYTES;
        case PacketType::MEDIUM: return MEDIUM_BYTES;
        case PacketType::LARGE:  return LARGE_BYTES;
        case PacketType::IMIX:   return 0; // meaningless for IMIX, sequence varies
    }
    return 0;
}

std::string PacketSpecs::GetName(PacketType size) {
    switch (size) {
        case PacketType::SMALL:  return "SMALL";
        case PacketType::MEDIUM: return "MEDIUM";
        case PacketType::LARGE:  return "LARGE";
        case PacketType::IMIX:   return "IMIX";
    }
    return "UNKNOWN";
}

std::string ToString(Protocol p) {
    return p == Protocol::TCP ? "TCP" : "UDP";
}

Protocol GetProtocolFromString(const std::string& string) {
    std::string upperCase = MakeUpperCase(string);
    if (upperCase == "TCP") return Protocol::TCP;
    if (upperCase == "UDP") return Protocol::UDP;
    throw std::invalid_argument("Unknown protocol: " + string);
}

PacketType GetPacketTypeFromString(const std::string& string) {
    std::string upperCase = MakeUpperCase(string);
    if (upperCase == "SMALL")  return PacketType::SMALL;
    if (upperCase == "MEDIUM") return PacketType::MEDIUM;
    if (upperCase == "LARGE")  return PacketType::LARGE;
    if (upperCase == "IMIX")   return PacketType::IMIX;
    throw std::invalid_argument("Unknown packet size: " + string);
}

PacketSequenceGenerator::PacketSequenceGenerator(PacketType mode) : mode_(mode) {}

const std::vector<size_t>& PacketSequenceGenerator::IMIXPattern() {
    // 7 small : 4 medium : 1 large, a common simplified IMIX ratio (12 packets/cycle)
    static const std::vector<size_t> pattern = {
        PacketSpecs::SMALL_BYTES, PacketSpecs::SMALL_BYTES, PacketSpecs::SMALL_BYTES,
        PacketSpecs::SMALL_BYTES, PacketSpecs::SMALL_BYTES, PacketSpecs::SMALL_BYTES,
        PacketSpecs::SMALL_BYTES,
        PacketSpecs::MEDIUM_BYTES, PacketSpecs::MEDIUM_BYTES,
        PacketSpecs::MEDIUM_BYTES, PacketSpecs::MEDIUM_BYTES,
        PacketSpecs::LARGE_BYTES
    };
    return pattern;
}

std::vector<size_t> PacketSequenceGenerator::GeneratePacketType(size_t count) const {
    std::vector<size_t> out;
    out.reserve(count);

    if (mode_ != PacketType::IMIX) {
        size_t fixedSize = PacketSpecs::GetPacketType(mode_);
        for (size_t i = 0; i < count; ++i) out.push_back(fixedSize);
        return out;
    }

    const auto& pattern = IMIXPattern();
    for (size_t i = 0; i < count; ++i) {
        out.push_back(pattern[i % pattern.size()]);
    }
    return out;
}
