#pragma once
#include <memory>
#include "ip.hpp"
#include "tcp.hpp"
#include "udp.hpp"
#include "packet_types.hpp"

class ProtocolFactory {
public:
    static std::unique_ptr<IProtocol> CreateProtocol(Protocol protocol) {
        switch (protocol) {
            case Protocol::TCP: return std::make_unique<TCProtocol>();
            case Protocol::UDP: return std::make_unique<UDProtocol>();
        }
        return nullptr;
    }
};