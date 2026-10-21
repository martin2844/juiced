// The front end's side of the network game: what its windows ask through NetworkHost and the front end itself
// does with the session (docs/spec/28-network.md).

#include "Game/Frontend/Frontend.h"
#include "Game/Frontend/FrontendInternal.h"

#include "Core/Base/Log.h"
#include "Core/Base/Random.h"
#include "Core/Data/GameTexts.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/NetTransport.h"
#include "Core/Network/VoiceChat.h"
#include "Game/Media/Music.h"
#include <algorithm>
#include <cstring>
#include <format>
#include <windows.h>
#include "Core/Base/Text.h"
#include "Core/Career/Career.h"
#include "Core/Cars/CarCollection.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Race/RaceSession.h"
#include "Core/Base/X87.h"
#include "Core/Data/JetTexts.h"

namespace oj {

using namespace frontend;

namespace {

Frontend* g_netOwner = nullptr;  // the front end the session's setup goes to

}  // namespace

// The message (the manager's +0x44), else the network's message (+0x3C), still moving in (0x005AA120).
bool Frontend::onlineTopMoving() const {
    const ui::Screen* top = m_message ? m_message->screen.get() : (m_netMessage ? m_netMessage->screen.get() : nullptr);
    return top != nullptr && !top->transitionsDone();
}

// 0x004EBBF0 (the session's reset first). Without the matchmaking, off the online career, without a pink slip, or
// with no race ([G+0x34]) or no car of its first player: the career's +0x90 0 (the stakes dropped). Else won when
// that car finished first (its competitor's +0x118 and +0x104 1), or the host left (the transport's state 3), else
// by 0x004EBCF0 (hosting, a player left the race); the end (0x004A4C60, no tie), the career's +0x98 won and +0x94 1.
void Frontend::onlineRaceSettle() {
    if (m_data == nullptr || !m_data->career) return;
    Career& career = *m_data->career;
    net::NetGame* net = net::netGame();
    bool first = false;
    if (net::matchmaking() == nullptr || net::onlineHosted().mode != net::OnlineKind::Career ||
        net::onlineHosted().career.pinkslip == 0 || net == nullptr || !net->session.hooks.firstPlayer ||
        !net->session.hooks.firstPlayer(first)) {
        career.onlinePinkSlip = 0;
        return;
    }
    const bool won = first || net->transport.state() == net::State::HostLeft || net->session.playerLeftRace();
    onlineSettle(career, won, false, m_data->catalogue, m_data->dynamics.get());
    career.onlineWon = won ? 1 : 0;
    career.onlineSettled = 1;
}

// 0x004D4090, the network's delayed call of the pink slip's autosave: the grid walk's timeout.
void Frontend::onlineGridWalkTimeout() {
    m_onlineGridWalk.timeout();
}

// The session's setup taken by this front end (its game data), while it lives.
void Frontend::netHook() {
    net::NetGame* net = net::netGame();
    if (net == nullptr || m_data == nullptr) return;
    g_netOwner = this;
    net->session.hooks.setupChanged = [this](uint8_t id) { netSetupCame(id); };
    net->session.hooks.resetting = [this] { onlineRaceSettle(); };
}

void Frontend::netUnhook() {
    if (g_netOwner != this) return;
    g_netOwner = nullptr;
    if (net::NetGame* net = net::netGame()) {
        net->session.hooks.setupChanged = nullptr;
        net->session.hooks.resetting = nullptr;
    }
}

// The setup's messages as the session takes them (0x004E96A0): 3 the race's track (+0x10, 31 characters), 4 its
// route (+0x30), 5 nothing more (the race takes the session's laps), 6 its time of day (+0x6C), 7 its weather (+0x70),
// 9 the settings' difficulty (+0x64) and the race's (+0x60) as a float; 8 (0x004E8750) off the online career the
// custom race's group ([G+0x84]+0x784); a stage (0x0A, 0x004E8EE0) and the race (0x0E, 0x004E90A0) all of them but the
// laps, then the group.
void Frontend::netSetupCame(uint8_t id) {
    net::NetGame* net = net::netGame();
    if (net == nullptr || m_data == nullptr) return;
    const net::NetSession& session = net->session;
    RaceSetup& race = m_data->race;
    const bool all = id == 0x0A || id == 0x0E;
    if (id == 3 || all) race.track = session.place().substr(0, 31);
    if (id == 4 || all) race.route = session.route().substr(0, 31);
    if (id == 6 || all) race.timeOfDay = session.timeOfDay();
    if (id == 7 || all) race.weather = session.weather();
    if (id == 9 || all) {
        const auto difficulty = static_cast<float>(session.difficulty());
        save::Settings& set = optionSettings();
        set.setU32(save::Settings::kCustomDifficulty, std::bit_cast<uint32_t>(difficulty));
        race.difficulty = difficulty;
    }
    if (id == 8 || all) m_data->arcadeGroups.group = session.raceType();
}

// 0x0046DE30: a client goes where the host's stage is: 0 link_lobby, 1 instant_manu (the online career, 0x004E1AC0:
// instant_seltype), 2 .. 7 instant_setup, 8 link_selcar (the online career: its race setup, 0x004A44E0, until its
// grid walk was entered ([0x0075EE6C]), then CMSOnlineGridWalk). Nothing outside a network game.
void Frontend::netFollow() {
    net::NetGame* net = net::netGame();
    if (net == nullptr) return;
    switch (net->session.stage()) {
    case net::SessionStage::Lobby: request("link_lobby", SwitchMode::Replace); break;
    case net::SessionStage::RaceKind:
        request(onlineCareer() ? "instant_seltype" : "instant_manu", SwitchMode::Replace);
        break;
    case net::SessionStage::Location:
    case net::SessionStage::Difficulty:
    case net::SessionStage::Route:
    case net::SessionStage::Laps:
    case net::SessionStage::TimeOfDay:
    case net::SessionStage::Weather: request("instant_setup", SwitchMode::Replace); break;
    case net::SessionStage::Cars:
        if (!onlineCareer()) {
            request("link_selcar", SwitchMode::Replace);
        } else if (s_shared.onlineForms.gridWalked == 0) {
            onlineCareerRaceSetup();
        } else {
            request("CMSOnlineGridWalk", SwitchMode::Replace);
        }
        break;
    default:
        break;
    }
}

// 0x00471860: the session lost (the transport's state 1 .. 4): everything reset (0x004EA4E0 with 0), a message up
// closed, a note of why: the host not reached or silent (1, 4) "The connection to the host has failed.", the session
// full (2), the host gone (3). The race's front end's part (0x00407FB0 while it is up) is the race's.
bool Frontend::netDropped() {
    net::NetGame* net = net::netGame();
    if (net == nullptr) return false;
    const net::State state = net->transport.state();
    size_t text = 0;
    switch (state) {
    case net::State::JoinTimedOut:
    case net::State::HostTimedOut: text = kTextHostFailed; break;
    case net::State::Refused: text = kTextSessionFull; break;
    case net::State::HostLeft: text = kTextHostLeft; break;
    default:
        return false;
    }
    net->session.reset(false);
    if (m_message) m_message->close = true;
    message(ui::MessageType::Continue, m_gameTexts.at(text), false, nullptr);
    return true;
}

// 0x0046DFE0: past the cars' stage with `loading` the race is loaded ("loading"); a client whose window is not the
// host's stage goes there (0x0046DE30), else the session lost goes back to link_search (online_main online). True
// when the window is left.
bool Frontend::netWait(net::SessionStage stage, bool loading) {
    net::NetGame* net = net::netGame();
    if (net == nullptr) return false;
    if (net->session.stage() > net::SessionStage::Cars && loading) {
        request("loading", SwitchMode::Replace);
        return true;
    }
    if (net->transport.hosting()) return false;
    if (stage != net->session.stage()) {
        netFollow();
        return true;
    }
    if (!netDropped()) return false;
    net->session.reset(false);
    request("link_search", SwitchMode::Replace);
    return true;
}

// 0x0046E0F0: BACK (CLICK) asks: the host "Leaving the game now will end this session .." in the lobby, else "Abandon
// race set up and quit back to the lobby?"; a client "Leave game session?" ([0x0079E7D8], starting on no). Yes: the
// host past the lobby takes everyone back to it (the stage 0, link_lobby); else with the matchmaking (its hosting
// stopped for the host, vt+0x10) online_main, without it link_search, and everything reset (0x004EA4E0 with 1). Then
// the online keys (8 players, 9 friends: Game/Frontend/Network/OnlineFriendWindows.cpp), then 0x0046DFE0.
bool Frontend::netKeys(uint32_t event, net::SessionStage stage, bool loading) {
    net::NetGame* net = net::netGame();
    if (net == nullptr) return false;
    if (event == input::kBack) {
        sound("CLICK");
        size_t text = kTextLeaveSession;
        if (net->transport.hosting())
            text = net->session.stage() == net::SessionStage::Lobby ? kTextEndSession : kTextAbandonSetup;
        message(ui::MessageType::YesNo, m_gameTexts.at(text), false, &m_netAnswer, true);
    }
    if (m_netAnswer == 1) {
        m_netAnswer = 0;
        if (net->transport.hosting() && net->session.stage() != net::SessionStage::Lobby) {
            net->session.setStage(net::SessionStage::Lobby);
            request("link_lobby", SwitchMode::Replace);
        } else {
            if (net::Matchmaking* mm = net::matchmaking()) {
                if (net->transport.hosting()) mm->stopHosting();
                request("online_main", SwitchMode::Replace);
            } else {
                request("link_search", SwitchMode::Replace);
            }
            net->session.reset(true);
            return true;
        }
    }
    if (frontend::netOnlineKeys(*this, event)) return true;
    return netWait(stage, loading);
}

// 0x00471740: COUNTDOWN emptied; in a network game with the stage's wait running, under 16 s left COUNTDOWNBOX shown
// (moving in), under 15 s left the seconds cut toward zero + 1, under 1 s "1".
void Frontend::netCountdown() {
    ui::Screen& s = *m_screen;
    if (s.find("COUNTDOWN") >= 0) s.setText("COUNTDOWN", L"");
    net::NetGame* net = net::netGame();
    if (net == nullptr || !net->session.waiting()) return;
    const float left = net->session.waitLeft();
    if (left < 16.0f) s.show("COUNTDOWNBOX", true);
    if (left < 1.0f) {
        s.setText("COUNTDOWN", L"1");
    } else if (left < 15.0f) {
        s.setText("COUNTDOWN", std::format(L"{}", x87::chop(left) + 1));
    }
}

namespace {


}  // namespace

// 0x0046C3FA, the front end object's frame after the window manager's, while it watches the players' controllers
// (+0x38). Its network notes write their answer into +0x2C (their +0x20); answered, the next frame goes back from the
// network (0x0046C403): with the garage's music on (carsel's +0x3C) that music again (0x00436C60) and the game's music
// (with a song, [G+0x48] +0x08 and +0x10) to the settings' volume (+0x1C), else the front end's music (with a song,
// +0x0C) to it (+0x14); online (the user object, [0x0075F040]) with the matchmaking ([0x0075EF84]), the network game
// ([0x0075EF8C]) or the top window online_logon_gs, online_create_gs, online_connect_gs or eula_ps2 "Currently in an
// online screen, returning to main": the network game ended (0x004E9DD0), the message closed (+0x48) and menu asked
// (mode 1, inline: only with no switch waiting); then every online object let go (friends, matchmaking, voice,
// statistics, the user object, Winsock: 0x004E1810 .. 0x005D9EE4, logged); off line the network game ended, Winsock
// let go, the message closed and menu asked. Either way the network's message closed (+0x40), +0x2C 0, and the frame
// ends (0x0046C64E). Else (0x0046C64F): the user object's +0x90 (nothing); with Winsock started ([0x0075F754]) and the
// network down (0x00597730) under a top window but online_logon_xbox and online_passcode_xbox, unless +0x34: +0x3C
// counts up to 11 frames, then msgbox_network (with none up) the note (type 2) "The network cable is not connected. .."
// (1671); with the network up (or no such window) off line +0x50 and +0x54 -1, +0x3C and +0x34 0 and the network's
// message closed. Then online (0x0046C732): the user object's state 6 with no message up (or its number 0) the note
// (msgbox, type 1) jet 107; and with no network's message, under such a window, its state 1 or 7 msgbox_network's note
// (type 2) "Connection to network lost. .." (jet 110).
bool Frontend::networkWatch() {
    if (!m_watchControllers) return false;
    if (m_netNoteAnswer != 0) {
        if (m_carSel.musicOn()) {
            m_carSel.music(*this, true);
            Jukebox& music = jukebox();
            if (music.player() != nullptr && music.handle != 0) music.target = settingsMusic();
        } else if (m_musicHandle != 0) {
            m_musicTarget = settingsMusic();
        }
        if (net::onlineUser() != nullptr) {
            const std::string& top = topWindowName();
            if (net::matchmaking() != nullptr || net::netGame() != nullptr || is(top, "online_logon_gs") ||
                is(top, "online_create_gs") || is(top, "online_connect_gs") || is(top, "eula_ps2")) {
                log::info("frontend: Currently in an online screen, returning to main");
                net::endNetGame();
                closeMessage();
                request("menu", SwitchMode::Replace);
            }
            log::info("frontend: Current window is called [{}]", top);
            net::endOnline();
            log::info("frontend: ALL ONLINE STUFF CLOSED");
        } else {
            net::endNetGame();
            net::stopWinsock();
            closeMessage();
            request("menu", SwitchMode::Replace);
        }
        if (m_netMessage) m_netMessage->close = true;
        m_netNoteAnswer = 0;
        return true;
    }
    if (net::OnlineUser* u = net::onlineUser()) u->networkFrame();
    if (net::winsockStarted()) {
        const std::string& top = topWindowName();
        if (net::networkDown() && !top.empty() && !is(top, "online_logon_xbox") && !is(top, "online_passcode_xbox")) {
            if (m_cableNoteHeld == 0) {
                if (m_cableFrames <= 10) {
                    ++m_cableFrames;
                } else if (!m_netMessage) {
                    netMessage(ui::MessageType::ContinueModeless, m_gameTexts.at(kTextNoCable), &m_netNoteAnswer);
                }
            }
        } else if (net::onlineUser() == nullptr) {
            m_networkBack = {-1, -1};
            m_cableFrames = 0;
            m_cableNoteHeld = 0;
            if (m_netMessage) m_netMessage->close = true;
        }
    }
    const std::string& top = topWindowName();
    if (net::OnlineUser* u = net::onlineUser()) {
        if (u->state == net::UserState::ForcedDisconnect && (!m_message || m_messageNumber == 0)) {
            message(ui::MessageType::Continue, jetText(kJetSignedOnElsewhere), false, &m_netNoteAnswer);
        }
        if (!m_netMessage && !top.empty() && !is(top, "online_logon_xbox") && !is(top, "online_passcode_xbox") &&
            (u->state == net::UserState::ParameterError || u->state == net::UserState::ConnectionClosed)) {
            netMessage(ui::MessageType::ContinueModeless, jetText(kJetNetworkLost), &m_netNoteAnswer);
        }
    }
    return false;
}

}  // namespace oj
