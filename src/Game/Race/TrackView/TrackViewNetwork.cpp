// The network race: its hooks into the session, its notes, the watched racer and its overlay.

#include "Game/Race/TrackView/TrackView.h"

#include "Core/Base/Log.h"
#include "Core/Base/Text.h"
#include "Core/Base/X87.h"
#include "Core/Career/Money.h"
#include "Core/Data/GameTexts.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/NetTransport.h"
#include "Core/Network/VoiceChat.h"
#include "Core/Data/JetTexts.h"

#include <windows.h>

#include <algorithm>
#include <limits>

namespace app {

bool TrackView::netSweep(RacePlayer& player) {
    if (net == nullptr || rules.type == RaceType::Solo || rules.type == RaceType::Cruise) return false;
    if (hasCamera()) {
        player.raceCamera.take(oj::scene::RaceCamera::kBeforeStart, true, cameraTarget(player), cameraRace(player),
                               player.camera);
    }
    return true;
}

void TrackView::netDetach() {
    if (net != nullptr) {
        for (oj::net::NetPlayer& p : net->players()) p.raceCar = nullptr;
        net->hooks.carState = nullptr;
        net->hooks.applyState = nullptr;
        net->hooks.smoothState = nullptr;
        net->hooks.extrapolate = nullptr;
        net->hooks.putBack = nullptr;
        net->hooks.carDistance = nullptr;
        net->hooks.carByte = nullptr;
        net->hooks.setCarByte = nullptr;
        net->hooks.carGone = nullptr;
        net->hooks.remote = nullptr;
        net->hooks.raceType = nullptr;
        net->hooks.racePaused = nullptr;
        net->hooks.hud = nullptr;
        net->hooks.firstPlayer = nullptr;
        net->hooks.careerPinkSlip = nullptr;
        net->hooks.firstPlayerFinishCleared = nullptr;
        net->hooks.joined = nullptr;
        net->hooks.left = nullptr;
        net->hooks.firstFinished = nullptr;
        net->hooks.leader = nullptr;
        net->hooks.countdown = nullptr;
        net->hooks.cruiseScore = nullptr;
        net->hooks.cruiseTrick = nullptr;
        if (rules.career != nullptr) rules.career->cruise.net = oj::CruiseNet{};
    }
    net = nullptr;
    netLoaded = false;
}

void TrackView::drawNetOverlay() {
    if (net == nullptr || frontEnd.hudDevice == nullptr || frontEnd.hudLanguage == nullptr ||
        frontEnd.hudTexts == nullptr)
        return;
    if (!netOverlay) netOverlay = std::make_unique<oj::ui::Screen>(*frontEnd.hudDevice, *frontEnd.hudLanguage);
    oj::ui::Screen& s = *netOverlay;
    const float w = static_cast<float>(frontEnd.hudDevice->width());
    const float h = static_cast<float>(frontEnd.hudDevice->height());
    const char* font = "goodtime11";
    const oj::net::SessionStage stage = net->stage();
    if (stage > oj::net::SessionStage::Loading && stage < oj::net::SessionStage::Racing) {
        const std::wstring& text = frontEnd.hudTexts->at(oj::kTextWaitingForPlayers);
        s.drawFreeText(font, text, w * 0.5f - s.freeTextWidth(font, text) * 0.5f, h * 0.5f, 0xFFFFFF00u);
    }
    if (!(0.0f < namesTime()) && !rules.spectator) return;
    const RaceSlot* watched = followedSlot(first());
    for (const std::unique_ptr<RaceSlot>& slot : cars) {
        const oj::RaceCar& car = slot->car;
        if ((car.kind != oj::CarKind::Remote && car.kind != oj::CarKind::Ai) || car.parked) continue;
        std::wstring name;
        for (const oj::net::NetPlayer& p : net->players()) {
            if (p.raceCar == &car) name = std::wstring(p.name.begin(), p.name.end()).substr(0, 15);
        }
        if (car.netPaused) name += frontEnd.hudTexts->at(oj::kTextPaused);
        float x = 0.0f;
        float y = 0.0f;
        float alpha = 1.0f;
        if (rules.spectator && slot.get() == watched) {
            x = w * 0.5f;
            y = 0.16f * h;
        } else {
            const oj::Vec4& p = slot->body->position;
            const oj::Mat44& m = first().camera.worldToClip();
            const float px = p[0] * 100.0f, py = p[1] * 100.0f + 120.0f, pz = p[2] * 100.0f;
            const float cx = ((m[0] * px + m[2] * pz) + m[1] * py) + m[3];
            const float cy = ((m[4] * px + m[6] * pz) + m[5] * py) + m[7];
            const float cw = ((m[12] * px + m[14] * pz) + m[13] * py) + m[15];
            if (!(cw > 0.0f)) continue;
            x = (cx / cw * 0.5f + 0.5f) * w;
            y = (0.5f - cy / cw * 0.5f) * h;
            alpha = 0.0f < cw && cw < 10000.0f ? 1.0f - cw / 10000.0f : 0.0f;
        }
        const auto a = static_cast<uint32_t>(oj::x87::chop(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)) & 0xFF;
        const uint32_t colour = (a << 24) | 0x00FFFF00u;
        const float half = s.freeTextWidth(font, name) * 0.5f;
        s.drawFreeText(font, name, x - half, y, colour);
        oj::net::VoiceChat* voice = oj::net::gameVoice();
        oj::net::OnlineUser* user = oj::net::onlineUser();
        if (voice == nullptr || user == nullptr) continue;
        const std::vector<oj::net::NetPlayer>& netPlayers = net->players();
        size_t i = 0;
        while (i < netPlayers.size() && netPlayers[i].raceCar != &car) ++i;
        if (i == netPlayers.size()) continue;
        const int32_t profile = netPlayers[i].profile;
        user->userAddRef(profile);
        if (profile == -1) continue;
        const int32_t firstPlayer = user->localPlayers.empty() ? -1 : user->localPlayers[0];
        const oj::net::VoiceIcons& icons = oj::net::voiceIcons();
        const std::string* icon = nullptr;
        if (voice->isMuted(firstPlayer, profile)) {
            icon = &icons.mute;
        } else if (const oj::net::VoiceChat::RemoteUser* r = voice->findUser(profile); r != nullptr && r->hasVoice != 0) {
            icon = voice->userTalking(profile) ? &icons.on : &icons.off;
        }
        if (icon == nullptr || icon->empty()) continue;
        const float x1 = half + x + 5.0f;
        s.drawFreePicture(*icon, x1, y - 25.0f, x1 + 25.0f, y, colour);
    }
}

void TrackView::netBeforeWindows(float dt) {
    oj::net::NetGame* n = oj::net::netGame();
    if (n == nullptr) return;
    if (netHostWait && netHostTime > 0.0f) netHostTime -= dt;
    if (netNoteAnswer != 0) {
        if (frontEnd.window.id != RaceWindowId::Closedown) {
            frontEnd.message.reset();
            frontEnd.askWindow(RaceWindowId::Closedown);
        }
        netNoteAnswer = 0;
    }
    if (oj::Frontend::sharedState().onlineForms.friendJoining == 0 && n->transport.inSession() &&
        n->session.stage() < oj::net::SessionStage::Loading && frontEnd.critical == nullptr &&
        frontEnd.netMessage == nullptr) {
        frontEnd.message.reset();
        if (frontEnd.window.id != RaceWindowId::Autosave) {
            input->useUi();
            if (frontEnd.window.id != RaceWindowId::Closedown) frontEnd.askWindow(RaceWindowId::Closedown);
        }
    } else if (n->transport.state() == oj::net::State::HostTimedOut && frontEnd.netMessage == nullptr) {
        frontEnd.windowAsked.clear();
        frontEnd.message.reset();
        if (!allFinished() && !rules.replayRace && !paused) frontEnd.askWindow(RaceWindowId::Pause);
        n->session.reset(false);
        input->useUi();
        frontEnd.netNote(oj::ui::MessageType::Continue, frontEnd.hudLine(oj::kTextHostFailed), &netNoteAnswer);
    }
    if (n->transport.state() == oj::net::State::HostLeft && frontEnd.netMessage == nullptr) {
        n->session.reset(false);
        input->useUi();
        frontEnd.netNote(oj::ui::MessageType::Continue, frontEnd.hudLine(oj::kTextHostLeft), &netNoteAnswer);
    }
}

bool TrackView::netClientNote() {
    oj::net::NetGame* n = oj::net::netGame();
    if (frontEnd.noControllerNote || n == nullptr || n->session.stage() >= oj::net::SessionStage::Loading ||
        n->transport.hosting())
        return false;
    if (frontEnd.netMessage != nullptr) return false;
    frontEnd.netNote(oj::ui::MessageType::Continue, frontEnd.hudLine(oj::kTextHostFailed), &netClientAnswer);
    return true;
}

bool TrackView::netAfterWindows() {
    oj::net::NetGame* n = oj::net::netGame();
    if (netErrorAnswer != 0 && !netQuitting) {
        bool quits = true;
        netLostFrames = 0;
        netQuitting = true;
        if (oj::net::matchmaking() != nullptr) {
            oj::log::info("race: Quitting online race");
            if (n != nullptr) n->session.reset(false);
            if (oj::net::netGame() != nullptr) oj::net::endNetGame();
            oj::net::endOnline();
        } else if (n != nullptr) {
            oj::log::info("race: Quitting link-up race");
            n->session.reset(true);
            oj::net::endNetGame();
            if (oj::net::onlineUser() != nullptr) {
                oj::net::endOnline();
            } else {
                oj::net::stopWinsock();
            }
        } else {
            oj::log::info("race: Closing network during single player race");
            quits = false;
            if (oj::net::onlineUser() != nullptr) {
                oj::net::endOnline();
                netErrorAnswer = 0;
            }
        }
        frontEnd.netMessage.reset();
        if (quits) {
            input->useUi();
            if (gameData != nullptr) gameData->afterRaceWindow = "menu";
            if (frontEnd.window.id != RaceWindowId::Closedown) frontEnd.askWindow(RaceWindowId::Closedown);
            return true;
        }
    }
    if (netClientAnswer != 0 && frontEnd.noControllerNote) {
        netClientAnswer = 0;
        bool quits = true;
        if (oj::net::matchmaking() != nullptr) {
            oj::log::info("race: Quitting online race");
            if (n != nullptr) n->session.reset(false);
            if (oj::net::netGame() != nullptr) oj::net::endNetGame();
            oj::net::releaseMatchmaking();
            if (gameData != nullptr) gameData->afterRaceWindow = "online_main";
        } else if (n != nullptr) {
            oj::log::info("race: Quitting link-up race");
            n->session.reset(true);
            oj::net::endNetGame();
            if (gameData != nullptr) gameData->afterRaceWindow = "link_search";
        } else {
            oj::log::info("race: Closing network during single player race");
            quits = false;
        }
        frontEnd.netMessage.reset();
        if (quits) {
            frontEnd.message.reset();
            input->useUi();
            if (frontEnd.window.id != RaceWindowId::Closedown) frontEnd.askWindow(RaceWindowId::Closedown);
            return true;
        }
    }
    oj::net::OnlineUser* user = oj::net::onlineUser();
    const int32_t userFrame = user != nullptr ? user->networkFrame() : 0;
    if (oj::net::winsockStarted() && frontEnd.watchesControllers()) {
        if (oj::net::networkDown() && userFrame == 0) {
            if (netNoteHeld == 0) {
                if (netLostFrames > 0 && frontEnd.netMessage == nullptr && netErrorAnswer == 0) {
                    frontEnd.windowAsked.clear();
                    if (frontEnd.netNote(oj::ui::MessageType::ContinueModeless, frontEnd.hudLine(oj::kTextNetworkError),
                                         &netErrorAnswer) != 0) {
                        if (!first().finished && !rules.replayRace) frontEnd.askWindow(RaceWindowId::Pause);
                        input->useUi();
                    }
                    if (oj::net::NetGame* game = oj::net::netGame(); game != nullptr && game->transport.hosting()) {
                        if (!netHostWait) netHostTime = 9.0f;
                        netHostWait = true;
                    }
                } else {
                    ++netLostFrames;
                }
            }
        } else {
            if (netHostWait && !(netHostTime > 0.0f)) {
                if (oj::net::NetGame* game = oj::net::netGame()) game->session.reset(false);
            }
            if (user == nullptr || (oj::net::netGame() != nullptr && oj::net::matchmaking() == nullptr)) {
                netBack = {-1, -1};
                netLostFrames = 0;
                netNoteHeld = 0;
                netHostWait = false;
            }
        }
    }
    if (user != nullptr && (frontEnd.message == nullptr || frontEnd.messageNumber == 0) &&
        frontEnd.watchesControllers() && user->state == oj::net::UserState::ForcedDisconnect &&
        frontEnd.netMessage == nullptr) {
        frontEnd.windowAsked.clear();
        if (!allFinished() && !rules.replayRace && !paused) frontEnd.askWindow(RaceWindowId::Pause);
        if (rules.replayRace && !replay.done()) {
            replay.fast = 0;
            replay.timeScale = 1.0f;
            replay.state = oj::ReplayState::Stopped;
        }
        if (pausing == nullptr || pausing->index != 1) {
            input->useUi(first().controller);
            pausing = &first();
        }
        frontEnd.netNote(oj::ui::MessageType::Continue, frontEnd.jetText(oj::kJetSignedOnElsewhere), &netErrorAnswer);
    }
    return false;
}

void TrackView::netWatchNext(bool select) {
    std::vector<oj::net::NetPlayer>& netPlayers = net->players();
    if (netPlayers.empty()) return;
    const RaceSlot* on = followedSlot(first());
    size_t u = 0;
    for (size_t i = 0; i < netPlayers.size(); ++i) {
        if (on != nullptr && netPlayers[i].raceCar == &on->car) {
            u = i;
            break;
        }
    }
    if (netPlayers[u].race.left == 0 && !select) return;
    for (size_t k = 1; k <= netPlayers.size(); ++k) {
        const oj::net::NetPlayer& p = netPlayers[(u + k) % netPlayers.size()];
        if (p.race.notRacing != 0 || p.race.left != 0 || p.raceCar == nullptr) continue;
        for (std::unique_ptr<RaceSlot>& s : cars) {
            if (&s->car != p.raceCar) continue;
            first().viewed = s.get();
            first().raceCamera.newCar();
            cameraTakes(first(), s.get());
            return;
        }
    }
    if (!first().director.active) {
        first().director.start(first().raceCamera, 13, std::numeric_limits<float>::max(), cameraTarget(first()),
                               cameraRace(first()), first().camera);
    }
}

void TrackView::netHooks(oj::net::NetSession& session) {
    net = &session;
    netLoaded = false;
    session.hooks.carState = [this](void* car, uint8_t* state) {
        const auto& c = *static_cast<oj::RaceCar*>(car);
        oj::net::carState(c, paused, c.hornSounding, state);
    };
    session.hooks.applyState = [this](void* car, const uint8_t* state) {
        oj::net::applyState(*static_cast<oj::RaceCar*>(car), state, world.get(), &netHorn);
    };
    session.hooks.smoothState = [this](void* car, const uint8_t* state) {
        oj::net::smoothState(*static_cast<oj::RaceCar*>(car), state, world.get(), &netHorn);
    };
    session.hooks.extrapolate = [](uint8_t* state, float dt) { oj::net::extrapolate(state, dt); };
    session.hooks.putBack = [this](void* car) {
        if (race != nullptr && world != nullptr) oj::respawn(*static_cast<oj::RaceCar*>(car), race->rules(), world->pools());
    };
    session.hooks.carDistance = [](void* car) {
        const auto& c = *static_cast<oj::RaceCar*>(car);
        return c.competitor != nullptr ? c.competitor->raceDistance : 0.0f;
    };
    session.hooks.carByte = [](void* car) { return static_cast<int32_t>(static_cast<oj::RaceCar*>(car)->modelParts); };
    session.hooks.setCarByte = [](void* car, int32_t value) {
        static_cast<oj::RaceCar*>(car)->modelParts = static_cast<uint8_t>(value);
    };
    session.hooks.carGone = [](void* car) {
        auto& c = *static_cast<oj::RaceCar*>(car);
        c.parked = true;
        c.netPaused = false;
    };
    session.hooks.remote = [](void* car) { return static_cast<oj::RaceCar*>(car)->kind == oj::CarKind::Remote; };
    session.hooks.raceType = [this] { return rules.type; };
    session.hooks.racePaused = [this] { return paused; };
    // The race front end there ([G+0x3C]) and its notes to the first player's messages, with no colour of their
    // own: a player joined (0x004E8B50, "Spectator <name> has joined", 2 s), gone (0x004E88E0, "Spectator " for
    // one not racing, "<name> has left", 2 s), the first finish (0x004E86C0, "WINNER: <name>", 2 s), the leader
    // (0x004E85C0, "NEW LEADER: <name>" told by the leader's message, else "1st: <name>", 2 s), the stage's wait
    // (0x004EADE0, "Race ends in <60 or 10> seconds", 3 s).
    session.hooks.hud = [this] { return frontEnd.hud != nullptr; };
    session.hooks.firstPlayer = [this](bool& winner) {
        if (first().slot == nullptr) return false;
        const oj::Competitor* c = first().slot->car.competitor;
        winner = c != nullptr && c->finished && c->place == 1;
        return true;
    };
    // 0x004E7EB0's: [G+0x80]+0x90, and the race track's first player's +0x1C taken (made 0) with its +0x18.
    session.hooks.careerPinkSlip = [this] { return rules.career != nullptr && rules.career->onlinePinkSlip != 0; };
    session.hooks.firstPlayerFinishCleared = [this] {
        RacePlayer& p = first();
        const bool complete = p.raceComplete;
        p.raceComplete = false;
        return !complete && !p.finished;
    };
    const auto note = [this](const std::wstring& text, float seconds) {
        if (frontEnd.hud != nullptr) frontEnd.hud->messages(0).push(text, seconds);
    };
    const auto line = [this](size_t index) {
        return frontEnd.hudTexts != nullptr && index < frontEnd.hudTexts->size() ? frontEnd.hudTexts->at(index)
                                                                                 : std::wstring();
    };
    const auto name = [](const oj::net::NetPlayer& p) { return std::wstring(p.name.begin(), p.name.end()); };
    session.hooks.joined = [note, line, name](const oj::net::NetPlayer& p) {
        note(line(oj::kTextSpectator) + name(p) + line(oj::kTextHasJoined), 2.0f);
    };
    session.hooks.left = [note, line, name](const oj::net::NetPlayer& p) {
        const std::wstring spectator = p.race.notRacing != 0 ? line(oj::kTextSpectator) : std::wstring();
        note(spectator + name(p) + line(oj::kTextHasLeft), 2.0f);
    };
    session.hooks.firstFinished = [note, line, name](const oj::net::NetPlayer& p) {
        note(line(oj::kTextWinner) + L": " + name(p), 2.0f);
    };
    session.hooks.leader = [note, line, name](const oj::net::NetPlayer& p, bool fromLeader) {
        note((fromLeader ? line(oj::kTextNewLeader) : L"1" + line(oj::kTextOrdinals)) + L": " + name(p), 2.0f);
    };
    session.hooks.countdown = [note, line](uint32_t seconds) {
        note(line(oj::kTextRaceEndsIn) + std::to_wstring(seconds) + line(oj::kTextSeconds), 3.0f);
    };
    // The cruise on the network: our score (0x19, not reliable, [4] 0), a trick (0x1A), its end (0x004AB460) and
    // the board (0x004B0A50), also each time a player's score comes (0x004E9650).
    session.hooks.cruiseScore = [this](const oj::net::NetPlayer&) { netCruiseBoard(); };
    // 0x004E9680 -> 0x004B0E90: another player's trick: "<name> <the trick's name>" (339 + trick; past 100 the
    // special move's, 359 + trick - 100) as the first player's note for 2.5 s while the race front end is there.
    session.hooks.cruiseTrick = [note, line, name](const oj::net::NetPlayer& p, int32_t trick) {
        const int64_t text = trick > 100 ? 359 + static_cast<int64_t>(trick) - 100 : 339 + static_cast<int64_t>(trick);
        note(name(p) + L" " + (text >= 0 ? line(static_cast<size_t>(text)) : std::wstring()), 2.5f);
    };
    if (rules.career != nullptr) {
        oj::CruiseNet& c = rules.career->cruise.net;
        c.score = [this](int32_t score) {
            if (net != nullptr) net->sendCruiseScore(score);
        };
        c.trick = [this](int32_t trick) {
            if (net != nullptr) net->sendTrick(trick);
        };
        c.end = [this] {
            if (net != nullptr) net->cruiseEnd();
        };
        c.board = [this] { netCruiseBoard(); };
    }
}

void TrackView::netCruiseBoard() {
    if (net == nullptr) return;
    const auto board = net->cruiseBoard();
    std::array<std::wstring, 3> lines;
    std::array<bool, 3> shown{};
    const int language = frontEnd.hudLanguage != nullptr ? frontEnd.hudLanguage->index : 7;
    for (size_t i = 0; i < 3 && i < board.size(); ++i) {
        shown[i] = board[i].second >= 0;
        lines[i] = std::wstring(board[i].first.begin(), board[i].first.end()) + L" " +
                   oj::toWide(oj::groupThousands(board[i].second, language));
    }
    if (frontEnd.cruiseResults != nullptr && frontEnd.window.id == RaceWindowId::CruiseResults) {
        frontEnd.cruiseResults->netBoard(lines, shown);
    } else if (frontEnd.hud != nullptr) {
        frontEnd.hud->netBoard(lines, shown);
    }
}

}  // namespace app
