#include "EchoServer.hpp"
#include "ProtocolFactory.hpp"
#include "UDProtocol.hpp"
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

EchoServer::EchoServer(Protocol protocol, uint16_t port) : protocol_(protocol), port_(port) {}

void EchoServer::Run() {
    auto protocol = ProtocolFactory::CreateProtocol(protocol_);
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

            readMutexHandler.lock();
            queueNotFull_CV.wait(readMutexHandler, [&] { return queue.size() < tcpQueueSize; });

            queue.push_back(std::move(msg));
            readMutexHandler.unlock();
            queueNotEmpty_CV.notify_one();
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

        // Not reached while the loop runs forever (Ctrl+C ends the process). For a future stop flag:
        // readMutexHandler.lock();
        // done = true;
        // readMutexHandler.unlock();
        // queueNotEmpty_CV.notify_one();
        // echo.join();
        // std::cout << "Echoed " << count << " packets, dropped " << drops << "\n";
        // protocol->Close();
    }


    // Not reached while the loop runs forever (Ctrl+C ends the process), kept for a future stop flag:
    // { std::lock_guard<std::mutex> lk(queueMutex); done = true; }
    // queueNotEmpty_CV.notify_one();
    // echo.join();
}