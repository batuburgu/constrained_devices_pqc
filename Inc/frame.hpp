#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace wire {  
    inline void PutU16(uint8_t* p, uint16_t v) {
        p[0] = static_cast<uint8_t>(v >> 8);
        p[1] = static_cast<uint8_t>(v);
    }
    
    inline void PutU32(uint8_t* p, uint32_t v) {
        for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (24 - 8 * i));
    }
    
    inline void PutU64(uint8_t* p, uint64_t v) {
        for (int i = 0; i < 8; ++i) p[i] = static_cast<uint8_t>(v >> (56 - 8 * i));
    }
    
    inline uint16_t GetU16(const uint8_t* p) { 
        return static_cast<uint16_t>((p[0] << 8) | p[1]); 
    }
    
    inline uint32_t GetU32(const uint8_t* p) {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v = (v << 8) | p[i];
        return v;
    }
    
    inline uint64_t GetU64(const uint8_t* p) {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
        return v;
    }
}  // namespace wire

namespace frame {
    enum class Type : uint8_t {
        Echo = 1,
        KeyExchangeRequest = 12,
        KeyExchangeResponse = 13,
        TypeEnd
    };
    
    struct Header {
        Type type;
        uint32_t length;
        uint64_t sequenceId;
        uint64_t time;
    };

    struct Frame {
        struct Header header;
        std::vector<uint8_t> payload;
    };



    constexpr size_t lengthOffset = 1;
    constexpr size_t sequenceIdOffset = 5;
    constexpr size_t timeStampOffset = 13;
    constexpr size_t headerSize = sizeof(Header);
    constexpr uint32_t kMaxPayload = 16u * 1024u * 1024u;    
    

    void FillHeader(uint8_t* dst, Type type, size_t len, uint64_t sequenceId = 0);
    std::vector<uint8_t> BuildPacket(Type type, const uint8_t* data, size_t len, uint64_t sequenceId = 0);
    bool ParseHeader(const uint8_t* bytes, Header& out);
    bool ParsePacket(const std::vector<uint8_t>& bytes, Frame& out);
    void StampTime(std::vector<uint8_t>& encodedFrame, uint64_t timeSentNs);
} // namespace frame