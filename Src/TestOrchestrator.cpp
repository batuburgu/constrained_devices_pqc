    #include "TestOrchestrator.hpp"
    #include "ProtocolFactory.hpp"
    #include "Packet.hpp"
    #include <chrono>
    #include <iostream>
    #include <thread>
    #include <atomic>

    TestOrchestrator::TestOrchestrator(TestConfig config) 
        : config_(std::move(config)) {}

    static uint64_t GetTimeinNS() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
            .count();    
    }

    bool TestOrchestrator::Run() {
        auto protocol = ProtocolFactory::CreateProtocol(config_.protocol);
        if (!protocol->StartClient(config_.host, config_.port)) {
            std::cerr << "Failed to start client for " << protocol->Name() << "\n";
            return false;
        }

        PacketSequenceGenerator Generator(config_.packetType);
        std::vector<size_t> packetSizes2Send = Generator.GeneratePacketType(config_.packetCount);
        const uint64_t totalPackets = packetSizes2Send.size();
        
        std::cout << "Running " << config_.packetCount << " packets over " << protocol->Name()
                << " (" << PacketSpecs::GetName(config_.packetType) << ")...\n";

        measurement_.Reset(packetSizes2Send.size());
        measurement_.MarkStart();
        if (protocol->Name() == std::string("TCP")) {
            // TCP Receiver Thread
            std::thread receiver([&] {
                for (uint64_t i = 0; i < totalPackets; ++i) {
                    std::vector<uint8_t> raw;
                    if (!protocol->Receive(raw, config_.timeoutMs)) {
                        // A timeout or close means the rest are lost too
                        for (uint64_t j = i; j < totalPackets; ++j)
                            measurement_.RecordLost(j, packetSizes2Send[j]);
                        return;
                    }
                    uint64_t recvTime = GetTimeinNS();

                    Packet echoed = Packet::Deserialize(raw);
                    uint64_t seq = echoed.header.sequenceId;

                    // Use the timestamps stored in the header for calculating RTT
                    double rttMs = static_cast<double>(recvTime - echoed.header.timeSentNs) / 1e6;
                    measurement_.RecordReceived(seq, packetSizes2Send[seq], rttMs);
                }
            });

            // TCP Sender Thread
            for (uint64_t seq = 0; seq < packetSizes2Send.size(); ++seq) {
                Packet pkt = Packet::CreatePacket(seq, 0, packetSizes2Send[seq]); 

                uint64_t timeSent = GetTimeinNS();
                pkt.header.timeSentNs = timeSent;

                if (!protocol->Send(pkt.Serialize())) {
                    std::cerr << "  seq " << seq << ": send failed\n";
                    break;   
                }

            }
            receiver.join();
        }
        else if (protocol->Name() == std::string("UDP")) {
            std::vector<uint8_t> returnedPackages(totalPackets, 0);   // uint8_t, not vector<bool>: written by receiver, read after join
            std::atomic<bool> sendDone{false};

            //std::atomic<uint64_t> lastRecvNs{0};

            using clock = std::chrono::steady_clock;
            const uint32_t pps = config_.packetsPerSecond;
            const auto interval = pps > 0 ? std::chrono::nanoseconds(static_cast<int64_t>(1e9 / pps)) : std::chrono::nanoseconds(0);
            auto nextSend = clock::now();

            // UDP Receiver Thread
            std::thread receiver([&] {
                uint64_t received = 0;
                while (received < totalPackets) {
                    std::vector<uint8_t> raw;
                    if (!protocol->Receive(raw, config_.timeoutMs)) {
                        if (sendDone.load()) break;
                        continue;
                    }
                    uint64_t recvTime = GetTimeinNS();

                    if (raw.size() < sizeof(PacketHeader)) continue;   // Garbage packet
                    Packet echoed = Packet::Deserialize(raw);
                    uint64_t seq = echoed.header.sequenceId;

                    if (seq >= totalPackets || returnedPackages[seq]) continue;            // Invalid packet
                    returnedPackages[seq] = 1;
                    ++received;
                    //lastRecvNs.store(recvTime);

                    double rttMs = static_cast<double>(recvTime - echoed.header.timeSentNs) / 1e6;
                    measurement_.RecordReceived(seq, packetSizes2Send[seq], rttMs);
                }
            });

            // UDP Sender Thread
            for (uint64_t seq = 0; seq < totalPackets; ++seq) {
                Packet pkt = Packet::CreatePacket(seq, 0, packetSizes2Send[seq]);
                if (pps > 0) {
                    nextSend += interval;
                    auto currentTime = clock::now();
                    if (currentTime > nextSend + std::chrono::milliseconds(10))
                        nextSend = currentTime; // App fell far behind, send packages immediatelly
                    if (nextSend - currentTime > std::chrono::microseconds(300))
                        std::this_thread::sleep_until(nextSend - std::chrono::microseconds(200)); // Sleep in the leftover time
                    while(clock::now() < nextSend) { }
                }

                pkt.header.timeSentNs = GetTimeinNS();
                if (!protocol->Send(pkt.Serialize())) {
                    std::cerr << "  seq " << seq << ": send failed\n";
                }
            }
            sendDone = true;

            receiver.join();

            // Everything that never came back is lost (single thread now, no race)
            for (uint64_t idx = 0; idx < totalPackets; ++idx)
                if (!returnedPackages[idx]) measurement_.RecordLost(idx, packetSizes2Send[idx]);
        }
        measurement_.MarkEnd(); 
        protocol->Close();
        return true;
    }