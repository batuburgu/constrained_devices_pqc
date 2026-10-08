#pragma once
#include "TestOrchestrator.hpp"
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
    TestConfig CreateConfigFromPrompts() const;
    //void Export2CSV(const Measurement& measurement) const;

    static Protocol     GetProtocolPrompt();
    static PacketType   GetPacketTypePrompt();
    static std::string  GetHostPrompt();
    static uint16_t     GetPortPrompt();
    static size_t       GetCountPrompt();
    static uint32_t     GetPPSPrompt();
};