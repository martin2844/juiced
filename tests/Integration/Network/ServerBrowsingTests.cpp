// Integration tests: ServerBrowsing.

#include "Framework/Test.h"

#include "Core/Base/Random.h"
#include "Core/Network/GameSpy/GameSpy.h"
#include "Core/Network/GameSpy/GameSpyBrowser.h"
#include "Core/Network/GameSpy/GameSpyQr2.h"
#include "Core/Network/NetTcp.h"
#include "Core/Network/NetTransport.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// QR2 and server browsing on the loopback with stand-ins of the master and the master list server: the host's
// record on a transport's socket (its FE FD filter); the first heartbeat (statechanged 3, the keys), the
// challenge answered and the public address taken from it, a query, an echo, a client message carrying a NAT
// negotiation's cookie; the browser's list (encrypted: two servers with keys, one public and one behind a NAT,
// and the QR2 host without keys, queried directly over UDP), pushed messages (a server, a deletion, a
// keep-alive), a server's keys asked of the master again (reconnecting without the list) and the cookie sent
// through it; the shutdown's heartbeat.
OJ_INTEGRATION_TEST(ServerBrowsing, Qr2AndBrowser) {
    oj::net::startWinsock();
    auto swap16 = [](uint16_t v) { return static_cast<uint16_t>((v >> 8) | (v << 8)); };
    auto bytesOfText = [](std::string_view s) { return std::vector<uint8_t>(s.begin(), s.end()); };
    auto has = [](const std::vector<uint8_t>& v, std::string_view s) {
        return std::search(v.begin(), v.end(), s.begin(), s.end()) != v.end();
    };
    auto pair = [](std::string_view k, std::string_view v) {
        std::string s(k);
        s.push_back('\0');
        s += v;
        s.push_back('\0');
        return s;
    };
    const uint32_t loop = 0x0100007Fu;
    oj::net::ServiceHosts& hosts = oj::net::serviceHosts();
    oj::net::UdpSocket master;
    oj::net::UdpSocket browser;
    const bool sockets = master.open() && browser.open();
    hosts.master = "127.0.0.1";
    hosts.masterPort = swap16(master.localPort());
    oj::net::Transport host;
    oj::net::Transport::Options options;
    options.lanPort = 31666;
    options.gamePort = 31667;
    options.ownIp = loop;
    options.broadcastIp = loop;
    options.broadcastPort = 31668;
    oj::CrtRandom netRandom;
    std::string why;
    const bool opened = sockets && host.open(options, netRandom, &why);
    oj::net::Qr2& qr2 = oj::net::qr2Record();
    host.addFilter({0xFE, 0xFD}, [&qr2](const uint8_t* d, size_t n, const oj::net::Address& from) {
        qr2.parseQuery(d, n, from);
    });
    oj::net::GameSocket game;
    game.send = [&host](const oj::net::Address& to, const void* d, size_t n) { return host.sendRaw(to, d, n); };
    game.port = swap16(31667);
    for (int k = 50; k <= 53; ++k) oj::net::qr2RegisterKey(k, "key" + std::to_string(k));
    const int init = qr2.initSocket(
        game, 31667, oj::net::kGameName, oj::net::kSecretKey, true, true,
        [](int key, oj::net::Qr2Buffer& out) {
            if (key == 1 || key == 2) out.add("OJ host");
            if (key == 8 || key == 50) out.addInt(2);
            if (key == 10) out.addInt(6);
        },
        [](int, int, oj::net::Qr2Buffer&) {}, [](int, int, oj::net::Qr2Buffer&) {},
        [](int type, oj::net::Qr2KeyBuffer& keys) {
            if (type == oj::net::kQr2ServerKey) {
                for (int k : {1, 2, 8, 10, 50}) keys.add(k);
            }
        },
        [](int) { return 0; }, [](int, const std::u16string&) {});
    uint32_t publicIp = 0;
    uint16_t publicPort = 0;
    int32_t cookie = 0;
    qr2.publicAddressCallback = [&](uint32_t ip, uint16_t port) {
        publicIp = ip;
        publicPort = port;
    };
    qr2.natNegotiateCallback = [&](int32_t c) { cookie = c; };
    auto receive = [&](oj::net::UdpSocket& s, std::vector<uint8_t>& out) {
        for (int i = 0; i < 1000; ++i) {
            host.update(0.001f);
            uint8_t b[2048];
            uint32_t ip = 0;
            uint16_t port = 0;
            const int n = s.receive(b, sizeof b, ip, port);
            if (n >= 0) {
                out.assign(b, b + n);
                return true;
            }
            Sleep(1);
        }
        return false;
    };
    const std::array<uint8_t, 4> key = qr2.instanceKey();
    auto withKey = [&](std::initializer_list<uint8_t> head, const std::vector<uint8_t>& tail) {
        std::vector<uint8_t> p(head);
        p.insert(p.end(), key.begin(), key.end());
        p.insert(p.end(), tail.begin(), tail.end());
        return p;
    };
    bool heartbeat = false, challenged = false, queried = false, echoed = false, messaged = false;
    std::vector<uint8_t> got;
    if (opened && init == 0) {
        qr2.think();
        heartbeat = receive(master, got) && got.size() > 5 && got[0] == 0x03 &&
                    std::equal(key.begin(), key.end(), got.begin() + 1) && has(got, pair("localport", "31667")) &&
                    has(got, pair("natneg", "1")) && has(got, pair("statechanged", "3")) &&
                    has(got, pair("gamename", "juicedpc")) && has(got, pair("publicip", "0")) &&
                    has(got, pair("hostname", "OJ host")) && has(got, pair("key50", "2")) &&
                    qr2.listedState() == 2;
        // The challenge with our address 127.0.0.1:8667 in its last 13 bytes.
        const std::string challenge = "abcdef7F00000121DB";
        std::vector<uint8_t> tail = bytesOfText(challenge);
        tail.push_back(0);
        const std::vector<uint8_t> c = withKey({0xFE, 0xFD, 0x01}, tail);
        master.sendTo(loop, swap16(31667), c.data(), c.size());
        challenged = receive(master, got) && got.size() > 5 && got[0] == 0x01 &&
                     std::string(reinterpret_cast<const char*>(got.data() + 5)) ==
                         oj::net::challengeAnswer(challenge) &&
                     publicIp == loop && publicPort == 8667 && qr2.listedState() == 0;
        const std::vector<uint8_t> q = {0xFE, 0xFD, 0x00, 1, 2, 3, 4, 3, 1, 8, 50, 0, 0};
        browser.sendTo(loop, swap16(31667), q.data(), q.size());
        const std::string reply = std::string("\x00\x01\x02\x03\x04", 5) + pair("OJ host", "2") + "2" + '\0';
        queried = receive(browser, got) && std::string(got.begin(), got.end()) == reply;
        const std::vector<uint8_t> e = {0xFE, 0xFD, 0x02, 9, 8, 7, 6, 'p', 'i', 'n', 'g'};
        browser.sendTo(loop, swap16(31667), e.data(), e.size());
        echoed = receive(browser, got) &&
                 got == std::vector<uint8_t>{0x05, 9, 8, 7, 6, 'p', 'i', 'n', 'g'};
        const std::vector<uint8_t> m =
            withKey({0xFE, 0xFD, 0x06}, {7, 7, 7, 7, 0xFD, 0xFC, 0x1E, 0x66, 0x6A, 0xB2, 0x00, 0x00, 0x30, 0x39});
        master.sendTo(loop, swap16(31667), m.data(), m.size());
        messaged = receive(master, got) && got.size() == 9 && got[0] == 0x07 && got[5] == 7 && cookie == 12345;
        cookie = 0;
        master.sendTo(loop, swap16(31667), m.data(), m.size());
        messaged = messaged && receive(master, got) && got.size() == 9 && cookie == 0;  // the same key: acked only
    }
    check(heartbeat && challenged && queried && echoed && messaged,
          "QR2: the heartbeat, the challenge answered (the public address), a query, an echo, a client message's "
          "NAT negotiation cookie");

    // The master list server's stand-in.
    oj::net::availableResult() = oj::net::AvailableCheck::Available;
    oj::net::TcpListener listServer;
    const bool listening = listServer.listen(0);
    hosts.list = "127.0.0.1";
    hosts.listPort = listServer.port();
    std::vector<std::pair<int, oj::net::SbServer*>> browsed;
    std::unique_ptr<oj::net::ServerBrowser> sb = oj::net::ServerBrowser::create(
        oj::net::kGameName, oj::net::kGameName, oj::net::kSecretKey, 0, 10, 1,
        [&browsed](oj::net::ServerBrowser&, int reason, oj::net::SbServer* s) { browsed.push_back({reason, s}); });
    auto pumpUntil = [&](const std::function<bool()>& done) {
        for (int i = 0; i < 2000 && !done(); ++i) {
            host.update(0.001f);
            if (sb) sb->think();
            Sleep(1);
        }
        return done();
    };
    // A request read whole (its u16 BE length first).
    auto readRequest = [&](oj::net::TcpConnection& c, size_t at_least) {
        for (int i = 0; i < 2000; ++i) {
            c.poll();
            if (c.received().size() >= at_least) return true;
            Sleep(1);
        }
        return false;
    };
    auto take = [](oj::net::TcpConnection& c, size_t n) {
        std::vector<uint8_t> v(c.received().begin(), c.received().begin() + static_cast<std::ptrdiff_t>(n));
        c.received().erase(c.received().begin(), c.received().begin() + static_cast<std::ptrdiff_t>(n));
        return v;
    };
    // The reply: the crypt's header with the server's key, then the rest encrypted by the challenge's GOA.
    auto replyHeader = [](const std::array<uint8_t, 8>& challenge, oj::net::GoaCrypt& crypt) {
        const uint8_t serverKey[8] = {'S', 'E', 'R', 'V', 'E', 'R', 'K', 'Y'};
        std::vector<uint8_t> out = {0x02 ^ 0xEC, 0x11, 0x22, 0x08 ^ 0xEA};
        out.insert(out.end(), serverKey, serverKey + 8);
        oj::net::listCryptInit(crypt, challenge, oj::net::kSecretKey, serverKey, 8);
        return out;
    };
    auto put = [](std::vector<uint8_t>& v, std::initializer_list<uint8_t> b) { v.insert(v.end(), b); };
    auto putText = [](std::vector<uint8_t>& v, std::string_view s) {
        v.insert(v.end(), s.begin(), s.end());
        v.push_back(0);
    };
    auto message = [](uint8_t type, const std::vector<uint8_t>& body) {
        std::vector<uint8_t> m = {static_cast<uint8_t>((body.size() + 3) >> 8), static_cast<uint8_t>(body.size() + 3),
                                  type};
        m.insert(m.end(), body.begin(), body.end());
        return m;
    };
    bool requested = false, listed = false, pushed = false, direct = false, aux = false, sentCookie = false;
    oj::net::TcpConnection conn;
    oj::net::GoaCrypt crypt;
    const uint8_t fields[] = {50, 51};
    if (opened && listening && sb && sb->update(true, false, fields, 2, nullptr) == 0) {
        for (int i = 0; i < 2000 && !listServer.accept(conn); ++i) Sleep(1);
        if (readRequest(conn, 2)) {
            const size_t length = (static_cast<size_t>(conn.received()[0]) << 8) | conn.received()[1];
            if (readRequest(conn, length)) {
                const std::vector<uint8_t> r = take(conn, length);
                std::vector<uint8_t> expect = {0x00, 0x01, 0x03, 0, 0, 0, 0};
                putText(expect, "juicedpc");
                putText(expect, "juicedpc");
                const std::array<uint8_t, 8> challenge = sb->challenge();
                expect.insert(expect.end(), challenge.begin(), challenge.end());
                putText(expect, "");
                putText(expect, "\\key50\\key51");
                put(expect, {0, 0, 0, 0});
                requested = std::vector<uint8_t>(r.begin() + 2, r.end()) == expect;
                std::vector<uint8_t> out = replyHeader(challenge, crypt);
                std::vector<uint8_t> list;
                put(list, {1, 2, 3, 4, 0x19, 0x64});            // our address, the default port 6500
                put(list, {2, 0});                              // two keys: key50 a string, key51 a byte
                putText(list, "key50");
                put(list, {1});
                putText(list, "key51");
                put(list, {1});                                 // one popular value
                putText(list, "popular");
                put(list, {0x41, 10, 0, 0, 1, 0x00, 3});        // public: popular[0], 3
                put(list, {0x56, 5, 6, 7, 8, 0x23, 0x28, 192, 168, 1, 5, 0xFF});  // behind a NAT: 9000
                putText(list, "inline");
                put(list, {2});
                put(list, {0x11, 127, 0, 0, 1, 0x7B, 0xB3});    // the QR2 host (31667), no keys
                put(list, {0x00, 0xFF, 0xFF, 0xFF, 0xFF});      // the end
                std::vector<uint8_t> pushServer = {0xC1, 9, 9, 9, 9};
                putText(pushServer, "pushed");
                put(pushServer, {4});
                putText(pushServer, "rule");
                putText(pushServer, "x");
                put(pushServer, {0});
                for (const auto& m : {message(2, pushServer), message(4, {10, 0, 0, 1, 0x19, 0x64}),
                                      message(3, {})}) {
                    list.insert(list.end(), m.begin(), m.end());
                }
                for (uint8_t& c : list) c = crypt.encrypt(c);
                out.insert(out.end(), list.begin(), list.end());
                conn.send(out.data(), out.size());
            }
        }
        auto count = [&browsed](int reason) {
            return std::count_if(browsed.begin(), browsed.end(), [reason](const auto& e) { return e.first == reason; });
        };
        pumpUntil([&] { return count(oj::net::ServerBrowser::kUpdateComplete) > 0; });
        const oj::net::SbServer* nat = sb->count() > 0 ? sb->getServer(0) : nullptr;
        listed = sb->count() == 3 && sb->myPublicIp() == 0x04030201u && nat != nullptr && nat->flags == 0x56 &&
                 nat->publicIp == 0x08070605u && nat->publicPort == swap16(9000) &&
                 nat->privateIp == 0x0501A8C0u && nat->privatePort == swap16(6500) &&
                 nat->stringValue("key50", "") == "inline" && nat->intValue("key51", 0) == 2 &&
                 count(oj::net::ServerBrowser::kServerAdded) == 4;
        const oj::net::SbServer* pushedServer = sb->count() == 3 ? sb->getServer(2) : nullptr;
        pushed = pushedServer != nullptr && pushedServer->stringValue("key50", "") == "pushed" &&
                 pushedServer->stringValue("rule", "") == "x" && pushedServer->hasFullKeys() &&
                 count(oj::net::ServerBrowser::kServerDeleted) == 1 && readRequest(conn, 3) &&
                 take(conn, 3) == std::vector<uint8_t>{0, 3, 3};
        const oj::net::SbServer* qr2Host = sb->count() == 3 ? sb->getServer(1) : nullptr;
        direct = qr2Host != nullptr && qr2Host->publicPort == swap16(31667) &&
                 qr2Host->stringValue("key50", "") == "2" && qr2Host->hasBasicKeys() &&
                 count(oj::net::ServerBrowser::kServerUpdated) >= 2;
        // Asked again through the master: disconnected first, so connected again without the list.
        sb->disconnect();
        const bool asked = sb->auxUpdateIp("5.6.7.8", 9000, true, true, true) == 0;
        oj::net::TcpConnection second;
        for (int i = 0; asked && i < 2000 && !listServer.accept(second); ++i) Sleep(1);
        if (asked && readRequest(second, 2)) {
            const size_t length = (static_cast<size_t>(second.received()[0]) << 8) | second.received()[1];
            if (readRequest(second, length + 9)) {
                const std::vector<uint8_t> r = take(second, length);
                const std::vector<uint8_t> info = take(second, 9);
                const bool noList = r[length - 1] == 2;
                std::vector<uint8_t> out = replyHeader(sb->challenge(), crypt);
                std::vector<uint8_t> body;
                put(body, {1, 2, 3, 4, 0x19, 0x64});
                // The server's full rules (no keys: the list's are gone with the disconnection).
                std::vector<uint8_t> full = {0x96, 5, 6, 7, 8, 0x23, 0x28, 192, 168, 1, 5};
                putText(full, "gamemode");
                putText(full, "openwaiting");
                putText(full, "key50");
                putText(full, "full");
                put(full, {0});
                const std::vector<uint8_t> m = message(2, full);
                body.insert(body.end(), m.begin(), m.end());
                for (uint8_t& c : body) c = crypt.encrypt(c);
                out.insert(out.end(), body.begin(), body.end());
                second.send(out.data(), out.size());
                const size_t before = browsed.size();
                pumpUntil([&] { return browsed.size() > before; });
                aux = noList && info == std::vector<uint8_t>{0, 9, 1, 5, 6, 7, 8, 0x23, 0x28} &&
                      nat->stringValue("gamemode", "") == "openwaiting" && nat->hasFullKeys() &&
                      browsed.back().first == oj::net::ServerBrowser::kServerUpdated;
                sentCookie = sb->sendNatNegotiateCookieToServer("5.6.7.8", 9000, 0x01020304) == 0 &&
                             readRequest(second, 19) &&
                             take(second, 19) == std::vector<uint8_t>{0, 19, 2, 5, 6, 7, 8, 0x23, 0x28, 0xFD, 0xFC,
                                                                      0x1E, 0x66, 0x6A, 0xB2, 1, 2, 3, 4};
            }
        }
    }
    sb.reset();
    check(requested && listed && pushed && direct && aux && sentCookie,
          "server browsing: the list's request, the encrypted list (public, behind a NAT, queried directly), "
          "pushed messages, a server asked again through the master, the NAT negotiation's cookie sent");

    bool shut = false;
    if (opened && init == 0) {
        qr2.shutdown();
        shut = receive(master, got) && got[0] == 0x03 && has(got, pair("statechanged", "2")) &&
               got.size() > 2 && got[got.size() - 1] == 0 && got[got.size() - 2] == 0 &&
               has(got, pair("publicport", "8667")) && !qr2.valid();
    }
    check(shut, "QR2: the shutdown's heartbeat (statechanged 2, no keys)");
    hosts = oj::net::ServiceHosts{};
}
