// Integration tests: Matchmaking.

#include "Framework/Test.h"

#include "Core/Base/Random.h"
#include "Core/Network/GameSpy/GameSpy.h"
#include "Core/Network/GameSpy/GameSpyNatNeg.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetTcp.h"
#include "Core/Network/NetTransport.h"

#include <windows.h>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// The matchmaking on the loopback with stand-ins of the master, the list server and the natneg servers: a host
// listed by QR2 (its challenge and public address), a client's search (the list giving the host without keys,
// its keys asked of it directly), the result, the client's direct join (JOIN, PEER: both in the transport's
// session); a second client behind the host's NAT negotiation (the cookie through the master, the negotiation,
// its JOIN after it): the first client told of it (PEER, the same public address); the leave.
OJ_INTEGRATION_TEST(Matchmaking, HostListJoin) {
    oj::net::startWinsock();
    auto swap16 = [](uint16_t v) { return static_cast<uint16_t>((v >> 8) | (v << 8)); };
    const uint32_t loop = 0x0100007Fu;
    oj::net::ServiceHosts& hosts = oj::net::serviceHosts();
    oj::net::UdpSocket master;
    oj::net::UdpSocket natneg;
    oj::net::TcpListener listServer;
    const bool sockets = master.open() && natneg.open() && listServer.listen(0);
    hosts.master = "127.0.0.1";
    hosts.masterPort = swap16(master.localPort());
    hosts.list = "127.0.0.1";
    hosts.listPort = listServer.port();
    hosts.natneg1 = "127.0.0.1";
    hosts.natneg2 = "127.0.0.1";
    hosts.natnegPort = swap16(natneg.localPort());
    oj::net::availableResult() = oj::net::AvailableCheck::Available;
    oj::net::availableGameName() = oj::net::kGameName;
    auto options = [&](uint16_t lan, uint16_t game) {
        oj::net::Transport::Options o;
        o.lanPort = lan;
        o.gamePort = game;
        o.ownIp = loop;
        o.broadcastIp = loop;
        o.broadcastPort = 1;
        return o;
    };
    oj::CrtRandom netRandom;
    std::string why;
    oj::net::Transport hostLink, firstLink, secondLink;
    const bool opened = sockets && hostLink.open(options(33666, 33667), netRandom, &why) &&
                        firstLink.open(options(33668, 33669), netRandom, &why) &&
                        secondLink.open(options(33670, 33671), netRandom, &why);
    auto hostMm = std::make_unique<oj::net::Matchmaking>(&hostLink);
    auto firstMm = std::make_unique<oj::net::Matchmaking>(&firstLink);
    auto pump = [&](const std::function<bool()>& done) {
        for (int i = 0; i < 3000 && !done(); ++i) {
            hostLink.update(0.001f);
            firstLink.update(0.001f);
            secondLink.update(0.001f);
            hostMm->update(0.001f);
            firstMm->update(0.001f);
            Sleep(1);
        }
        return done();
    };
    struct Datagram {
        std::vector<uint8_t> bytes;
        oj::net::Address from;
    };
    auto receive = [&](oj::net::UdpSocket& s, Datagram& out) {
        bool got = false;
        return pump([&] {
            if (got) return true;
            uint8_t b[2048];
            uint32_t ip = 0;
            uint16_t port = 0;
            const int n = s.receive(b, sizeof b, ip, port);
            if (n < 0) return false;
            out.bytes.assign(b, b + n);
            out.from = {ip, port};
            got = true;
            return true;
        });
    };
    bool listedUp = false, found = false, joined = false, negotiated = false, told = false, left = false;
    if (opened) {
        // The host: its transport hosting without the LAN's answers, then listed.
        hostLink.host(u"OJ online", 1, 6);
        hostLink.setAnswering(false);
        hostMm->setInfo(u"OJ online", 0, u"Arcade");
        oj::net::MatchAttribute name;
        name.id = 0;
        name.type = 2;
        name.length = 16;
        name.text = u"Host";
        oj::net::MatchAttribute playing;
        playing.id = 1;
        playing.type = 1;
        const oj::net::MatchAttribute* attributes[2] = {&name, &playing};
        const bool hosted = hostMm->host(0, 1, 5, 2, attributes);
        Datagram heartbeat;
        const bool beat = hosted && receive(master, heartbeat) && heartbeat.bytes[0] == 0x03;
        // The challenge with the host's address 127.0.0.1:33667 (0x8383).
        std::vector<uint8_t> c = {0xFE, 0xFD, 0x01};
        c.insert(c.end(), heartbeat.bytes.begin() + 1, heartbeat.bytes.begin() + 5);
        for (char ch : std::string("xyz7F0000018383")) c.push_back(static_cast<uint8_t>(ch));
        c.push_back(0);
        master.sendTo(heartbeat.from.ip, heartbeat.from.port, c.data(), c.size());
        Datagram answer;
        listedUp = beat && receive(master, answer) && answer.bytes[0] == 0x01 && hostMm->hostingUp() &&
                   pump([&] { return hostMm->publicAddressKnown == 1; }) &&
                   hostMm->presenceAddress && hostMm->presenceAddress->port == swap16(33667);
        // The client's search.
        oj::net::SearchAttribute wanted[2] = {{0, 2, 16, 0}, {1, 1, 0, 0}};
        firstMm->setSearchAttributes(5, 2, wanted);
        firstMm->startSearch(2, 0, 0);
        oj::net::TcpConnection conn;
        for (int i = 0; i < 2000 && !listServer.accept(conn); ++i) Sleep(1);
        for (int i = 0; i < 2000 && conn.received().size() < 2; ++i) {
            conn.poll();
            Sleep(1);
        }
        if (conn.received().size() >= 2 && firstMm->browser() != nullptr) {
            const size_t length = (static_cast<size_t>(conn.received()[0]) << 8) | conn.received()[1];
            for (int i = 0; i < 2000 && conn.received().size() < length; ++i) {
                conn.poll();
                Sleep(1);
            }
            const std::string fields(conn.received().begin(), conn.received().end());
            const bool asked = fields.find("\\key50\\key51\\key52\\key53\\key54\\key55") != std::string::npos;
            const uint8_t serverKey[8] = {'S', 'E', 'R', 'V', 'E', 'R', 'K', 'Y'};
            std::vector<uint8_t> out = {0x02 ^ 0xEC, 0x11, 0x22, 0x08 ^ 0xEA};
            out.insert(out.end(), serverKey, serverKey + 8);
            oj::net::GoaCrypt crypt;
            oj::net::listCryptInit(crypt, firstMm->browser()->challenge(), oj::net::kSecretKey, serverKey, 8);
            // Our address 1.2.3.4, no keys, no popular values, the host (unsolicited UDP, its port) without keys.
            std::vector<uint8_t> list = {1, 2, 3, 4, 0x19, 0x64, 0, 0, 0x11, 127, 0, 0, 1, 0x83, 0x83,
                                         0x00, 0xFF, 0xFF, 0xFF, 0xFF};
            for (uint8_t& b : list) b = crypt.encrypt(b);
            out.insert(out.end(), list.begin(), list.end());
            conn.send(out.data(), out.size());
            found = asked && pump([&] { return firstMm->listIn == 1 && firstMm->detailsIn == 1; }) &&
                    firstMm->results.size() == 1 && firstMm->results[0].valid == 1 &&
                    firstMm->results[0].players == 1 && firstMm->results[0].openSlots == 5 &&
                    firstMm->results[0].sessionId == hostMm->sessionId() &&
                    firstMm->results[0].attributes.size() == 2 &&
                    firstMm->results[0].attributes[0].text == u"Host" && firstMm->results[0].quality > 0.0f;
        }
        if (found) {
            firstMm->sortResults();
            firstMm->join(0, 1);
            joined = pump([&] {
                return firstLink.inSession() && hostLink.peers().size() == 1 && hostMm->clients() == 1;
            }) && firstLink.hostAddress().port == swap16(33667) && firstLink.peers().size() == 1 &&
                     oj::net::findAttribute(firstMm->results[0], 0) != nullptr;
        }
        // The second client's cookie through the master (the host's QR2 client message), then the negotiation.
        if (joined) {
            std::vector<uint8_t> m = {0xFE, 0xFD, 0x06};
            m.insert(m.end(), heartbeat.bytes.begin() + 1, heartbeat.bytes.begin() + 5);
            m.insert(m.end(), {1, 2, 3, 4, 0xFD, 0xFC, 0x1E, 0x66, 0x6A, 0xB2, 0x00, 0x77, 0x77, 0x77});
            master.sendTo(heartbeat.from.ip, heartbeat.from.port, m.data(), m.size());
            Datagram acknowledged;
            const bool messageAcked = receive(master, acknowledged) && acknowledged.bytes.size() == 9 &&
                                      acknowledged.bytes[0] == 0x07;
            // The host's INITs (index 0); the CONNECT to its own socket with the second client's address.
            std::array<oj::net::Address, 3> from{};
            bool inits = messageAcked;
            for (int i = 0; i < 3 && inits; ++i) {
                Datagram d;
                inits = receive(natneg, d) && d.bytes.size() >= 21 && d.bytes[7] == 0 && d.bytes[13] == 0 &&
                        d.bytes[12] < 3;
                if (inits) from[d.bytes[12]] = d.from;
            }
            std::vector<uint8_t> connect = {0xFD, 0xFC, 0x1E, 0x66, 0x6A, 0xB2, 2, 5, 0x00, 0x77, 0x77, 0x77,
                                            127, 0, 0, 1, 0x83, 0x87, 0, 0};
            natneg.sendTo(from[1].ip, from[1].port, connect.data(), connect.size());
            Datagram ack;
            // The second client's ping back from its game socket, then its JOIN after the negotiation.
            const bool acked = inits && receive(natneg, ack) && ack.bytes[7] == 6;
            std::vector<uint8_t> ping = {0xFD, 0xFC, 0x1E, 0x66, 0x6A, 0xB2, 2, 7, 0x00, 0x77, 0x77, 0x77,
                                         127, 0, 0, 1, 0x83, 0x83, 1, 0};
            secondLink.sendRaw({loop, swap16(33667)}, ping.data(), ping.size());
            negotiated = acked && pump([&] {
                return hostMm->hostAddress() && hostMm->hostAddress()->port == swap16(33671);
            }) && hostMm->negotiating();
            uint8_t join[0x2C] = {0xD2, 0xC8, 0x2A, 0};
            join[8] = 2;  // public: 1.2.3.4 as the first client's
            join[0x0C] = 1;
            join[0x0D] = 2;
            join[0x0E] = 3;
            join[0x0F] = 4;
            join[0x18] = 2;  // private: 127.0.0.1:33671
            join[0x1A] = 0x83;
            join[0x1B] = 0x87;
            join[0x1C] = 127;
            join[0x1F] = 1;
            join[0x28] = 1;
            secondLink.sendPacket({loop, swap16(33667)}, join, sizeof join, true);
            told = negotiated && pump([&] {
                return hostLink.peers().size() == 2 && firstLink.peers().size() == 2 && hostMm->clients() == 2;
            }) && !hostMm->negotiating();
        }
        firstMm->leave();
        hostMm->leave();
        Datagram bye;
        left = receive(master, bye) && bye.bytes[0] == 0x03 && !hostMm->hostingUp() && hostMm->stoppedHosting == 1;
    }
    firstMm.reset();
    hostMm.reset();
    oj::net::nnFreeNegotiateList();
    check(listedUp && found && joined && negotiated && told && left,
          "matchmaking: hosted and listed, a search's result (its keys asked of the host), a direct join, a "
          "client after the host's NAT negotiation told to the first, the leave");
    hosts = oj::net::ServiceHosts{};
}
