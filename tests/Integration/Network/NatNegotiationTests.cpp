// Integration tests: NatNegotiation.

#include "Framework/Test.h"

#include "Core/Base/Random.h"
#include "Core/Network/GameSpy/GameSpy.h"
#include "Core/Network/GameSpy/GameSpyNatNeg.h"
#include "Core/Network/NetTcp.h"
#include "Core/Network/NetTransport.h"

#include <windows.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// NAT negotiation on the loopback: a negotiator on a transport's socket (its magic filter) and a peer played by
// the test, through a stand-in of the natneg servers: the three INITs acknowledged, an ERTTEST answered, the
// CONNECT acknowledged, the pings both ways, the success with the peer's address; a second one told the
// partner never came (a dead beat).
OJ_INTEGRATION_TEST(NatNegotiation, Negotiator) {
    auto swap16 = [](uint16_t v) { return static_cast<uint16_t>((v >> 8) | (v << 8)); };
    const uint32_t loop = 0x0100007Fu;
    oj::net::ServiceHosts& hosts = oj::net::serviceHosts();
    oj::net::UdpSocket natneg;
    oj::net::UdpSocket peer;
    const bool sockets = natneg.open() && peer.open();
    hosts.natneg1 = "127.0.0.1";
    hosts.natneg2 = "127.0.0.1";
    hosts.natnegPort = swap16(natneg.localPort());
    oj::net::availableResult() = oj::net::AvailableCheck::Available;
    oj::net::availableGameName() = oj::net::kGameName;
    oj::net::Transport link;
    oj::net::Transport::Options options;
    options.lanPort = 32666;
    options.gamePort = 32667;
    options.ownIp = loop;
    options.broadcastIp = loop;
    options.broadcastPort = 32668;
    oj::CrtRandom netRandom;
    std::string why;
    const bool opened = sockets && link.open(options, netRandom, &why);
    link.addFilter({0xFD, 0xFC, 0x1E, 0x66, 0x6A, 0xB2},
                   [](const uint8_t* d, size_t n, const oj::net::Address& from) { oj::net::nnProcessData(d, n, from); });
    oj::net::GameSocket game;
    game.send = [&link](const oj::net::Address& to, const void* d, size_t n) { return link.sendRaw(to, d, n); };
    game.port = swap16(32667);
    std::vector<int> progress;
    int result = -1;
    oj::net::Address remote;
    int deadResult = -1;
    auto header = [](uint8_t type, uint32_t c) {
        return std::vector<uint8_t>{0xFD, 0xFC, 0x1E, 0x66, 0x6A, 0xB2, 2, type, static_cast<uint8_t>(c >> 24),
                                    static_cast<uint8_t>(c >> 16), static_cast<uint8_t>(c >> 8),
                                    static_cast<uint8_t>(c)};
    };
    struct Datagram {
        std::vector<uint8_t> bytes;
        oj::net::Address from;
    };
    auto receive = [&](oj::net::UdpSocket& s, Datagram& out) {
        for (int i = 0; i < 2000; ++i) {
            link.update(0.001f);
            oj::net::nnThink();
            uint8_t b[512];
            uint32_t ip = 0;
            uint16_t port = 0;
            const int n = s.receive(b, sizeof b, ip, port);
            if (n >= 0) {
                out.bytes.assign(b, b + n);
                out.from = {ip, port};
                return true;
            }
            Sleep(1);
        }
        return false;
    };
    bool inits = false, ert = false, connected = false, pinged = false, succeeded = false, dead = false;
    if (opened) {
        const int begun = oj::net::nnBeginNegotiationWithSocket(
            game, 4242, 0, [&progress](int state) { progress.push_back(state); },
            [&](int r, std::unique_ptr<oj::net::UdpSocket>, const oj::net::Address* a) {
                result = r;
                if (a != nullptr) remote = *a;
            });
        // The three INITs: porttype 0 from the game's socket, 1 and 2 from the negotiation's own.
        std::array<oj::net::Address, 3> from{};
        int seen = 0;
        for (int i = 0; begun == 0 && i < 3; ++i) {
            Datagram d;
            if (!receive(natneg, d) || d.bytes.size() != 21 + 9 || d.bytes[7] != 0 || d.bytes[12] > 2) break;
            const uint8_t type = d.bytes[12];
            from[type] = d.from;
            seen |= 1 << type;
            const bool portOk = type == 2 ? (d.bytes[19] << 8 | d.bytes[20]) == 32667
                                          : d.bytes[19] == 0 && d.bytes[20] == 0;
            if (!portOk || d.bytes[13] != 0 || d.bytes[14] != 1 ||
                std::string(reinterpret_cast<const char*>(d.bytes.data() + 21)) != "juicedpc") {
                seen = -64;
            }
        }
        inits = seen == 7 && from[0].port == swap16(32667) && from[1].port == from[2].port;
        for (uint8_t type = 0; inits && type < 3; ++type) {
            std::vector<uint8_t> ack = header(1, 4242);
            ack.resize(21, 0);
            ack[12] = type;
            natneg.sendTo(from[type].ip, from[type].port, ack.data(), ack.size());
        }
        std::vector<uint8_t> test = header(2, 4242);
        test.resize(21, 0);
        natneg.sendTo(from[1].ip, from[1].port, test.data(), test.size());
        Datagram d;
        ert = inits && receive(natneg, d) && d.bytes.size() == 21 && d.bytes[7] == 3 && progress == std::vector<int>{1};
        // CONNECT: the peer's address, not finished.
        std::vector<uint8_t> connect = header(5, 4242);
        const uint16_t peerPort = swap16(peer.localPort());
        connect.insert(connect.end(), {127, 0, 0, 1, static_cast<uint8_t>(peerPort >> 8),
                                       static_cast<uint8_t>(peerPort), 0, 0});
        natneg.sendTo(from[1].ip, from[1].port, connect.data(), connect.size());
        connected = ert && receive(natneg, d) && d.bytes.size() == 21 && d.bytes[7] == 6 && d.bytes[13] == 0;
        // The negotiator's ping reaches the peer from the game's socket; the peer answers it has its data.
        Datagram ping;
        pinged = connected && receive(peer, ping) && ping.bytes.size() == 20 && ping.bytes[7] == 7 &&
                 ping.from.port == swap16(32667) && ping.bytes[18] == 0 && ping.bytes[19] == 0;
        std::vector<uint8_t> back = header(7, 4242);
        back.insert(back.end(), {127, 0, 0, 1, 0x7F, 0x9B, 1, 0});
        peer.sendTo(loop, swap16(32667), back.data(), back.size());
        for (int i = 0; pinged && i < 2000 && result < 0; ++i) {
            link.update(0.001f);
            oj::net::nnThink();
            Sleep(1);
        }
        succeeded = result == oj::net::kNnSuccess && remote.ip == loop && remote.port == peer.localPort() &&
                    progress == std::vector<int>{1, 2};
        // A second negotiation: the servers say the partner never came.
        const int second = oj::net::nnBeginNegotiationWithSocket(
            game, 99, 1, nullptr, [&](int r, std::unique_ptr<oj::net::UdpSocket>, const oj::net::Address*) {
                deadResult = r;
            });
        Datagram init;
        for (int i = 0; second == 0 && i < 3 && receive(natneg, init); ++i) {
            if (init.bytes[12] == 1) {
                std::vector<uint8_t> finished = header(5, 99);
                finished.insert(finished.end(), {0, 0, 0, 0, 0, 0, 0, 1});
                natneg.sendTo(init.from.ip, init.from.port, finished.data(), finished.size());
            }
        }
        for (int i = 0; i < 2000 && deadResult < 0; ++i) {
            link.update(0.001f);
            oj::net::nnThink();
            Sleep(1);
        }
        dead = deadResult == oj::net::kNnDeadBeatPartner;
        oj::net::nnFreeNegotiateList();
    }
    check(inits && ert && connected && pinged && succeeded && dead,
          "NAT negotiation: the INITs, ERTTEST, CONNECT and its ack, the pings, the success; a dead beat partner");
    hosts = oj::net::ServiceHosts{};
}
