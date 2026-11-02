// Integration tests: the player's car's touches with the other bodies (Game/Race/TrackView, TrackView::playerFrameEnd)
// off line and in the online sessions, with the game's Eclipse.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Network/Matchmaking.h"
#include "Core/Network/OnlineSessions.h"
#include "Core/Physics/CarBody.h"
#include "Core/Race/Racer.h"
#include "Game/Race/TrackView/TrackView.h"

#include <memory>
#include <string>

namespace {

using oj::test::check;

}  // namespace

// The end of the player's frame (0x0041F3F0): while the ghost time runs the car touches nothing; else off line it
// touches the others again; online it does when the session hosted has collisions (the arcade one's +0x34, the career
// one's +0x44, each 1) and without them it stays as it was made (0x00485F86 made it touching nothing).
OJ_INTEGRATION_TEST(TrackViewCollisions, PlayerFrameBySession) {
    const oj::CarDynamics* d = oj::test::eclipseDynamics();
    if (d == nullptr) return;
    auto view = std::make_unique<app::TrackView>();
    app::RaceSlot slot;
    slot.body = std::make_unique<oj::CarBody>(*d, oj::Vec4{});
    slot.car.kind = oj::CarKind::Player;
    slot.car.body = slot.body.get();
    app::RacePlayer& player = view->first();
    player.slot = &slot;
    oj::net::OnlineHosted& hosted = oj::net::onlineHosted();
    const oj::net::OnlineHosted had = hosted;
    // The car's touches after the frame, as they were before it and with the ghost's time.
    auto after = [&](bool before, float ghostTime) {
        slot.body->collides = before;
        slot.car.ghostTime = ghostTime;
        view->playerFrameEnd(player);
        return slot.body->collides;
    };
    check(after(false, 0.0f) && !after(true, 1.5f), "off line: the car touches the others, not while a ghost");
    oj::net::makeMatchmaking();
    struct Session {
        oj::net::OnlineKind mode;
        int32_t collision;
        const char* name;
    };
    for (const Session s : {Session{oj::net::OnlineKind::Arcade, 1, "online arcade with collisions"},
                            Session{oj::net::OnlineKind::Arcade, 0, "online arcade without collisions"},
                            Session{oj::net::OnlineKind::Career, 1, "online career with collisions"},
                            Session{oj::net::OnlineKind::Career, 0, "online career without collisions"}}) {
        hosted = had;
        hosted.mode = s.mode;
        // The other kind's session the opposite: only the online kind's is read.
        hosted.arcade.collision = s.mode == oj::net::OnlineKind::Arcade ? s.collision : 1 - s.collision;
        hosted.career.collision = s.mode == oj::net::OnlineKind::Career ? s.collision : 1 - s.collision;
        const bool fromOff = after(false, 0.0f);
        const bool fromOn = after(true, 0.0f);
        const bool ghost = after(true, 1.5f);
        if (s.collision == 1) {
            check(fromOff && fromOn && !ghost, std::string(s.name) + ": the car touches the others, not while a ghost");
        } else {
            check(!fromOff && fromOn && !ghost,
                  std::string(s.name) + ": the car's touches stay as they were, none while a ghost");
        }
    }
    oj::net::releaseMatchmaking();
    hosted = had;
    player.slot = nullptr;
}
