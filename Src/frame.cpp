#include "frame.hpp"
#include <cstring>
#include <stdexcept>

namespace frame {
    void FillHeader(uint8_t* dst, Type type, size_t len, uint64_t sequenceId) {
        if (len > kMaxPayload) throw std::length_error("frame payload too large");

        dst[0] = static_cast<uint8_t>(type);
        wire::PutU32((dst + lengthOffset), static_cast<uint32_t>(len));
        wire::PutU64((dst + sequenceIdOffset), sequenceId);
        return;
    }

    std::vector<uint8_t> BuildPacket(Type type, const uint8_t* data, size_t len, uint64_t sequenceId) {
        std::vector<uint8_t> out(headerSize + len);

        FillHeader(out.data(), type, len, sequenceId);
        
        if (len > 0) std::memcpy((out.data() + headerSize), data, len);
        return out;
    }    

    bool ParseHeader(const uint8_t* bytes, Header& out) {
        const uint32_t length = wire::GetU32(bytes + 1);
        if (length > kMaxPayload) return false;

        out.type = static_cast<Type>(bytes[0]);
        out.length = length;
        out.sequenceId = wire::GetU64(bytes + sequenceIdOffset);
        out.time = wire::GetU64(bytes + timeStampOffset);
        return true;
    }

    bool ParsePacket(const std::vector<uint8_t>& bytes, Frame& out) {
        if (bytes.size() < headerSize) return false;
        Header header;

        if (!ParseHeader(bytes.data(), header)) return false;
        if (bytes.size() != headerSize + header.length) return false;

        out.header = header;
        out.payload.assign(bytes.begin() + headerSize, bytes.end());

        return true;
    }    
    void StampTime(std::vector<uint8_t>& encodedFrame, uint64_t timeSentNs) {
        wire::PutU64(encodedFrame.data() + timeStampOffset, timeSentNs);
    }
}
