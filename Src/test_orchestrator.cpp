    #include "test_orchestrator.hpp"
    #include "protocol_factory.hpp"
    #include "measurement.hpp"
    #include "kem.hpp"
    #include "frame.hpp"
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
        auto protocol = ProtocolFactory::CreateProtocol(config_.protocol.value());
        if (!protocol->StartClient(config_.host.value(), config_.port.value())) {
            std::cerr << "Failed to start client for " << protocol->Name() << "\n";
            return false;
        }

        if (ECHO_REQ()) {
            PacketSequenceGenerator Generator(config_.packetType.value());
            std::vector<size_t> packetSizes2Send = Generator.GeneratePacketType(config_.packetCount.value());
            const uint64_t totalPackets = packetSizes2Send.size();
            
            std::cout << "Running " << config_.packetCount.value() << " packets over " << protocol->Name()
                    << " (" << PacketSpecs::GetName(config_.packetType.value()) << ")...\n";

            measurement_.Reset(packetSizes2Send.size(), MT_ECHO);
            measurement_.MarkStart();
            if (protocol->Name() == std::string("TCP")) {
                // TCP Receiver Thread
                std::thread receiver([&] {
                    for (uint64_t i = 0; i < totalPackets; ++i) {
                        std::vector<uint8_t> raw;
                        if (!protocol->Receive(raw, config_.timeoutMs.value())) {
                            // A timeout or close means the rest are lost too
                            for (uint64_t j = i; j < totalPackets; ++j)
                                measurement_.Echo_RecordLost(j, packetSizes2Send[j]);
                            return;
                        }
                        uint64_t recvTime = GetTimeinNS();

                        frame::Frame incomingFrame;
                        if (!frame::ParsePacket(raw, incomingFrame) || incomingFrame.header.sequenceId >= totalPackets) {
                            for (uint64_t j = i; j < totalPackets; ++j)
                                measurement_.Echo_RecordLost(j, packetSizes2Send[j]);
                            return;   
                        }
                        double rttMs = static_cast<double>(recvTime - incomingFrame.header.time) / 1e6;
                        measurement_.Echo_RecordReceived(incomingFrame.header.sequenceId, packetSizes2Send[incomingFrame.header.sequenceId], rttMs);
                    }
                });

                // TCP Sender Thread
                for (uint64_t seq = 0; seq < packetSizes2Send.size(); ++seq) { 
                    size_t packetSize = packetSizes2Send[seq];
                    std::vector<uint8_t> fakePayload(packetSize, 0xBA);// Generate fake payload

                    std::vector<uint8_t> wireBytes = frame::BuildPacket(frame::Type::Echo, fakePayload.data(), packetSize, seq);
                    frame::StampTime(wireBytes, GetTimeinNS());
                    if (!protocol->Send(wireBytes)) {
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
                const uint32_t pps = config_.packetsPerSecond.value();
                const auto interval = pps > 0 ? std::chrono::nanoseconds(static_cast<int64_t>(1e9 / pps)) : std::chrono::nanoseconds(0);
                auto nextSend = clock::now();

                // UDP Receiver Thread
                std::thread receiver([&] {
                    uint64_t received = 0;
                    while (received < totalPackets) {
                        std::vector<uint8_t> raw;
                        if (!protocol->Receive(raw, config_.timeoutMs.value())) {
                            if (sendDone.load()) break;
                            continue;
                        }
                        uint64_t recvTime = GetTimeinNS();

                        frame::Frame incomingFrame;
                        if (!frame::ParsePacket(raw, incomingFrame)) continue;   // garbage datagram
                        uint64_t seq = incomingFrame.header.sequenceId;
                        if (seq >= totalPackets || returnedPackages[seq]) continue;
                        returnedPackages[seq] = 1;
                        ++received;

                        double rttMs = static_cast<double>(recvTime - incomingFrame.header.time) / 1e6;
                        measurement_.Echo_RecordReceived(seq, packetSizes2Send[seq], rttMs);
                    }
                });

                // UDP Sender Thread
                for (uint64_t seq = 0; seq < totalPackets; ++seq) {
                    size_t packetSize = packetSizes2Send[seq];
                    std::vector<uint8_t> fakePayload(packetSize, 0xBA); // Generate fake payload
                    std::vector<uint8_t> wireBytes = frame::BuildPacket(frame::Type::Echo, fakePayload.data(), packetSize, seq);
                    
                    if (pps > 0) {
                        nextSend += interval;
                        auto currentTime = clock::now();
                        if (currentTime > nextSend + std::chrono::milliseconds(10))
                            nextSend = currentTime; // App fell far behind, send packages immediatelly
                        if (nextSend - currentTime > std::chrono::microseconds(300))
                            std::this_thread::sleep_until(nextSend - std::chrono::microseconds(200)); // Sleep in the leftover time
                        while(clock::now() < nextSend) { }
                    }
                    frame::StampTime(wireBytes, GetTimeinNS());
                    if (!protocol->Send(wireBytes)) {
                        std::cerr << "  seq " << seq << ": send failed\n";
                    }
                }            
                sendDone = true;

                receiver.join();

                // Everything that never came back is lost (single thread now, no race)
                for (uint64_t idx = 0; idx < totalPackets; ++idx)
                    if (!returnedPackages[idx]) measurement_.Echo_RecordLost(idx, packetSizes2Send[idx]);
            }
        }

        else if (KEM_REQ()) {
            std::string kemAlgorithm = config_.kemAlgorithm.value();
            pqc::KEM kem(kemAlgorithm);
            pqc::KEM::KeyPair keyPair;
            std::vector<uint8_t> rawBytes;
            frame::Frame respFrame;
            measurement_.Reset(config_.excRepeatNumber.value(), MT_KEM);
            measurement_.MarkStart();
            for (uint64_t seq = 0; seq < config_.excRepeatNumber.value(); ++seq) {
                /* Key Generation */
                const uint64_t keyGenStart = GetTimeinNS();
                keyPair = kem.GenerateKeyPair();
                const uint64_t keyGenEnd = GetTimeinNS();
                const double keyGenTimeUs = static_cast<double>(keyGenEnd - keyGenStart) / 1e3;

                /* Generate KEM Request Package */
                // AlgorithmLen | Algorithm | Public Key
                std::vector<uint8_t> payload(1 + kemAlgorithm.size() + keyPair.publicKey.size());
                payload.push_back(static_cast<uint8_t>(kemAlgorithm.size()));
                payload.insert(payload.end(), kemAlgorithm.begin(), kemAlgorithm.end());
                payload.insert(payload.end(), keyPair.publicKey.begin(), keyPair.publicKey.end());

                /* Send Public Key */
                std::vector<uint8_t> wireBytes = frame::BuildPacket(frame::Type::KeyExchangeRequest, payload.data(), payload.size(), seq);
                
                const uint64_t sendTimeNs = GetTimeinNS();
                frame::StampTime(wireBytes, sendTimeNs);
                if (!protocol->Send(wireBytes)) {
                    std::cerr << "  seq " << seq << ": send failed\n";
                    break;
                }

                /* Receive Encapsulated Key */
                // Server sends the encapsulation latency inside the header.time
                if(!protocol->Receive(rawBytes, config_.timeoutMs.value())) {
                    measurement_.KEM_RecordLost(seq, keyGenTimeUs);
                    continue;
                }
                frame::ParsePacket(rawBytes, respFrame);
                if ((respFrame.header.type != frame::Type::KeyExchangeResponse) || (respFrame.header.sequenceId != seq)) {
                    measurement_.KEM_RecordLost(seq, keyGenTimeUs);
                    continue;
                }

                /* Decapsulate Package, Extract Shared Key */
                const uint64_t serverEncapsTime = respFrame.header.time;
                const uint64_t decapsStart = GetTimeinNS();
                pqc::SecretBuffer sharedKey = kem.Decapsulate(respFrame.payload, keyPair.privateKey);
                const uint64_t decapsEnd = GetTimeinNS();

                const double decapsTimeUs = static_cast<double>(decapsEnd - decapsStart) / 1e3;
                const double serverEncapsTimeUs = static_cast<double>(serverEncapsTime) / 1e3;
                const double totalTimeMs = static_cast<double>(decapsEnd - keyGenStart) / 1e6;
                const double networkMs = totalTimeMs - ((decapsTimeUs + serverEncapsTimeUs + keyGenTimeUs) /1e3); 

                measurement_.KEM_RecordReceived(seq, keyGenTimeUs, serverEncapsTime, decapsTimeUs, totalTimeMs, networkMs);
            }
        }

        measurement_.MarkEnd(); 
        protocol->Close();
        return true;
    }