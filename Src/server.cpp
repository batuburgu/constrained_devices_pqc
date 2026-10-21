#include "server.hpp"
#include "protocol_factory.hpp"
#include "udp.hpp"
#include "frame.hpp"
#include "kem.hpp"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

struct Datagram {
    sockaddr_in peer;
    std::vector<uint8_t> data;
};

static uint64_t GetTimeinNS() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
        .count();    
}

Server::Server(Protocol protocol, uint16_t port) : protocol_(protocol), port_(port) {}

void Server::Run() {
    auto protocol = ProtocolFactory::CreateProtocol(protocol_);
    frame::Frame incomingFrame;
    if (!protocol->StartServer(port_)) {
        std::cerr << "Failed to start server on port " << port_ << "\n";
        return;
    }

    if (!protocol->AcceptClient()) {
        std::cerr << "Failed to accept client\n";
        return;
    }

    std::cout << "[" << protocol->Name() << "] Echoing packets. Ctrl+C to stop.\n";

    if (protocol->Name() == std::string("TCP")) {
        const size_t tcpQueueSize = TCP_QUEUE_SIZE;
        std::deque<std::vector<uint8_t>> queue;
        std::mutex recvEchoMutex;
        std::condition_variable queueNotEmpty_CV;
        std::condition_variable queueNotFull_CV;
        std::unique_lock<std::mutex> readMutexHandler(recvEchoMutex, std::defer_lock);
        bool done = false;
        uint64_t count = 0;
        
        // Echo Thread
        std::thread echo([&] {
            std::vector<uint8_t> msg;
            std::unique_lock<std::mutex> echoMutexHandler(recvEchoMutex, std::defer_lock);
            while (true) {
                echoMutexHandler.lock();
                queueNotEmpty_CV.wait(echoMutexHandler, [&] { return !queue.empty() || done; });
                
                if (queue.empty()) break;
                
                msg = std::move(queue.front());
                queue.pop_front();
                echoMutexHandler.unlock();
                queueNotFull_CV.notify_one();

                if (!protocol->Send(msg)) {
                    std::cerr << "Echo send failed on packet " << count << "\n";
                }
                if (++count % LOG_PER_PACKET == 0) std::cout << "  echoed " << count << " packets\n";
            }
        });

        // Reader Thread
        while (true) {
            std::vector<uint8_t> msg;
            if (!protocol->Receive(msg, /*timeoutMs=*/0)) break;
            
            frame::ParsePacket(msg, incomingFrame);

            if (incomingFrame.header.type == frame::Type::Echo) {
                readMutexHandler.lock();
                queueNotFull_CV.wait(readMutexHandler, [&] { return queue.size() < tcpQueueSize; });

                queue.push_back(std::move(msg));
                readMutexHandler.unlock();
                queueNotEmpty_CV.notify_one();
            }
            else if (incomingFrame.header.type == frame::Type::KeyExchangeRequest) {
                uint8_t algoNameSize = incomingFrame.payload[0];
                auto algoNameBeginning = incomingFrame.payload.begin();
                auto keyBegin = algoNameBeginning + static_cast<std::ptrdiff_t>(algoNameSize);

                const std::string kemAlgorithm(algoNameBeginning, keyBegin);
                const std::vector<uint8_t> publicKey(keyBegin, incomingFrame.payload.end());
                
                pqc::KEM kem(kemAlgorithm);
                const auto encapsStart = GetTimeinNS();
                auto encapsData = kem.Encapsulate(publicKey);
                const auto encapsEnd = GetTimeinNS();
                const auto elapsedEncapsTime = encapsEnd - encapsStart;
                std::vector<uint8_t> payload = std::move(encapsData.cipherText);
                std::vector<uint8_t> wireBytes = frame::BuildPacket(frame::Type::KeyExchangeResponse, payload.data(),payload.size(), incomingFrame.header.sequenceId);
                frame::StampTime(wireBytes, elapsedEncapsTime);
                if (!protocol->Send(wireBytes)) {
                    std::cerr <<" Send failed\n";
                    break;
                }                
            }
        }

        readMutexHandler.lock();
        done = true;
        readMutexHandler.unlock();
        queueNotEmpty_CV.notify_one();
        echo.join();

        std::cout << "Connection closed after " << count << " packets.\n";
        protocol->Close();
    }
    
    else if (protocol->Name() == std::string("UDP")) {
        const size_t udpQueueSize = UDP_QUEUE_SIZE;   // large on purpose: UDP can't push back
        std::deque<Datagram> queue;
        std::mutex recvEchoMutex;
        std::condition_variable queueNotEmpty_CV;
        std::unique_lock<std::mutex> readMutexHandler(recvEchoMutex, std::defer_lock);
        bool done = false;
        uint64_t drops = 0;                       // only touched by the reader thread
        uint64_t count = 0;                       // only touched by the echo thread

        // Echo Thread
        std::thread echo([&] {
            Datagram dg;
            std::unique_lock<std::mutex> echoMutexHandler(recvEchoMutex, std::defer_lock);
            while (true) {
                echoMutexHandler.lock();
                queueNotEmpty_CV.wait(echoMutexHandler, [&] { return !queue.empty() || done; });

                if (queue.empty()) break;

                dg = std::move(queue.front());
                queue.pop_front();
                echoMutexHandler.unlock();

                if (!protocol->SendTo(dg.data, dg.peer)) {
                    std::cerr << "Echo send failed on packet " << count << "\n";
                }
                if (++count % LOG_PER_PACKET == 0) std::cout << "  echoed " << count << " packets\n";
            }
        });

        // Reader Thread
        while (true) {
            Datagram dg{};
            if (protocol->ReceiveFrom(dg.data, dg.peer, /*timeoutMs=*/0) < 0) continue;

            frame::ParsePacket(dg.data, incomingFrame);
            
            if(incomingFrame.header.type == frame::Type::Echo) {
                readMutexHandler.lock();
                if (queue.size() >= udpQueueSize) {
                    readMutexHandler.unlock();
                    ++drops;                          
                    continue;
                }
                bool wasEmpty = queue.empty();
                queue.push_back(std::move(dg));
                readMutexHandler.unlock();

                if (wasEmpty) queueNotEmpty_CV.notify_one();   // only wake the echo thread when needed
            } 
            else if (incomingFrame.header.type == frame::Type::KeyExchangeRequest) {
                uint8_t algoNameSize = incomingFrame.payload[0];
                auto algoNameBeginning = incomingFrame.payload.begin();
                auto keyBegin = algoNameBeginning + static_cast<std::ptrdiff_t>(algoNameSize);

                const std::string kemAlgorithm(algoNameBeginning, keyBegin);
                const std::vector<uint8_t> publicKey(keyBegin, incomingFrame.payload.end());
                
                pqc::KEM kem(kemAlgorithm);
                const auto encapsStart = GetTimeinNS();
                auto encapsData = kem.Encapsulate(publicKey);
                const auto encapsEnd = GetTimeinNS();
                const auto elapsedEncapsTime = encapsEnd - encapsStart;
                std::vector<uint8_t> payload = std::move(encapsData.cipherText);
                std::vector<uint8_t> wireBytes = frame::BuildPacket(frame::Type::KeyExchangeResponse, payload.data(),payload.size(), incomingFrame.header.sequenceId);
                frame::StampTime(wireBytes, elapsedEncapsTime);
                if (!protocol->SendTo(wireBytes, dg.peer)) {
                    std::cerr <<" Send failed\n";
                    break;
                }                    
            }
        }

        // Not reached while the loop runs forever (Ctrl+C ends the process). For a future stop flag:
        // readMutexHandler.lock();
        // done = true;
        // readMutexHandler.unlock();
        // queueNotEmpty_CV.notify_one();
        // echo.join();
        // std::cout << "Echoed " << count << " packets, dropped " << drops << "\n";
        // protocol->Close();
    }
    
}