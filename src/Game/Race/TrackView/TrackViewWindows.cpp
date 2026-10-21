// The race's side of its front end (Game/Race/FrontEnd): the front end's frame between the race's watches, what its
// windows ask of the race, the window the race asks for and what the HUD reads of the race.

#include "Game/Race/TrackView/TrackView.h"

#include "Core/Base/GameFiles.h"
#include "Core/Base/Log.h"
#include "Core/Data/GameTexts.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/OnlineRecords.h"
#include "Core/Data/JetTexts.h"

#include <windows.h>

#include <utility>

namespace app {

void TrackView::frontEndFrame(float dt) {
    netFrameEnded = false;
    netBeforeWindows(dt);
    if (controllerWatch()) {
        netFrameEnded = true;
        return;
    }
    frontEnd.windowsFrame(dt);
    // 0x00457E28: after the window manager's frame the network's answers, its loss and the account signed on
    // elsewhere.
    if (netAfterWindows()) {
        netFrameEnded = true;
        return;
    }
    frontEnd.playerMessages(dt, players.size());
}

void TrackView::hideFollowed(const RacePlayer& player) {
    const int mode = player.raceCamera.mode();
    const bool onCar = mode == oj::scene::RaceCamera::kBumper || mode == oj::scene::RaceCamera::kBonnet;
    for (std::unique_ptr<RaceSlot>& s : cars) {
        s->model->hidden = onCar && s.get() == followedSlot(player);
        if (s->shadow != nullptr) s->shadow->on = !s->model->hidden;
        for (oj::scene::Silhouette* silhouette : s->silhouettes) {
            if (silhouette != nullptr) silhouette->on = !s->model->hidden;
        }
        if (!s->model->hidden) continue;
        for (size_t i = 0; i < 2; ++i) {
            if (s->car.exhausts[i] && s->particles.flames[i]) s->particles.flames[i]->setLevel(0.0f);
        }
    }
}

void TrackView::goOn(int player) {
    paused = false;
    pausing = nullptr;  // 0x0044EDF5: the race track's pausing player (+0x10) cleared
    first().director.period = first().director.time < 2.0f ? 2.0f - first().director.time : 0.0f;
    first().director.cutDue = true;
    chooseWindow(true);
    for (const std::unique_ptr<RacePlayer>& p : players) {
        if (player < 0 || p->index == player) input->useDriver(p->controller);
    }
    oj::log::info("race: goes on");
}

void TrackView::followWindow(const oj::RaceWindowFrame& f) {
    if (f.netEnd != 0) {
        if (oj::net::NetGame* n = oj::net::netGame()) {
            if (f.netEnd == 1) {
                n->session.reset(true);  // 0x004EA4E0 with 1
                if (oj::net::Matchmaking* mm = oj::net::matchmaking()) mm->stopHosting();  // 0x004E2740
            } else {
                n->session.setStage(oj::net::SessionStage::Lobby);  // 0x004EA850, every machine back to the lobby
            }
        }
    }
    if (f.useUi) input->useUi();
    if (f.stopRecord) replay.stop();
    if (f.replayOff) {
        replay.playing = 0;
        syncReplayFlags();
    }
    if (f.highScores) frontEnd.newHighScore();
    if (f.goOn) goOn(f.player);
    if (!f.onlineWindow.empty()) oj::Frontend::sharedState().onlineForms.background = f.onlineWindow;
    if (_stricmp(f.window.c_str(), "savereplay") == 0) {
        // 0x0044E2F0: savereplay over the window (mode 5).
        if (gameData != nullptr) frontEnd.saveReplay.enter(*gameData, saveFolder);
    } else if (!f.window.empty()) {
        frontEnd.askWindow(f.window);
    }
    if (!f.question.empty()) frontEnd.raceMessage(oj::ui::MessageType::YesNo, f.question, f.answer, f.startNo);
}

void TrackView::chooseWindow(bool asked) {
    // 0x004584C0 on the network only watched: hud_spectator alone (no message up, not paused, no window waiting).
    if (net != nullptr && rules.spectator) {
        if (frontEnd.netMessage == nullptr && !paused && frontEnd.windowAsked.empty())
            frontEnd.askWindow(RaceWindowId::Spectator);
        return;
    }
    if (rules.replayRace) {
        frontEnd.askWindow(RaceWindowId::Replay);
        return;
    }
    // In a split screen race (mode 3), whatever was asked: with every player finished hud_race_results; else while
    // the first player's camera plays the fly-by or the sweep before the start (modes 9, 10) blank_split, else
    // hud_split.
    if (rules.mode == RaceMode::SplitScreen) {
        if (allFinished()) {
            frontEnd.askWindow(RaceWindowId::RaceResults);
        } else if (first().introPlaying ||
                   first().raceCamera.mode() == oj::scene::RaceCamera::kBeforeStart) {
            frontEnd.askWindow(RaceWindowId::BlankSplit);
        } else {
            frontEnd.askWindow(RaceWindowId::HudSplit);
        }
        return;
    }
    if (!first().finished && !asked) {
        frontEnd.askWindow(RaceWindowId::Blank);
        return;
    }
    const RaceType type = rules.type;
    const bool completed = std::exchange(first().raceComplete, false);
    if (type == RaceType::Cruise) {
        // A network cruise goes to its results at once.
        frontEnd.askWindow(completed ? (net != nullptr ? RaceWindowId::CruiseResults : RaceWindowId::RaceComplete)
                           : first().finished ? RaceWindowId::CruiseResults
                                              : RaceWindowId::HudCruise);
        // With the player finished, the arcade's result (0x0042B290 with 100).
        if (!completed && first().finished && race != nullptr) race->arcadeResult(100);
    } else if (type == RaceType::Circuit || type == RaceType::PointToPoint || type == RaceType::Unused8) {
        if (completed) {
            frontEnd.askWindow(RaceWindowId::RaceComplete);
        } else if (first().finished) {
            frontEnd.askWindow(RaceWindowId::RaceResults);
        } else {
            frontEnd.windowAsked.clear();
            frontEnd.askWindow(RaceWindowId::HudRace);
        }
    } else if (type == RaceType::Sprint) {
        if (completed || first().finished) {
            frontEnd.askWindow(RaceWindowId::SprintResults);
        } else {
            frontEnd.windowAsked.clear();
            frontEnd.askWindow(RaceWindowId::HudSprint);
        }
    } else if (completed) {
        frontEnd.askWindow(RaceWindowId::RaceComplete);
    } else if (first().finished) {
        frontEnd.askWindow(type == RaceType::Solo ? RaceWindowId::RaceResults : RaceWindowId::ChallengeResults);
    } else {
        frontEnd.askWindow(RaceWindowId::HudSolo);
    }
}

oj::HudRace TrackView::hudRace() {
    oj::HudRace r;
    r.race = race.get();
    r.mode = rules.mode;
    r.type = rules.type;
    r.challengeLapTime = rules.lapTime;
    r.challengeSpeed = rules.speed;
    // The outcome the race's frame sets (the race's own rules).
    r.challenge = race != nullptr ? race->rules().challenge : rules.challenge;
    r.heat = race != nullptr ? race->rules().heat : rules.heat;
    r.heats = race != nullptr ? race->rules().heats : rules.heats;
    r.shiftedUp = first().control.shiftedUp;
    r.session = rules.session;
    // [0x0075EF8C]: the network game, this machine hosting it, only watching it, its session.
    if (net != nullptr) {
        r.networked = true;
        r.spectator = rules.spectator;
        r.net = net;
        if (const oj::net::NetGame* n = oj::net::netGame()) r.host = n->transport.hosting();
    }
    if (rules.settings != nullptr && rules.settings->has_value()) {
        r.settings = &**rules.settings;
    } else {
        // The options' own settings before a driver is made or loaded.
        static const oj::save::Settings defaults = oj::save::Settings::defaults(frontEnd.hudLanguage->index);
        r.settings = &defaults;
    }
    // The route's lap record (the records object [G+0xA8]).
    if (rules.career != nullptr) r.cruise = &rules.career->cruise;
    if (gameData != nullptr) {
        const int index = gameData->tables.routeIndex(trackName, routeLine);
        r.lapRecord = gameData->highScores.time(index);
        r.highScores = &gameData->highScores;
        r.routeIndex = index;
        r.takeFirstComplete = [this] { return std::exchange(first().raceComplete, false); };
        r.lapTimeStatistics = [this](size_t player, bool pause) {
            if (oj::net::stats::gameStatistics() == nullptr || (pause && rules.replayRace)) return;
            if (player >= players.size() || players[player]->slot == nullptr) return;
            if (pause && rules.type != RaceType::Circuit && rules.type != RaceType::Solo &&
                rules.type != RaceType::Sprint && rules.type != RaceType::TestTrack)
                return;
            if (lapTimeStatistics(*players[player])) oj::net::stats::reporter()->send();
        };
        r.tables = &gameData->tables;
        r.driverName = gameData->profileName;
        r.saving = gameData->saving != 0;
        if (gameData->career) {
            r.raceCarsChosen = gameData->career->profile.raceCarsChosen != 0;
            r.career = gameData->career.get();
        }
        r.catalogue = &gameData->catalogue;
        r.directorTime = &first().director.time;
    }
    // Each player's own car, else (a watched race) the first player's camera's car (0x0044B41C).
    if (first().slot != nullptr) {
        for (const std::unique_ptr<RacePlayer>& p : players) {
            if (p->slot != nullptr) r.players.push_back({&p->slot->car, &p->slot->record, true});
        }
    } else if (watching && followedSlot(first()) != nullptr) {
        r.players.push_back({&followedSlot(first())->car, &followedSlot(first())->record, false});
    }
    if (followedSlot(first()) != nullptr) r.cameraCar = &followedSlot(first())->car;
    r.pausing = pausing != nullptr ? pausing->index : -1;
    r.route = routeLine;
    for (const std::unique_ptr<RaceSlot>& s : cars) r.cars.push_back(&s->car);
    return r;
}

}  // namespace app
