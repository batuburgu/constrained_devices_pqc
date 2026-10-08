#pragma once
#include "ip.hpp"

class TCProtocol : public IProtocol {
public:
    TCProtocol();
    ~TCProtocol() override;

    bool StartServer(uint16_t port) override;
    bool StartClient(const std::string& host, uint16_t port) override;
    bool Send(const std::vector<uint8_t>& data) override;
    bool Receive(std::vector<uint8_t>& outData, int timeoutMs = 0) override;
    bool AcceptClient() override;
    void Close() override;
    std::string Name() const override { return "TCP"; }

private:
    int listenFd_ = -1;   // server-side: listening socket
    int connFd_   = -1;   // active connection (server-side after accept, or client-side)
    bool isServer_ = false;

    bool ReadExact(int fd, void* buf, size_t len, int timeoutMs);
};
