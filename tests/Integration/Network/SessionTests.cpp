// Integration tests: Session.

#include "Framework/Test.h"

#include "Core/Base/Random.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/NetTransport.h"

#include <windows.h>

#include <cstring>
#include <functional>
#include <string>
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
