#include "udp.hpp"
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>

UDProtocol::UDProtocol() = default;
UDProtocol::~UDProtocol() { Close(); }

bool UDProtocol::StartServer(uint16_t port) {
    isServer_ = true;
    sockFd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockFd_ < 0) {
        std::cerr << "[UDP] socket() failed: " << std::strerror(errno) << "\n";
        return false;
    }

    int opt = 1;
    setsockopt(sockFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    int kernelQueueSize = 16 * 1024 * 1024;
    setsockopt(sockFd_, SOL_SOCKET, SO_RCVBUF, &kernelQueueSize, sizeof(kernelQueueSize));
    setsockopt(sockFd_, SOL_SOCKET, SO_SNDBUF, &kernelQueueSize, sizeof(kernelQueueSize));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sockFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[UDP] bind() failed: " << std::strerror(errno) << "\n";
        return false;
    }
    std::cout << "[UDP] Bound on port " << port << ", waiting for datagrams...\n";
    return true;
}

bool UDProtocol::AcceptClient() {
    // UDP is connectionless: nothing to accept, ready to receive right after bind.
    return true;
}

bool UDProtocol::StartClient(const std::string& host, uint16_t port) {
    isServer_ = false;
    sockFd_ = socket(AF_INET, SOCK_DGRAM, 0);    
    if (sockFd_ < 0) {
        std::cerr << "[UDP] socket() failed: " << std::strerror(errno) << "\n";
        return false;
    }

    int kernelQueueSize = 16 * 1024 * 1024;
    setsockopt(sockFd_, SOL_SOCKET, SO_RCVBUF, &kernelQueueSize, sizeof(kernelQueueSize));
    setsockopt(sockFd_, SOL_SOCKET, SO_SNDBUF, &kernelQueueSize, sizeof(kernelQueueSize));

    peerAddr_.sin_family = AF_INET;
    peerAddr_.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &peerAddr_.sin_addr) <= 0) {
        std::cerr << "[UDP] Invalid address: " << host << "\n";
        return false;
    }
    std::cout << "[UDP] Target set to " << host << ":" << port << "\n";
    return true;
}

bool UDProtocol::Send(const std::vector<uint8_t>& data) {
    if (sockFd_ < 0) return false;

    const sockaddr_in* dest = isServer_ ? &lastSender_ : &peerAddr_;
    if (isServer_ && !haveLastSender_) return false; // no client to reply to yet

    ssize_t n = sendto(sockFd_, data.data(), data.size(), 0,
                        reinterpret_cast<const sockaddr*>(dest), sizeof(*dest));
    return n == static_cast<ssize_t>(data.size());
}

bool UDProtocol::Receive(std::vector<uint8_t>& outData, int timeoutMs) {
    if (sockFd_ < 0) return false;

    if (timeoutMs > 0) {
        pollfd pfd{sockFd_, POLLIN, 0};
        int rc = poll(&pfd, 1, timeoutMs);
        if (rc <= 0) return false; // timeout or error
    }

    sockaddr_in fromAddr{};
    socklen_t fromLen = sizeof(fromAddr);
    ssize_t packetSize = recvfrom(sockFd_, UDProtocol::rxBuff.data(), UDProtocol::rxBuff.size(), 0,
                          reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
    if (packetSize <= 0) return false;
    outData.assign(UDProtocol::rxBuff.begin(), UDProtocol::rxBuff.begin() + packetSize);
    if (isServer_) {
        lastSender_ = fromAddr;
        haveLastSender_ = true;
    }
    return true;
}

ssize_t UDProtocol::ReceiveFrom(std::vector<uint8_t>& outData, sockaddr_in& peer, int timeoutMs) {
    if (sockFd_ < 0) return -1;

    if (timeoutMs > 0) {
        pollfd pfd{sockFd_, POLLIN, 0};
        if (poll(&pfd, 1, timeoutMs) <= 0) return -1;
    }

    socklen_t len = sizeof(peer);
    ssize_t n = recvfrom(sockFd_, rxBuff.data(), rxBuff.size(), 0,
                         reinterpret_cast<sockaddr*>(&peer), &len);
    if (n < 0) return -1;
    outData.assign(rxBuff.begin(), rxBuff.begin() + n);
    return n;
}

bool UDProtocol::SendTo(const std::vector<uint8_t>& data, const sockaddr_in& peer) {
    if (sockFd_ < 0) return false;
    ssize_t n = sendto(sockFd_, data.data(), data.size(), 0,
                       reinterpret_cast<const sockaddr*>(&peer), sizeof(peer));
    return n == static_cast<ssize_t>(data.size());
}

void UDProtocol::Close() {
    if (sockFd_ >= 0) { ::close(sockFd_); sockFd_ = -1; }
}