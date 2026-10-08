#pragma once
#include <memory>
#include "IProtocol.hpp"
#include "TCProtocol.hpp"
#include "UDProtocol.hpp"
#include "PacketTypes.hpp"

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