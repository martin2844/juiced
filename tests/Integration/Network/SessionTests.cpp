// Integration tests: Session.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Base/GameFiles.h"
#include "Core/Base/Random.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Formats/Cld.h"
#include "Core/Network/NetCar.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/NetTcp.h"
#include "Core/Network/NetTransport.h"
#include "Core/Physics/CarBody.h"
#include "Core/Physics/PhysicsWorld.h"
#include "Core/Race/Racer.h"
#include "Core/Tracks/Track.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// The network's session on the loopback: a host and a guest, their names learned, the setup and the stage
// sent, the cars and their readiness, the race started (the grid, the 0x0E), loaded, the count, the race
// (the guest's car's states reaching the host's copy of it), the finishes and the way back to the lobby.
OJ_INTEGRATION_TEST(Session, HostAndGuest) {
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
    oj::net::Transport hostLink, guestLink;
    std::string why;
    const bool opened = hostLink.open(options(29666, 29667, 29668), netRandom, &why) &&
                        guestLink.open(options(29668, 29669, 29666), netRandom, &why);
    oj::net::NetSession hostSession(hostLink), guestSession(guestLink);
    int hostCar = 0, guestCar = 0, remoteOnHost = 0;
    std::vector<uint8_t> applied;
    for (oj::net::NetSession* n : {&hostSession, &guestSession}) {
        n->hooks.remote = [&](void* car) { return car == &remoteOnHost; };
        n->hooks.carState = [](void*, uint8_t* state) {
            for (int i = 0; i < 0x40; ++i) state[i] = static_cast<uint8_t>(i);
            state[0x3D] = 0x20;  // its race paused: applied as it is
        };
        n->hooks.applyState = [&](void*, const uint8_t* state) { applied.assign(state, state + 0x40); };
        n->hooks.carDistance = [](void*) { return 100.0f; };
        n->hooks.raceType = [] { return oj::RaceType::Circuit; };
    }
    auto pump = [&](float dt, const std::function<bool()>& done) {
        for (int i = 0; i < 2000 && !done(); ++i) {
            hostSession.frame(dt);
            guestSession.frame(dt);
            Sleep(0);
        }
        return done();
    };
    bool known = false, setup = false, started = false, raced = false, states = false, finished = false,
         lobby = false;
    if (opened) {
        hostSession.start();
        guestSession.start();
        hostSession.addOwnPlayer(u"Host", 0);
        guestSession.addOwnPlayer(u"Guest", 0);
        hostSession.host(u"OJ session", 6);
        oj::net::Address at{loop, static_cast<uint16_t>((29667 >> 8) | ((29667 & 0xFF) << 8))};
        guestSession.join(at);
        known = pump(0.05f, [&] {
            const oj::net::NetPlayer* g = hostSession.player(hostLink.peers().empty() ? 0 : hostLink.peers()[0].id, 0);
            const oj::net::NetPlayer* h = guestSession.player(0, 0);
            return g != nullptr && h != nullptr && g->name == u"Guest" && h->name == u"Host";
        });
        hostSession.sendPlace("ANGEL SUPERSPEEDWAY");
        hostSession.sendRoute("SPEEDWAY_R1");
        hostSession.sendLaps(3);
        hostSession.setStage(oj::net::SessionStage::Cars);
        setup = pump(0.05f, [&] {
            return guestSession.stage() == oj::net::SessionStage::Cars && guestSession.route() == "SPEEDWAY_R1" &&
                   guestSession.laps() == 3;
        });
        oj::net::CarConfig car;
        std::memcpy(car.name.data(), "ECLIPSE", 7);
        guestSession.sendCar(car);
        guestSession.sendReady(1, 0);
        hostSession.sendReady(1, 0);
        started = pump(0.05f,
                       [&] {
                           return hostSession.stage() == oj::net::SessionStage::Loading &&
                                  guestSession.stage() == oj::net::SessionStage::Loading;
                       }) &&
                  hostSession.racing() == 2 && guestSession.racing() == 2;
        // The race made: each machine's cars.
        for (oj::net::NetPlayer& p : hostSession.players()) p.raceCar = p.peer == -1 ? static_cast<void*>(&hostCar) : &remoteOnHost;
        for (oj::net::NetPlayer& p : guestSession.players()) p.raceCar = p.peer == -1 ? static_cast<void*>(&guestCar) : &hostCar;
        const oj::net::NetPlayer* g = hostSession.player(hostLink.peers().empty() ? 0 : hostLink.peers()[0].id, 0);
        const bool carCame = g != nullptr && std::string(g->car.name.data()) == "ECLIPSE";
        guestSession.loaded();
        hostSession.loaded();
        raced = carCame && pump(0.05f, [&] {
                    return hostSession.stage() == oj::net::SessionStage::Racing &&
                           guestSession.stage() == oj::net::SessionStage::Racing;
                });
        states = pump(0.05f, [&] { return applied.size() == 0x40 && applied[1] == 1 && applied[0x3C] == 0x3C; });
        guestSession.finished(1, 2);
        hostSession.finished(3, 4);
        finished = pump(0.05f, [&] {
            const oj::net::NetPlayer* f = hostSession.player(hostLink.peers().empty() ? 0 : hostLink.peers()[0].id, 0);
            return f != nullptr && f->race.finished == 1 && f->race.finishTime == 1;
        });
        lobby = pump(0.1f, [&] {
            return hostSession.stage() == oj::net::SessionStage::Lobby &&
                   guestSession.stage() == oj::net::SessionStage::Lobby;
        });
    }
    check(opened && known && setup && started && raced && states && finished && lobby,
          "network session: names, setup, stages, start, count, car states, finishes, the lobby again");
}

// The fixed build's cars' states: every player's period 0.05 s (the stock's 0.1 s up to three players, 0.2 s with
// six), the peers pinged each second and their one way time from the pings (the stock's pings were never on: the
// states were never carried on by the way here).
OJ_INTEGRATION_TEST(Session, StatesSyncFixed) {
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
    oj::net::Transport hostLink, guestLink;
    std::string why;
    const bool opened = hostLink.open(options(29676, 29677, 29678), netRandom, &why) &&
                        guestLink.open(options(29678, 29679, 29676), netRandom, &why);
    oj::net::NetSession hostSession(hostLink), guestSession(guestLink);
    auto pump = [&](float dt, int frames, const std::function<bool()>& done) {
        for (int i = 0; i < frames && !done(); ++i) {
            hostSession.frame(dt);
            guestSession.frame(dt);
            Sleep(0);
        }
        return done();
    };
    bool known = false, periods = false, pinged = false;
    if (opened) {
        hostSession.start();
        guestSession.start();
        hostSession.addOwnPlayer(u"Host", 0);
        guestSession.addOwnPlayer(u"Guest", 0);
        hostSession.host(u"OJ session", 6);
        oj::net::Address at{loop, static_cast<uint16_t>((29677 >> 8) | ((29677 & 0xFF) << 8))};
        guestSession.join(at);
        known = pump(0.05f, 2000, [&] {
            return guestSession.player(0, 0) != nullptr && !hostLink.peers().empty() &&
                   hostSession.player(hostLink.peers()[0].id, 0) != nullptr;
        });
        periods = known && hostSession.players().size() == 2 && guestSession.players().size() == 2;
        for (const oj::net::NetPlayer& p : hostSession.players()) periods = periods && p.period == 0.05f;
        for (const oj::net::NetPlayer& p : guestSession.players()) periods = periods && p.period == 0.05f;
        // Three seconds of frames: three pings each way, their answers a frame or two later.
        pump(0.01f, 300, [] { return false; });
        const float hostToGuest = known ? hostSession.oneWay(hostLink.peers()[0].id) : 1.0f;
        const float guestToHost = guestSession.oneWay(0);
        pinged = hostToGuest < 0.025f && guestToHost < 0.025f && hostToGuest >= 0.0f && guestToHost >= 0.0f;
    }
    check(opened && known && periods && pinged,
          "network session (fixed): the states each 0.05 s, the peers' one way time from their pings");
}

// The fixed build's contacts: two sessions of the fixed build know each other by their hello (0x30) and a contact
// (0x31) comes to the other's hook; a machine that passes over 0x30 (as the stock's session, 0x004E96A0) is never
// marked and is never sent a contact.
OJ_INTEGRATION_TEST(Session, ContactsToFixedPeersOnly) {
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
    oj::net::Transport hostLink, guestLink, stockLink;
    std::string why;
    const bool opened = hostLink.open(options(29686, 29687, 29688), netRandom, &why) &&
                        guestLink.open(options(29688, 29689, 29686), netRandom, &why) &&
                        stockLink.open(options(29690, 29691, 29686), netRandom, &why);
    oj::net::NetSession hostSession(hostLink), guestSession(guestLink);
    stockLink.setHandler([](const uint8_t*, size_t, int32_t) {});  // a machine that passes over every message
    int32_t contactIndex = -1;
    std::array<float, 3> contactLinear{};
    guestSession.hooks.contact = [&](int32_t, int32_t localIndex, const std::array<float, 3>& linear,
                                     const std::array<float, 3>&) {
        contactIndex = localIndex;
        contactLinear = linear;
    };
    auto pump = [&](const std::function<bool()>& done) {
        for (int i = 0; i < 3000 && !done(); ++i) {
            hostSession.frame(0.01f);
            guestSession.frame(0.01f);
            stockLink.update(0.01f);
            Sleep(0);
        }
        return done();
    };
    bool known = false, sent = false, stockLeft = false;
    if (opened) {
        hostSession.start();
        guestSession.start();
        hostSession.addOwnPlayer(u"Host", 0);
        guestSession.addOwnPlayer(u"Guest", 0);
        hostSession.host(u"OJ session", 6);
        const oj::net::Address at{loop, static_cast<uint16_t>((29687 >> 8) | ((29687 & 0xFF) << 8))};
        guestSession.join(at);
        stockLink.join(at, 1);
        known = pump([&] {
            return hostLink.peers().size() == 2 && guestSession.fixedPeer(0) && !hostLink.peers().empty() &&
                   hostSession.fixedPeer(hostLink.peers()[0].id);
        });
        int32_t guestId = -1, stockId = -1;
        for (const oj::net::Peer& p : hostLink.peers()) {
            (hostSession.fixedPeer(p.id) ? guestId : stockId) = p.id;
        }
        pump([] { return false; });
        stockLeft = stockId != -1 && !hostSession.fixedPeer(stockId) &&
                    !hostSession.sendContact(stockId, 0, {1.0f, 2.0f, 3.0f}, {0.0f, 0.0f, 0.0f});
        sent = guestId != -1 && hostSession.sendContact(guestId, 0, {1500.0f, 0.0f, -250.0f}, {0.0f, 10.0f, 0.0f}) &&
               pump([&] { return contactIndex != -1; }) && contactIndex == 0 && contactLinear[0] == 1500.0f &&
               contactLinear[2] == -250.0f;
    }
    check(opened && known && stockLeft && sent,
          "network session (fixed): fixed peers know each other, a contact to one; none to a machine that passes over");
}

namespace {

// The loopback between the guest and the host through a relay holding each datagram `delay` seconds of the test's
// clock each way, as the internet does: the guest joins the relay's port, the host sees the guest at the relay's.
class LaggyRelay {
public:
    LaggyRelay(uint16_t hostPort, float delay) : m_hostPort(swap(hostPort)), m_delay(delay) {
        m_open = m_guestFace.open() && m_hostFace.open();
    }
    bool open() const { return m_open; }
    // The port the guest joins (host order).
    uint16_t port() const { return swap(m_guestFace.localPort()); }
    // The datagrams come since the last call queued, those due by `clock` sent on.
    void pump(float clock) {
        uint8_t data[2048];
        uint32_t ip = 0;
        uint16_t port = 0;
        for (int got; (got = m_guestFace.receive(data, sizeof data, ip, port)) >= 0;) {
            m_guestIp = ip;
            m_guestPort = port;
            m_queue.push_back({clock + m_delay, true, std::vector<uint8_t>(data, data + got)});
        }
        for (int got; (got = m_hostFace.receive(data, sizeof data, ip, port)) >= 0;) {
            std::vector<uint8_t> bytes(data, data + got);
            hideHost(bytes);
            m_queue.push_back({clock + m_delay, false, std::move(bytes)});
        }
        for (auto it = m_queue.begin(); it != m_queue.end();) {
            if (it->due > clock) {
                ++it;
                continue;
            }
            if (it->toHost) {
                m_hostFace.sendTo(0x0100007Fu, m_hostPort, it->bytes.data(), it->bytes.size());
            } else if (m_guestPort != 0) {
                m_guestFace.sendTo(m_guestIp, m_guestPort, it->bytes.data(), it->bytes.size());
            }
            it = m_queue.erase(it);
        }
    }

private:
    struct Datagram {
        float due;
        bool toHost;
        std::vector<uint8_t> bytes;
    };
    static uint16_t swap(uint16_t v) { return static_cast<uint16_t>((v >> 8) | (v << 8)); }
    // The host tells the guest where it is (a reliable 0xD5 holding the peer's 0xDF, its address at +0x0C, the
    // packet coded as the transport codes them): the relay's address in place of the host's own, so the guest goes on
    // talking through the relay.
    void hideHost(std::vector<uint8_t>& bytes) const {
        if (bytes.size() < 8 + 0x1C) return;
        oj::net::Transport::code(bytes.data(), bytes.size());
        uint8_t* address = bytes.data() + 8 + 0x0C;
        uint16_t port = 0;
        uint32_t ip = 0;
        std::memcpy(&port, address + 2, 2);
        std::memcpy(&ip, address + 4, 4);
        if (bytes[0] == 0xD5 && bytes[8] == 0xDF && ip == 0x0100007Fu && port == m_hostPort) {
            const uint16_t relay = m_guestFace.localPort();
            std::memcpy(address + 2, &relay, 2);
        }
        oj::net::Transport::code(bytes.data(), bytes.size());
    }
    oj::net::UdpSocket m_guestFace, m_hostFace;
    uint16_t m_hostPort;  // network order
    float m_delay;
    bool m_open = false;
    uint32_t m_guestIp = 0;
    uint16_t m_guestPort = 0;
    std::vector<Datagram> m_queue;
};

struct HeadOn {
    bool racing = false;
    float hostClosest = 1e9f, guestClosest = 1e9f;  // B's place along the straight less A's, on each machine
    float hostEnd = 0.0f, guestEnd = 0.0f;           // the same at the end
};

// Two machines racing head-on on Angel SuperSpeedway's straight, each car at 300 km/h (600 km/h closing), the
// network's datagrams `delay` seconds each way: each machine's world holds its own car (the throttle held) and the
// other's car, carried on and smoothed towards the states the session passes (TrackView::netHooks' hooks).
HeadOn headOnOnline(const oj::Cld& cld, const oj::Spl& route, const oj::CarDynamics& dynamics, float delay,
                    uint16_t firstPort) {
    HeadOn out;
    const uint32_t loop = 0x0100007Fu;
    auto options = [&](uint16_t lan, uint16_t gamePort, uint16_t to) {
        oj::net::Transport::Options o;
        o.lanPort = lan;
        o.gamePort = gamePort;
        o.ownIp = loop;
        o.broadcastIp = loop;
        o.broadcastPort = to;
        return o;
    };
    oj::CrtRandom netRandom;
    oj::net::Transport hostLink, guestLink;
    std::string why;
    const uint16_t hostGame = static_cast<uint16_t>(firstPort + 1);
    const bool opened =
        hostLink.open(options(firstPort, hostGame, static_cast<uint16_t>(firstPort + 2)), netRandom, &why) &&
        guestLink.open(options(static_cast<uint16_t>(firstPort + 2), static_cast<uint16_t>(firstPort + 3), firstPort),
                       netRandom, &why);
    LaggyRelay relay(hostGame, delay);
    oj::net::NetSession hostSession(hostLink), guestSession(guestLink);

    // Each machine: its world, its own car (A the host's, B the guest's) and the other's.
    // The worlds' turns of finding near bodies are the game's one count (0x0053F110's, a world a process): two
    // worlds of two bodies each, stepped in turn, would give each world's same body every turn. A third body,
    // parked far off, makes the count go round every body of both worlds, as each of two machines would.
    struct Machine {
        oj::PhysicsWorld world;
        oj::CarBody ownBody, otherBody, parked;
        oj::RaceCar own, other;
        Machine(const oj::Cld& c, const oj::CarDynamics& d)
            : world(c), ownBody(d, {}), otherBody(d, {}), parked(d, {0.0f, -100000.0f, 0.0f, 0.0f}) {
            world.add(ownBody);
            world.add(otherBody);
            world.add(parked);
            own.body = &ownBody;
            own.kind = oj::CarKind::Player;
            other.body = &otherBody;
            other.kind = oj::CarKind::Remote;
        }
    };
    Machine host(cld, dynamics), guest(cld, dynamics);
    static void (*const noHorn)(oj::RaceCar&, bool) = [](oj::RaceCar&, bool) {};
    for (auto [session, machine] : {std::pair{&hostSession, &host}, std::pair{&guestSession, &guest}}) {
        oj::PhysicsWorld* world = &machine->world;
        session->hooks.carState = [](void* car, uint8_t* state) {
            oj::net::carState(*static_cast<oj::RaceCar*>(car), false, false, state);
        };
        session->hooks.applyState = [world](void* car, const uint8_t* state) {
            oj::net::applyState(*static_cast<oj::RaceCar*>(car), state, world, noHorn);
        };
        session->hooks.smoothState = [world](void* car, const uint8_t* state, float dt) {
            oj::net::smoothState(*static_cast<oj::RaceCar*>(car), state, world, noHorn, dt);
        };
        session->hooks.extrapolate = [](uint8_t* state, float dt) { oj::net::extrapolate(state, dt); };
        session->hooks.remote = [](void* car) {
            return static_cast<oj::RaceCar*>(car)->kind == oj::CarKind::Remote;
        };
        session->hooks.racePaused = [] { return false; };
        session->hooks.carDistance = [](void*) { return 0.0f; };
        session->hooks.raceType = [] { return oj::RaceType::Circuit; };
    }
    float clock = 0.0f;
    auto frames = [&](float dt) {
        relay.pump(clock);
        hostSession.frame(dt);
        guestSession.frame(dt);
        clock += dt;
    };
    auto pump = [&](float dt, const std::function<bool()>& done) {
        for (int i = 0; i < 4000 && !done(); ++i) {
            frames(dt);
            Sleep(0);
        }
        return done();
    };
    if (!opened || !relay.open()) return out;
    hostSession.start();
    guestSession.start();
    hostSession.addOwnPlayer(u"Host", 0);
    guestSession.addOwnPlayer(u"Guest", 0);
    hostSession.host(u"OJ head-on", 6);
    guestSession.join({loop, static_cast<uint16_t>((relay.port() >> 8) | (relay.port() << 8))});
    // Each machine's player known with its name (its info; one coming after the start gets a spectator's start).
    const bool known = pump(0.05f, [&] {
        const oj::net::NetPlayer* g = hostSession.player(hostLink.peers().empty() ? 0 : hostLink.peers()[0].id, 0);
        const oj::net::NetPlayer* h = guestSession.player(0, 0);
        return g != nullptr && h != nullptr && g->name == u"Guest" && h->name == u"Host";
    });
    hostSession.sendPlace("ANGEL SUPERSPEEDWAY");
    hostSession.sendRoute("SPEEDWAY_R1");
    hostSession.sendLaps(3);
    hostSession.setStage(oj::net::SessionStage::Cars);
    oj::net::CarConfig car;
    std::memcpy(car.name.data(), "ECLIPSE", 7);
    const bool cars = known && pump(0.05f, [&] { return guestSession.stage() == oj::net::SessionStage::Cars; });
    guestSession.sendCar(car);
    guestSession.sendReady(1, 0);
    hostSession.sendReady(1, 0);
    const bool loading = cars && pump(0.05f, [&] {
        return hostSession.stage() == oj::net::SessionStage::Loading &&
               guestSession.stage() == oj::net::SessionStage::Loading && hostSession.racing() == 2 &&
               guestSession.racing() == 2;
    });
    for (oj::net::NetPlayer& p : hostSession.players()) p.raceCar = p.peer == -1 ? &host.own : &host.other;
    for (oj::net::NetPlayer& p : guestSession.players()) p.raceCar = p.peer == -1 ? &guest.own : &guest.other;
    guestSession.loaded();
    hostSession.loaded();
    out.racing = loading && pump(0.05f, [&] {
        return hostSession.stage() == oj::net::SessionStage::Racing &&
               guestSession.stage() == oj::net::SessionStage::Racing;
    });
    if (!out.racing) return out;
    // A on the first grid place, B 200 m ahead turned to face it; each machine places both the same, at 300 km/h.
    const oj::Spl::GridPoint& g = route.grid[0];
    const float fx = std::sin(g.heading), fz = std::cos(g.heading);
    const float speed = 300.0f / 3.6f;
    auto place = [&](Machine& m, oj::CarBody& body, float ahead, float heading) {
        int steps = 75;
        oj::settleOnGrid(body, m.world.pools(), {g.x + fx * ahead, g.y, g.z + fz * ahead, 0.0f}, heading, steps);
        body.setLocalVelocity({0.0f, 0.0f, speed, 0.0f});
    };
    place(host, host.ownBody, 0.0f, g.heading);
    place(host, host.otherBody, 20000.0f, g.heading + 3.14159265f);
    place(guest, guest.otherBody, 0.0f, g.heading);
    place(guest, guest.ownBody, 20000.0f, g.heading + 3.14159265f);
    auto along = [&](const oj::CarBody& b) { return b.position[0] * fx + b.position[2] * fz; };
    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < 60 * 3; ++i) {
        host.ownBody.throttle = 1.0f;
        guest.ownBody.throttle = 1.0f;
        host.world.update(dt);
        guest.world.update(dt);
        frames(dt);
        out.hostEnd = along(host.otherBody) - along(host.ownBody);
        out.guestEnd = along(guest.ownBody) - along(guest.otherBody);
        out.hostClosest = std::min(out.hostClosest, out.hostEnd);
        out.guestClosest = std::min(out.guestClosest, out.guestEnd);
    }
    return out;
}

}  // namespace

// The race head-on online on the loopback, the relay holding the datagrams 0, 30 and 55 ms each way (no ping; the
// 61 ms ping of the players' test; the 110 ms they see at most), the cars meeting where one world's touches stop them
// (CarPhysics.HeadOnAtSpeed's phases): where they end on each machine's screen. The stock's drew the other's car on
// through ours from 30 ms on (each machine's car met the other's where that one was a ping before). The fixed build
// carries each state on by the peer's one way time too: each machine's car meets the other's where it is, and both
// screens stop the cars at 0 and 30 ms. At 55 ms they meet at a phase of a step where one world's touches find
// nothing (CarPhysics.HeadOnAtSpeed): they go on through each other untouched on both screens (the hits told
// between the machines are TrackView's, not here).
OJ_INTEGRATION_TEST(Session, HeadOnAtSpeed) {
    const oj::CarDynamics* d = oj::test::eclipseDynamics();
    if (d == nullptr) return;
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    const std::filesystem::path base = std::filesystem::path("tracks") / "Angel SuperSpeedway";
    const auto cldBytes = game.read(base / "angel superspeedway_speedway_r1.cld");
    const auto splBytes = game.read(base / "angel superspeedway_speedway_r1.spl");
    const std::optional<oj::Cld> cld =
        cldBytes ? oj::parseCld(std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(cldBytes->data()),
                                                     reinterpret_cast<const uint8_t*>(cldBytes->data()) +
                                                         cldBytes->size()))
                 : std::nullopt;
    const oj::Spl route =
        splBytes ? oj::parseSpl(std::string(reinterpret_cast<const char*>(splBytes->data()), splBytes->size()))
                 : oj::Spl{};
    if (!cld || route.grid.empty()) {
        check(false, "Angel SuperSpeedway's collision and grid read");
        return;
    }
    oj::CarDynamics withShapes = *d;
    oj::loadShapes(withShapes.body, game, std::filesystem::path("cars") / "eclipse");
    uint16_t port = 29676;
    for (const float delay : {0.0f, 0.03f, 0.055f}) {
        const HeadOn r = headOnOnline(*cld, route, withShapes, delay, port);
        port = static_cast<uint16_t>(port + 4);
        const std::string ms = std::to_string(std::lround(delay * 1000.0f));
        std::printf("     head-on online at 300 km/h each, %3s ms each way: %s; the host's screen closest %.2f m, "
                    "end %.2f m; the guest's closest %.2f m, end %.2f m\n",
                    ms.c_str(), r.racing ? "raced" : "NOT RACING", r.hostClosest, r.hostEnd, r.guestClosest,
                    r.guestEnd);
        check(r.racing, "head-on online, " + ms + " ms each way: raced");
        if (!r.racing) continue;
        const std::string ends =
            " (the host's end " + std::to_string(r.hostEnd) + " m, the guest's " + std::to_string(r.guestEnd) + " m)";
        if (delay < 0.05f) {
            check(r.hostEnd > 2.0f && r.guestEnd > 2.0f,
                  "head-on online, " + ms + " ms each way: the cars stopped apart on both screens" + ends);
        } else {
            check((r.hostClosest > 1.0f && r.guestClosest > 1.0f) || (r.hostEnd < -100.0f && r.guestEnd < -100.0f),
                  "head-on online, " + ms + " ms each way: stopped, or through untouched at a step's phase that "
                  "finds no touch, on both screens" + ends);
        }
    }
}
