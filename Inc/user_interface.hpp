#pragma once
#include "test_orchestrator.hpp"
// ---------------------------------------------------------------------------
// UserInterface owns the terminal menu: choosing protocol, packet size/IMIX,
// target, count, and (in the future) which cipher to use, then delegates
// the actual work to TestOrchestrator and prints/exports what Measurement
// computed. Keeping this separate means the menu can be swapped later
// (config file, argv flags, a TUI) without touching the test logic.
// ---------------------------------------------------------------------------
class UserInterface {
public:
    void Run(); // main interactive loop; returns when the user chooses to quit

private:
    struct Steps {
        bool kem = false;
        //bool sig = false;
        bool echo = false;
    };
    
    TestConfig CreateConfigFromPrompts() const;
    //void Export2CSV(const Measurement& measurement) const;

    static Protocol    GetProtocolPrompt();
    static Steps       GetStepsPrompt();
    static std::string GetKemPrompt();
    //static std::string GetSigPrompt();
    static size_t      GetRepetitionsPrompt();
    static PacketType  GetPacketTypePrompt();
    static std::string GetHostPrompt();
    static uint16_t    GetPortPrompt();
    static size_t      GetCountPrompt();
    static uint32_t    GetPPSPrompt();
};