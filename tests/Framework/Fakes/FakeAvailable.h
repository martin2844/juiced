#pragma once
// A stand-in of GameSpy's available check (<game>.available.gamespy.com) for the online tests.

#include "Core/Network/GameSpy/GameSpy.h"
#include "Core/Network/NetTcp.h"

#include <cstdint>

namespace oj::test {

// A stand-in of <game>.available.gamespy.com on the loopback for the tests only: the service hosts point the
// available check at it while it lives (the real name's lookup waits on the system's DNS, ~11 s where it times out),
// and answer() gives each query (0x09, four bytes, the game's name) the answer "available" (0xFE 0xFD 0x09 and a
// status of 0). The hosts as they were once it goes.
class FakeAvailable {
public:
    FakeAvailable() : m_kept(net::serviceHosts()) {
        m_open = m_socket.open();
        net::ServiceHosts& hosts = net::serviceHosts();
        hosts.available = "127.0.0.1";
        const uint16_t port = m_socket.localPort();  // network order
        hosts.availablePort = static_cast<uint16_t>((port >> 8) | (port << 8));
    }
    ~FakeAvailable() { net::serviceHosts() = m_kept; }
    FakeAvailable(const FakeAvailable&) = delete;
    FakeAvailable& operator=(const FakeAvailable&) = delete;

    bool open() const { return m_open; }
    // A query come since the last call answered; call it as the client's frames go.
    void answer() {
        uint8_t query[64];
        uint32_t ip = 0;
        uint16_t port = 0;
        const int got = m_socket.receive(query, sizeof query, ip, port);
        if (got < 5 || query[0] != 0x09) return;
        m_asked = true;
        const uint8_t available[7] = {0xFE, 0xFD, 0x09, 0, 0, 0, 0};
        m_socket.sendTo(ip, port, available, sizeof available);
    }
    // A query came.
    bool asked() const { return m_asked; }

private:
    net::ServiceHosts m_kept;
    net::UdpSocket m_socket;
    bool m_open = false;
    bool m_asked = false;
};

}  // namespace oj::test
