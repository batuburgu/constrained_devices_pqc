#pragma once
#include <cstdint>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <algorithm>

#pragma pack(push, 1)
struct PacketHeader {
    uint64_t sequenceId   = 0;
    uint64_t timeSentNs   = 0; // sender's monotonic clock at send time
    uint32_t payloadSize  = 0;
    uint8_t  flags        = 0; // reserved (e.g. bit 0 = "payload is encrypted")
};
#pragma pack(pop)

struct Packet {
    PacketHeader header;
    std::vector<uint8_t> payload;

    std::vector<uint8_t> Serialize() const {
        std::vector<uint8_t> buf(sizeof(PacketHeader) + payload.size());
        std::memcpy(buf.data(), &header, sizeof(PacketHeader));
        if (!payload.empty()) {
            std::memcpy(buf.data() + sizeof(PacketHeader), payload.data(), payload.size());
        }
        return buf;
    }

    // Parses a raw buffer (as produced by serialize()) back into a Packet.
    static Packet Deserialize(const std::vector<uint8_t>& buf) {
        Packet p;
        if (buf.size() < sizeof(PacketHeader)) {
            throw std::runtime_error("Buffer too small to contain a PacketHeader");
        }
        std::memcpy(&p.header, buf.data(), sizeof(PacketHeader));
        size_t available = buf.size() - sizeof(PacketHeader);
        size_t toCopy = std::min<size_t>(available, p.header.payloadSize);
        p.payload.assign(buf.begin() + sizeof(PacketHeader), buf.begin() + sizeof(PacketHeader) + toCopy);
        return p;
    }

    static Packet CreatePacket(uint64_t sequenceId, uint64_t timeSentNS, size_t payloadBytes) {
        Packet p;
        p.header.sequenceId = sequenceId;
        p.header.timeSentNs = timeSentNS;
        p.header.payloadSize = static_cast<uint32_t>(payloadBytes);
        p.payload.assign(payloadBytes, 0xBA); // deterministic filler, easy to spot in a hex dump
        return p;
    }

    size_t WireSize() const { return sizeof(PacketHeader) + payload.size(); }
};