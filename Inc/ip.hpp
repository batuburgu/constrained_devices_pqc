#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <chrono>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

class IProtocol {
public:
    virtual ~IProtocol() = default;

    virtual bool StartServer(uint16_t port) = 0;
    virtual bool StartClient(const std::string& host, uint16_t port) = 0;
    virtual bool Send(const std::vector<uint8_t>& data) = 0;
    virtual bool Receive(std::vector<uint8_t>& outData, int timeoutMs = 0) = 0;
    virtual bool AcceptClient() = 0;
    virtual void Close() = 0;
    virtual std::string Name() const = 0;
    virtual ssize_t ReceiveFrom(std::vector<uint8_t>& outData, sockaddr_in& peer, int timeoutMs = 0) {
        peer = {};
        return Receive(outData, timeoutMs) ? static_cast<ssize_t>(outData.size()) : -1;
    }
    
    virtual bool SendTo(const std::vector<uint8_t>& data, const sockaddr_in& peer) {
        (void)peer;
        return Send(data);
    }
};