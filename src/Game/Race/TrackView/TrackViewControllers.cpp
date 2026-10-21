// The players' controllers during the race: their notes and the wheels' rumble.

#include "Game/Race/TrackView/TrackView.h"

#include "Core/Base/Log.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

namespace app {

using DriverSettings = oj::save::Settings;

bool TrackView::controllerWatch() {
    if (rules.mode == RaceMode::Attract) return false;
    if (!frontEnd.watchesControllers()) return false;
    bool noteOpens = true;  // [esp+0x34]
    const auto lost = [&](RacePlayer& player, int32_t other, const std::wstring& text) {
        if (replay.playing != 0) replay.timeScale = 1.0f;
        if (!frontEnd.openCritical(1, text)) return false;
        frontEnd.windowAsked.clear();
        if (!allFinished() && !rules.replay && !paused) {
            paused = true;
            frontEnd.askWindow(RaceWindowId::Pause);
        } else {
            oj::Jukebox& music = oj::jukebox();
            if (music.player() != nullptr) {
                music.paused = true;
                if (music.handle != 0) music.player()->pause(music.handle);
            }
        }
        if (rules.replay && frontEnd.critical != nullptr && !replay.done()) {
            replay.fast = 0;
            replay.timeScale = 1.0f;
            replay.state = oj::ReplayState::Stopped;
        }
        if (pausing == nullptr || pausing->index != other) {
            input->useUi(player.controller);
            pausing = &player;
        }
        return true;
    };
    if (frontEnd.uiFrame.connected) {
        if (!frontEnd.noControllerNote) {
            input->useDriver(first().controller);
            frontEnd.closeCritical(1);
            frontEnd.noControllerNote = true;
            frontEnd.noControllerNote2 = true;
            if (pausing == nullptr || pausing->index != 1) input->useUi(first().controller);
            if (rules.replay) {
                frontEnd.askWindow(RaceWindowId::Replay);
                input->useUi(first().controller);
            }
            noteOpens = false;
            oj::log::info("race: the controller back");
        }
    } else {
        if (frontEnd.noControllerNote && lost(first(), 1, std::wstring())) {
            frontEnd.noControllerNote = false;
            oj::log::info("race: the controller lost");
        }
        if (netClientNote()) return true;
    }
    if (rules.players <= 1 || players.size() < 2) return false;
    if (frontEnd.uiFrame2.connected) {
        if (!frontEnd.noControllerNote2) {
            frontEnd.closeCritical(1);
            frontEnd.noControllerNote2 = true;
            frontEnd.noControllerNote = true;
            oj::log::info("race: the second player's controller back");
        }
        return false;
    }
    if (!frontEnd.noControllerNote2 || !noteOpens) return false;
    if (lost(*players[1], 0, std::format(L"{}.", input->playerDevice(1) + 1))) {
        frontEnd.noControllerNote2 = false;
        oj::log::info("race: the second player's controller lost");
    }
    return false;
}

void TrackView::playerRumble(RacePlayer& player, float dt, bool wheel) {
    oj::ff::Rumble& r = input->rumble(player.controller);
    const oj::RaceCar& c = player.slot->car;
    if (c.wheelSmoke > 0.3f) r.smallAtLeast(c.wheelSmoke);
    if (c.wheelSkid > 0.3f) r.smallAtLeast(c.wheelSkid);
    if (c.wheelSlide > 0.05f) r.smallAtLeast(c.wheelSlide + 0.2f);
    const float forward = std::fabs(player.slot->body->localVelocity[2]);
    if (!wheel) {
        if (c.contactSide != 0) r.smallAtLeast(std::clamp(forward * 0.025f, 0.0f, 1.0f));
        r.largeAtLeast(c.impact);
        if (c.landed) r.largeAtLeast(1.0f);
    }
    if (c.nitroAsked) {
        const float look = rules.mode == RaceMode::Arcade && rules.arcadeNitro ? c.nitroLook * 0.2f : c.nitroLook;
        r.largeAtLeast(look);
        r.smallAtLeast(look);
    }
    if (!wheel) {
        const bool byHand =
            settings == nullptr || !settings->has_value() || (*settings)->u32(DriverSettings::kAutomatic) == 0;
        if (c.gearChange != 0 && forward > 2.0f && byHand) r.largeAtLeast(1.0f);
        const oj::Car& record = c.car != nullptr ? *c.car : player.slot->record;
        if (record.steeringPulls != 0 && forward > 10.0f) {
            player.rumbleTimer = dt + player.rumbleTimer;
            if (player.rumbleTimer > 3.0f) player.rumbleTimer = 0.0f;
            if (player.rumbleTimer > 1.5f) r.largeAtLeast(std::clamp((forward - 10.0f) * 0.04f, 0.0f, 0.7f));
        }
        if (c.rumbleShare > 0.0f) {
            r.smallAtLeast(c.rumbleShare);
            if (c.jolted) r.largeAtLeast(1.0f);
        }
    }
    r = {};
}

}  // namespace app
