#pragma once
#include "IProtocol.hpp"
#include <netinet/in.h>

class UDProtocol : public IProtocol {
public:
    UDProtocol();
    ~UDProtocol() override;

    bool StartServer(uint16_t port) override;
    bool StartClient(const std::string& host, uint16_t port) override;
    bool Send(const std::vector<uint8_t>& data) override;
    bool Receive(std::vector<uint8_t>& outData, int timeoutMs = 0) override;
    ssize_t ReceiveFrom(std::vector<uint8_t>& outData, sockaddr_in& peer, int timeoutMs = 0) override;
    bool SendTo(const std::vector<uint8_t>& data, const sockaddr_in& peer) override;
    bool AcceptClient() override; // no-op for UDP, always returns true
    void Close() override;
    std::string Name() const override { return "UDP"; }

private:
    int sockFd_ = -1;
    bool isServer_ = false;

    std::vector<uint8_t> rxBuff = std::vector<uint8_t>(65536);
    sockaddr_in peerAddr_{};   // client-side: server we send to
    sockaddr_in lastSender_{}; // server-side: most recent sender, used to reply
    bool haveLastSender_ = false;
};
