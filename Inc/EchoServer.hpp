#pragma once
#include <cstdint>
#include "PacketTypes.hpp"

// ---------------------------------------------------------------------------
// EchoServer is the counterpart run on the other machine (e.g. the Pi):
// it accepts/binds on a port for a given protocol and echoes every packet
// straight back to whoever sent it, so the client side (TestOrchestrator)
// can measure round-trip latency. It's intentionally "dumb": all the
// interesting measurement logic lives client-side.
// ---------------------------------------------------------------------------

#define LOG_PER_PACKET      100000
#define TCP_QUEUE_SIZE      512
#define UDP_QUEUE_SIZE      65536
class EchoServer {
public:
    EchoServer(Protocol protocol, uint16_t port);

    // Blocks forever, echoing packets. Ctrl+C to stop.
    void Run();

private:
    Protocol protocol_;
    uint16_t port_;
};
