#include "TCProtocol.hpp"
#include "Packet.hpp"
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

TCProtocol::TCProtocol() = default;

TCProtocol::~TCProtocol() { Close(); }

bool TCProtocol::StartServer(uint16_t port) {
    isServer_ = true;
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        std::cerr << "[TCP] socket() failed: " << std::strerror(errno) << "\n";
        return false;
    }

    int opt = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    int kernelQueueSize = 64*1024; // 64 KB'den fazla olursa SMALL 120 ms ustune cikiyor
    setsockopt(listenFd_, SOL_SOCKET, SO_SNDBUF, &kernelQueueSize, sizeof(kernelQueueSize));
    setsockopt(listenFd_, SOL_SOCKET, SO_RCVBUF, &kernelQueueSize, sizeof(kernelQueueSize));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[TCP] bind() failed: " << std::strerror(errno) << "\n";
        return false;
    }
    if (listen(listenFd_, 1) < 0) {
        std::cerr << "[TCP] listen() failed: " << std::strerror(errno) << "\n";
        return false;
    }
    std::cout << "[TCP] Listening on port " << port << "...\n";
    return true;
}

bool TCProtocol::AcceptClient() {
    if (!isServer_ || listenFd_ < 0) return false;

    sockaddr_in clientAddr{};
    socklen_t len = sizeof(clientAddr);
    connFd_ = accept(listenFd_, reinterpret_cast<sockaddr*>(&clientAddr), &len);
    if (connFd_ < 0) {
        std::cerr << "[TCP] accept() failed: " << std::strerror(errno) << "\n";
        return false;
    }

    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));
    std::cout << "[TCP] Client connected from " << ipStr << ":" << ntohs(clientAddr.sin_port) << "\n";

    int flag = 1;
    setsockopt(connFd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)); // matters for latency tests
    return true;
}

bool TCProtocol::StartClient(const std::string& host, uint16_t port) {
    isServer_ = false;
    connFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (connFd_ < 0) {
        std::cerr << "[TCP] socket() failed: " << std::strerror(errno) << "\n";
        return false;
    }

    int flag = 1;
    setsockopt(connFd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    int kernelQueueSize = 256*1024;
    setsockopt(connFd_, SOL_SOCKET, SO_SNDBUF, &kernelQueueSize, sizeof(kernelQueueSize));
    setsockopt(connFd_, SOL_SOCKET, SO_RCVBUF, &kernelQueueSize, sizeof(kernelQueueSize));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "[TCP] Invalid address: " << host << "\n";
        return false;
    }

    if (connect(connFd_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "[TCP] connect() failed: " << std::strerror(errno) << "\n";
        return false;
    }
    std::cout << "[TCP] Connected to " << host << ":" << port << "\n";
    return true;
}

bool TCProtocol::Send(const std::vector<uint8_t>& data) {
    if (connFd_ < 0) return false;

    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = ::send(connFd_, data.data() + sent, data.size() - sent, 0);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool TCProtocol::ReadExact(int fd, void* buf, size_t len, int timeoutMs) {
    size_t got = 0;
    auto* bytes = static_cast<uint8_t*>(buf);
    while (got < len) {
        if (timeoutMs > 0) {
            pollfd pfd{fd, POLLIN, 0};
            int rc = poll(&pfd, 1, timeoutMs);
            if (rc <= 0) return false; // timeout or error
        }
        ssize_t n = ::recv(fd, bytes + got, len - got, 0);
        if (n <= 0) return false; // closed or error
        got += static_cast<size_t>(n);
    }
    return true;
}

bool TCProtocol::Receive(std::vector<uint8_t>& outData, int timeoutMs) {
    if (connFd_ < 0) return false;

    PacketHeader header{};
    if (!ReadExact(connFd_, &header, sizeof(header), timeoutMs)) return false;

    constexpr size_t kMaxPayload = 16 * 1024 * 1024; 
    if (header.payloadSize > kMaxPayload) return false;

    outData.resize(sizeof(header) + header.payloadSize);
    std::memcpy(outData.data(), &header, sizeof(header));
    if (header.payloadSize > 0) {
        if (!ReadExact(connFd_, outData.data() + sizeof(header), header.payloadSize, timeoutMs))
            return false;
    }
    return true;
}

void TCProtocol::Close() {
    if (connFd_ >= 0) { ::close(connFd_); connFd_ = -1; }
    if (listenFd_ >= 0) { ::close(listenFd_); listenFd_ = -1; }
}