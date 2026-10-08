#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Packet size categories used by every test scenario.
// IMIX = a mixed sequence of sizes, modeled after the classic Internet Mix
// used in networking benchmarks (mostly small packets, some medium, a few
// large -> approximates real-world traffic better than a single fixed size).
// ---------------------------------------------------------------------------
enum class PacketType {
    SMALL,
    MEDIUM,
    LARGE,
    IMIX
};

enum class Protocol {
    TCP,
    UDP
};

struct PacketSpecs {
    static constexpr size_t SMALL_BYTES  = 256;
    static constexpr size_t MEDIUM_BYTES = 512;
    static constexpr size_t LARGE_BYTES  = 1448;

    static size_t GetPacketType(PacketType size);
    static std::string GetName(PacketType size);
};

std::string ToString(Protocol p);
Protocol GetProtocolFromString(const std::string& s);
PacketType GetPacketTypeFromString(const std::string& s);

// Produces the ordered list of payload sizes to send for one test run.
// For SMALL/MEDIUM/LARGE it's just the same size repeated `count` times.
// For IMIX it cycles a weighted pattern (7 small : 4 medium : 1 large,
// the common simplified IMIX ratio) so the mix is deterministic and
// reproducible between runs.
class PacketSequenceGenerator {
public:
    explicit PacketSequenceGenerator(PacketType mode);

    std::vector<size_t> GeneratePacketType(size_t count) const;

private:
    PacketType mode_;
    static const std::vector<size_t>& IMIXPattern();
};
