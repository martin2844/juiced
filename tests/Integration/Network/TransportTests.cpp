// Integration tests: Transport.

#include "Framework/Test.h"

#include "Core/Base/Random.h"
#include "Core/Network/NetTransport.h"

#include <windows.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// The network's transport on the loopback: the code's keys; a host found by a search, two peers joining it
// (each told of the other), a reliable message from the host and a plain one from a peer, a peer leaving.
OJ_INTEGRATION_TEST(Transport, Loopback) {
    uint8_t keys[8] = {};
    oj::net::Transport::code(keys, sizeof keys);
    const bool coded = keys[0] == 0x0D && keys[1] == 0x7C && keys[2] == 0xCF && keys[3] == 0xF6 &&
                       keys[4] == 0x21 && keys[5] == 0xC0 && keys[6] == 0x83 && keys[7] == 0x5A;
    const uint32_t loop = 0x0100007Fu;  // 127.0.0.1 in network order
    auto options = [&](uint16_t lan, uint16_t game, uint16_t to) {
        oj::net::Transport::Options o;
        o.lanPort = lan;
        o.gamePort = game;
        o.ownIp = loop;
        o.broadcastIp = loop;
        o.broadcastPort = to;
        return o;
    };
    oj::CrtRandom netRandom;
    oj::net::Transport host, first, second;
    struct Seen {
        std::vector<std::pair<std::vector<uint8_t>, int32_t>> got;
    } seenHost, seenFirst, seenSecond;
    auto catcher = [](Seen& s) {
        return [&s](const uint8_t* d, size_t n, int32_t peer) { s.got.push_back({{d, d + n}, peer}); };
    };
    host.setHandler(catcher(seenHost));
    first.setHandler(catcher(seenFirst));
    second.setHandler(catcher(seenSecond));
    std::string why;
    bool opened = host.open(options(28666, 28667, 28668), netRandom, &why) &&
                  first.open(options(28668, 28669, 28666), netRandom, &why) &&
                  second.open(options(28670, 28671, 28666), netRandom, &why);
    auto pump = [&](const std::function<bool()>& done) {
        for (int i = 0; i < 300 && !done(); ++i) {
            host.update(0.01f);
            first.update(0.01f);
            second.update(0.01f);
            Sleep(1);
        }
        return done();
    };
    bool found = false, joined = false, both = false, messages = false, left = false;
    if (opened) {
        host.host(u"OJ test", 1, 6);
        first.search(netRandom);
        found = pump([&] { return !first.sessions().empty(); }) &&
                first.sessions()[0].info.used == 1 && first.sessions()[0].info.max == 6 &&
                std::u16string(first.sessions()[0].info.name.data()) == u"OJ test" &&
                first.sessions()[0].address.port == static_cast<uint16_t>((28667 >> 8) | ((28667 & 0xFF) << 8));
        if (found) {
            first.join(first.sessions()[0].address, 1);
            joined = pump([&] { return first.inSession() && host.peers().size() == 1; }) &&
                     first.peer(0) != nullptr && host.session().used == 2;
            second.join(first.sessions()[0].address, 1);
            both = pump([&] {
                return second.peers().size() == 2 && first.peers().size() == 2 && host.peers().size() == 2;
            });
            const uint8_t hello[12] = {0x05, 0, 0, 0, 42};
            host.send(-2, hello, sizeof hello, true);
            const uint8_t state[8] = {0x14, 0, 0, 0, 7};
            const int32_t firstId = host.peers()[0].id;
            first.send(0, state, sizeof state, false);
            messages = pump([&] {
                auto has = [](const Seen& s, uint8_t id, int32_t from) {
                    for (const auto& [d, p] : s.got) {
                        if (!d.empty() && d[0] == id && p == from) return true;
                    }
                    return false;
                };
                return has(seenFirst, 0x05, 0) && has(seenSecond, 0x05, 0) && has(seenHost, 0x14, firstId);
            });
            first.leave();
            left = pump([&] { return host.peers().size() == 1 && second.peers().size() == 1; }) &&
                   host.session().used == 2;
        }
    }
    check(coded && opened && found && joined && both && messages && left,
          "network transport: the code, a search, two joins, reliable and plain messages, a leave");
}

// A machine whose frames are held by the race's load (here on the main thread; the stock's frames went on): with the
// load's keep-alive the host keeps it past 25 s of its own time (the keep-alives every 1.5 s), without it the host
// drops it (timed out).
OJ_INTEGRATION_TEST(Transport, LoadKeepAlive) {
    const uint32_t loop = 0x0100007Fu;
    auto options = [&](uint16_t lan, uint16_t game, uint16_t to) {
        oj::net::Transport::Options o;
        o.lanPort = lan;
        o.gamePort = game;
        o.ownIp = loop;
        o.broadcastIp = loop;
        o.broadcastPort = to;
        return o;
    };
    oj::CrtRandom netRandom;
    oj::net::Transport host, client;
    std::string why;
    bool ok = host.open(options(28676, 28677, 28678), netRandom, &why) &&
              client.open(options(28678, 28679, 28676), netRandom, &why);
    auto pump = [&](const std::function<bool()>& done) {
        for (int i = 0; i < 300 && !done(); ++i) {
            host.update(0.01f);
            client.update(0.01f);
            Sleep(1);
        }
        return done();
    };
    // The host's 30 s (1 s a step, 100 ms apart) while the client's frames are held.
    auto held = [&] {
        for (int i = 0; i < 30 && host.peers().size() == 1; ++i) {
            host.update(1.0f);
            Sleep(100);
        }
        return host.peers().size() == 1;
    };
    bool kept = false, dropped = false;
    uint32_t sent = 0;
    if (ok) {
        host.host(u"OJ test", 1, 6);
        client.search(netRandom);
        ok = pump([&] { return !client.sessions().empty(); });
        if (ok) {
            client.join(client.sessions()[0].address, 1);
            ok = pump([&] { return client.inSession() && host.peers().size() == 1; });
        }
        if (ok) {
            {
                const oj::net::LoadKeepAlive keepAlive(&client);
                kept = held();
                sent = keepAlive.sent();
            }
            pump([] { return false; });
            dropped = !held();
        }
    }
    check(ok && kept && sent >= 1 && dropped,
          "network transport: the load's keep-alive keeps a held machine past 25 s; without it, dropped");
}
