#include "echo_server.hpp"
#include "packet_types.hpp"
#include <iostream>
#include <cstdlib>

// Usage: nettest_server <tcp|udp> <port>
int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <tcp|udp> <port>\n";
        return 1;
    }
    try {
        Protocol protocol = GetProtocolFromString(argv[1]);
        uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));
        EchoServer server(protocol, port);
        server.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
