// The network game's LAN windows (docs/spec/28-network.md, "The windows"): link_search, link_lobby, multi and
// link_selcar.

#include "Game/Frontend/Network/LinkWindows.h"

#include "Game/Frontend/Network/LobbyChat.h"

#include "Core/Base/Random.h"
#include "Core/Base/Text.h"
#include "Core/Base/X87.h"
#include "Core/Cars/CarCollection.h"
#include "Core/Data/GameTexts.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/OnlineRecords.h"
#include "Core/Network/VoiceChat.h"
#include "Core/Race/RaceSession.h"
#include "Game/Frontend/CustomRace/CustomRaceInternal.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <cstring>
#include <format>

namespace oj::frontend {

namespace {

bool same(const std::string& a, const char* b) {
    return _stricmp(a.c_str(), b) == 0;
}

// Each element of that name (case aside) hidden or shown (the windows' loops over their elements).
void hideAll(ui::Screen& s, const std::string& name, bool hidden) {
    for (int e = 0; e < s.size(); ++e) {
        if (same(s.nameOf(e), name.c_str())) s.setHidden(e, hidden);
    }
}

std::wstring widen(const std::string& text) {
    return std::wstring(text.begin(), text.end());
}

std::wstring wide(const std::u16string& text) {
    return std::wstring(text.begin(), text.end());
}

// The voice icon of a slot (0x0046F250, 0x00471160) on its element: shown; for a user the user object's first local
// player muted, the mute icon; else for one whose devices are opened the icon on while it talks (vt+0x2C, never on
// the PC), else off; else hidden.
void userVoiceIcon(ui::Screen& s, int e, net::VoiceChat& voice, int32_t first, int32_t user) {
    const net::VoiceIcons& icons = net::voiceIcons();
    s.setHidden(e, false);
    if (voice.isMuted(first, user)) {
        s.setTexture(e, icons.mute);
        return;
    }
    const net::VoiceChat::RemoteUser* r = voice.findUser(user);
    if (r == nullptr || r->hasVoice == 0) {
        s.setHidden(e, true);
        return;
    }
    s.setTexture(e, voice.userTalking(user) ? icons.on : icons.off);
}

}  // namespace

// 0x0046E800: GAMELIST emptied, the network game made when there is none (0x004E9C40), a search (the transport's
// +0x20), the list's timer and the join (+0x0C, +0x10) 0, the list (0x0046E3E0). (The voice's +0x14 online.)
void LinkSearch::enter(NetworkHost& host, ui::Screen& s) {
    if (s.find("GAMELIST") >= 0) s.clearMenu("GAMELIST");
    net::NetGame* net = net::netGame();
    if (net == nullptr) {
        net = &net::startNetGame(gameRandom());
        host.netHook();
    }
    net->transport.search(gameRandom());
    m_timer = 0.0f;
    m_joining = false;
    fillList(host, s);
}

// 0x0046E3E0: GAMELIST again: "NEW GAME" (its address none), then each session found, "<name> (<players>/<most>)"
// with its host's address; the first item highlighted, or the session highlighted before when it is still there (by
// its address).
void LinkSearch::fillList(NetworkHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    if (s.find("GAMELIST") < 0) return;
    const size_t before = s.selectedMenuItem("GAMELIST");
    const bool hadSession = before >= 1 && before - 1 < m_listed.size();
    const net::Address kept = hadSession ? m_listed[before - 1] : net::Address{};
    s.clearMenu("GAMELIST");
    m_listed.clear();
    s.addMenuItem("GAMELIST", texts.at(kTextNewGame), 0.0f);
    if (net::NetGame* net = net::netGame()) {
        for (const net::FoundSession& found : net->transport.sessions()) {
            std::u16string name;
            for (char16_t c : found.info.name) {
                if (c == 0) break;
                name.push_back(c);
            }
            const std::string count = std::format(" ({}/{})", found.info.used, found.info.max);
            s.addMenuItem("GAMELIST", wide(name) + widen(count), 0.0f);
            m_listed.push_back(found.address);
        }
    }
    s.selectMenuItem("GAMELIST", 0);
    if (!hadSession) return;
    for (size_t i = 0; i < m_listed.size(); ++i) {
        if (m_listed[i] == kept) s.selectMenuItem("GAMELIST", i + 1);
    }
}

// 0x0046E880. BACKMENU follows GAMELIST. Not joining: each second the list again; SELECT on an item (with the
// driver's name, [G+0x64]'s +0x1C): the race a custom one of type 5, one player, no opponents, one lap, and
// everything reset (0x004EA4E0 with 1); "NEW GAME": our player (0x004E9E40), host (the name, at most 6), the stage 0,
// CLICK, link_lobby; a session: our player, a join to its host, joining (+0x10), CLICK and "JOINING GAME..." (type 0).
// BACK: the network game ended (0x004E9DD0; Winsock let go off line), CLICK, menu. Joining, once no message nor window
// over this one moves in: the session lost searches again (the list from the start); in the session (the transport's
// +0xE4) the message closed and link_lobby.
void LinkSearch::update(NetworkHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (s.find("GAMELIST") >= 0 && s.find("BACKMENU") >= 0) {
        s.selectMenuItem("BACKMENU", s.selectedMenuItem("GAMELIST"));
    }
    net::NetGame* net = net::netGame();
    if (m_joining) {
        if (host.onlineTopMoving()) return;
        if (host.netDropped()) {
            if (net != nullptr) net->transport.search(gameRandom());
            m_timer = 0.0f;
            m_joining = false;
            fillList(host, s);
            return;
        }
        if (net != nullptr && net->transport.inSession()) {
            m_joining = false;
            host.closeMessage();
            host.request("link_lobby", SwitchMode::Replace);
        }
        return;
    }
    m_timer = host.frameTime() + m_timer;
    if (1.0f < m_timer) {
        m_timer = 0.0f;
        fillList(host, s);
    }
    if (event == input::kSelect) {
        if (s.find("GAMELIST") < 0 || s.menuSize("GAMELIST") == 0 || net == nullptr || data == nullptr) return;
        const size_t item = s.selectedMenuItem("GAMELIST");
        const std::u16string name(data->profileName.begin(), data->profileName.end());
        RaceSetup& race = data->race;
        race.mode = RaceMode::Custom;
        race.type = RaceType::Circuit;
        race.players = 1;
        race.opponents = 0;
        race.laps = 1;
        net->session.reset(true);
        if (item == 0 || item - 1 >= m_listed.size()) {
            net->session.addOwnPlayer(name, 0);
            net->session.host(name, 6);
            net->session.setStage(net::SessionStage::Lobby);
            host.sound("CLICK");
            host.request("link_lobby", SwitchMode::Replace);
        } else {
            net->session.addOwnPlayer(name, 0);
            net->session.join(m_listed[item - 1]);
            m_joining = true;
            host.sound("CLICK");
            host.message(MessageType::Note, texts.at(kTextJoining), false, nullptr);
        }
    } else if (event == input::kBack) {
        net::endNetGame();
        host.sound("CLICK");
        host.request("menu", SwitchMode::Replace);
    }
}

// 0x0046EED0: each slot's name multiline and "-", its icons (playericon, CPU, voice) hidden, its points, races and
// rating "-".
void LinkLobby::clearSlots(ui::Screen& s) {
    for (int slot = 1; slot <= 6; ++slot) {
        const std::string name = std::format("slot{}name", slot);
        for (int e = 0; e < s.size(); ++e) {
            if (!same(s.nameOf(e), name.c_str())) continue;
            s.setMultiline(s.nameOf(e), true);
            s.setText(e, L"-");
            break;
        }
        for (const char* icon : {"playericon{}", "CPU{}", "voice{}"}) {
            const std::string element = std::vformat(icon, std::make_format_args(slot));
            for (int e = 0; e < s.size(); ++e) {
                if (!same(s.nameOf(e), element.c_str())) continue;
                s.setHidden(e, true);
                break;
            }
        }
        for (const char* text : {"points{}", "races{}", "rating{}"}) {
            const std::string element = std::vformat(text, std::make_format_args(slot));
            for (int e = 0; e < s.size(); ++e) {
                if (!same(s.nameOf(e), element.c_str())) continue;
                s.setText(e, L"-");
                break;
            }
        }
    }
}

// 0x0046F250, in the lobby only (the stage 0): the players by their points, most first (a bubble sort starting again
// after each swap); the slots cleared (0x0046EED0); each slot: playericon shown for the host's machine's (ours when we
// host) whose index on its machine is 0 (Stock problem: the index read is the unsorted player's of the slot's place),
// CPU for a second player of a machine; with the voice ([0x0075F044]) the user of the slot's place's player (unsorted
// as playericon's) referenced (vt+0x10; Stock problem: never let go, one reference more a frame) and with one, voice<n>:
// ours, with the user object's first local player's devices opened, shown with the icon on while it sends (vt+0x20)
// else off; another's the user's icon (mute, on, off, or hidden); the name, the points and the races; with the
// statistics, the player's user's record asked for (0x0046F9CA): rating<n> its rank ("-" for 0).
void LinkLobby::showSlots(ui::Screen& s) {
    net::NetGame* net = net::netGame();
    if (net == nullptr || net->session.stage() > net::SessionStage::Lobby) return;
    const std::vector<net::NetPlayer>& players = net->session.players();
    const size_t count = players.size();
    std::array<uint32_t, 6> order{};
    std::array<int32_t, 6> points{};
    for (size_t i = 0; i < count && i < 6; ++i) {
        order[i] = static_cast<uint32_t>(i);
        points[i] = players[i].race.points;
    }
    if (count > 1) {
        for (size_t i = 0; i < count - 1 && i < 5; ++i) {
            if (points[i] < points[i + 1]) {
                std::swap(points[i], points[i + 1]);
                std::swap(order[i], order[i + 1]);
                i = static_cast<size_t>(-1);
            }
        }
    }
    clearSlots(s);
    auto element = [&](const std::string& name) {
        for (int e = 0; e < s.size(); ++e) {
            if (same(s.nameOf(e), name.c_str())) return e;
        }
        return -1;
    };
    for (size_t slot = 0; slot < count && slot < 6; ++slot) {
        const int n = static_cast<int>(slot) + 1;
        const uint32_t at = order[slot];
        const net::NetPlayer* p = at < count ? &players[at] : nullptr;
        if (const int e = element(std::format("playericon{}", n)); e >= 0) {
            // The host's machine: ours when we host, else the machine of the player found as the host's (peer 0).
            bool host = false;
            if (p != nullptr && p->peer == -1) {
                host = net->transport.hosting();
            } else if (p != nullptr) {
                for (const net::NetNode& node : net->session.nodes()) {
                    if (node.peer == p->peer) {
                        host = node.peer == 0;
                        break;
                    }
                }
            }
            s.setHidden(e, !(host && (count <= slot || players[slot].localIndex == 0)));
        }
        if (const int e = element(std::format("CPU{}", n)); e >= 0) {
            s.setHidden(e, !(p != nullptr && 0 < p->localIndex));
        }
        net::VoiceChat* voice = net::gameVoice();
        net::OnlineUser* user = net::onlineUser();
        if (voice != nullptr && user != nullptr && slot < count) {
            const net::NetPlayer& q = players[slot];
            user->userAddRef(q.profile);
            const int32_t first = user->localPlayers.empty() ? -1 : user->localPlayers[0];
            const int e = element(std::format("voice{}", n));
            if (q.profile != -1 && e >= 0) {
                if (q.peer == -1) {
                    if (first != -1 && voice->hasVoice(first) != 0) {
                        s.setHidden(e, false);
                        s.setTexture(e, voice->sending(first) != 0 ? net::voiceIcons().on : net::voiceIcons().off);
                    }
                } else {
                    userVoiceIcon(s, e, *voice, first, q.profile);
                }
            }
        }
        if (p == nullptr) continue;
        if (const int e = element(std::format("slot{}name", n)); e >= 0) s.setText(e, wide(p->name));
        if (const int e = element(std::format("points{}", n)); e >= 0) s.setText(e, std::format(L"{}", p->race.points));
        if (const int e = element(std::format("races{}", n)); e >= 0) s.setText(e, std::format(L"{}", p->race.races));
        if (net::stats::gameStatistics() != nullptr) {
            const net::stats::UserRecord* r = net::stats::record(p->profile);
            if (r != nullptr && r->asked != 0) {
                if (const int e = element(std::format("rating{}", n)); e >= 0) {
                    s.setText(e, r->rank == 0 ? std::wstring(L"-") : std::format(L"{}", r->rank));
                }
            }
        }
    }
}

// 0x0046EBB0: +0x0C .. +0x14 and [0x0075EE70] 0; every element shown; off the host (or without the network game)
// clienttext, SELECTA and SEL_TEXT hidden, the host clienttext; without the matchmaking FRIENDS_BUT, FRIENDS_TEXT,
// PLAYERS_BUT, PLAYERS_TEXT, RATING1 .. 6 and RATING_LABEL1 .. 6 hidden; the slots cleared (0x0046EED0);
// [0x0075EE6C] 0 (the online career's grid walk); with the statistics ([0x0075F030]) each session player's user's
// record (0x004EDFD0) asked for again (+0x28 0).
void LinkLobby::enter(WindowHost& host, ui::Screen& s) {
    *this = LinkLobby{};
    for (int e = 0; e < s.size(); ++e) s.setHidden(e, false);
    net::NetGame* net = net::netGame();
    if (net == nullptr || !net->transport.hosting()) {
        for (const char* name : {"clienttext", "SELECTA", "SEL_TEXT"}) hideAll(s, name, true);
    } else {
        hideAll(s, "clienttext", true);
    }
    host.shared().onlineForms.friendsFromMain = 0;
    if (net::matchmaking() == nullptr) {
        for (const char* name : {"FRIENDS_BUT", "FRIENDS_TEXT", "PLAYERS_BUT", "PLAYERS_TEXT"}) hideAll(s, name, true);
        for (int n = 1; n <= 6; ++n) {
            hideAll(s, std::format("RATING{}", n), true);
            hideAll(s, std::format("RATING_LABEL{}", n), true);
        }
    }
    // The fixed build's guest: no friends' windows (F1 and F2 do nothing), their hints hidden; the chat's hint as the
    // frames will show it.
    if (const net::OnlineUser* u = net::onlineUser(); u != nullptr && u->guest()) {
        for (const char* name : {"FRIENDS_BUT", "FRIENDS_TEXT", "PLAYERS_BUT", "PLAYERS_TEXT"}) hideAll(s, name, true);
    }
    lobbyChatHint(s);
    clearSlots(s);
    host.shared().onlineForms.gridWalked = 0;
    if (net::stats::gameStatistics() != nullptr && net != nullptr) {
        for (const net::NetPlayer& p : net->session.players()) {
            if (net::stats::UserRecord* r = net::stats::record(p.profile)) r->asked = 0;
        }
    }
}

// 0x0046FB20: the slots (0x0046F250). An online career's pink slip ([0x0075EF84], [0x0075EF88] 1, [0x00728D00]):
// with its stakes set (the career's +0x90) its end (0x004A4C60): under two players won by the career's +0xAC, else by
// the points of the first two players (ours the more: won; equal: a tie, nothing changed), and unless a tie
// "CONGRATULATIONS! You have won your opponent's car!" (670) or "BAD LUCK! You have lost your car!" (671, type 1);
// +0x14 1. Then (+0x14) with the saving on autosave (mode 5), +0x10 1; then (+0x10) everything reset (0x004EA4E0
// with 1), CLICK and, with the matchmaking, its hosting stopped (vt+0x10, 0x004E2740) and online_main, else
// link_search. Each of these frames does nothing more. The shared checks (0x0046E0F0 with the stage 0 and loading);
// not taken, SELECT of the host (CLICK): with the matchmaking the online career's stage 1 and instant_seltype, an
// arcade session of any group ([0x00728D34] 9999) the stage 1 and instant_manu, else its group sent (0x004EA7F0) and
// set ([G+0x84]+0x784), the stage 2 and instant_setup; on the LAN the stage 1 and instant_manu.
void LinkLobby::update(OnlineHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    showSlots(s);
    net::NetGame* net = net::netGame();
    if (net::matchmaking() != nullptr && net::onlineHosted().mode == net::OnlineKind::Career &&
        net::onlineHosted().career.pinkslip != 0 && data != nullptr && data->career) {
        Career& career = *data->career;
        if (career.onlinePinkSlip != 0) {
            bool won = false;
            bool tie = false;
            const std::vector<net::NetPlayer>* players = net != nullptr ? &net->session.players() : nullptr;
            if (players == nullptr || players->size() < 2) {
                won = career.valueAC != 0;
            } else {
                const int32_t first = (*players)[0].race.points;
                const int32_t second = (*players)[1].race.points;
                if (second < first && (*players)[0].peer == -1) won = true;
                if (first < second && (*players)[1].peer == -1) won = true;
                if (first == second) tie = true;
            }
            onlineSettle(career, won, tie, data->catalogue, data->dynamics.get());
            if (!tie) host.message(MessageType::Continue, texts.at(won ? kTextPinkWon : kTextPinkLost), false, nullptr);
            m_settled = 1;
            return;
        }
        if (m_settled != 0) {
            if (data->saving != 0) host.request("autosave", SwitchMode::Push);
            m_settled = 0;
            m_leaving = 1;
            return;
        }
        if (m_leaving != 0) {
            if (net != nullptr) net->session.reset(true);
            host.sound("CLICK", 1.0f, 0xF);
            if (net::Matchmaking* mm = net::matchmaking()) {
                mm->stopHosting();
                host.request("online_main", SwitchMode::Replace);
            } else {
                host.request("link_search", SwitchMode::Replace);
            }
            return;
        }
    }
    if (host.netKeys(event, net::SessionStage::Lobby, true)) return;
    // The fixed build's lobby chat of an online session: its hint; SELECT2 opens it (Frontend/Network/LobbyChat).
    lobbyChatHint(s);
    if (event == input::kSelect2 && lobbyChatOn()) {
        openLobbyChat(host);
        return;
    }
    if (event != input::kSelect || net == nullptr || !net->transport.hosting()) return;
    host.sound("CLICK", 1.0f, 0xF);
    if (net::matchmaking() == nullptr) {
        net->session.setStage(net::SessionStage::RaceKind);
        host.request("instant_manu", SwitchMode::Replace);
    } else if (host.onlineCareer()) {
        net->session.setStage(net::SessionStage::RaceKind);
        host.request("instant_seltype", SwitchMode::Replace);
    } else if (net::onlineHosted().arcade.level == net::kAny) {
        net->session.setStage(net::SessionStage::RaceKind);
        host.request("instant_manu", SwitchMode::Replace);
    } else {
        const int32_t group = net::onlineHosted().arcade.level;
        net->session.sendRaceType(group);
        if (data != nullptr) data->arcadeGroups.group = group;
        net->session.setStage(net::SessionStage::Location);
        host.request("instant_setup", SwitchMode::Replace);
    }
}

// 0x0046CAA0: the second player's device let go ([G+0x18]'s +0x2C -1); MENU's item "link" reads "LAN MODE" (1661).
void Multi::enter(CustomRaceHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    host.askSecondPlayerRelease();
    if (s.find("menu") < 0) return;
    for (size_t i = 0; i < s.menuSize("MENU"); ++i) {
        if (same(s.menuItemCommand("MENU", i), "link")) {
            s.setMenuItemText("MENU", i, texts.at(kTextLanMode));
            break;
        }
    }
}

// 0x0046CB10: BACKMENU follows MENU; "split" open with two input devices or more, else closed and the second item
// highlighted each frame. SELECT: a closed item NO; "split" a split screen custom race (two players, mode 3, type 5),
// CLICK, instant_manu; "link" Winsock started (0x00597690; off line) or "An error occurred starting the network
// adaptor. .." (513), else mode 1, type 5 (the players kept), CLICK, link_search. BACK (CLICK): menu.
void Multi::update(CustomRaceHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (s.find("MENU") >= 0 && s.find("BACKMENU") >= 0) s.selectMenuItem("BACKMENU", s.selectedMenuItem("MENU"));
    if (s.find("MENU") >= 0) {
        // The input manager's devices ([G+0x18]'s list, each counted by its method +0x20, 0x0042E470: 1).
        s.setMenuItemEnabled("MENU", "split", host.inputDevices() >= 2);
        if (host.inputDevices() < 2) s.selectMenuItem("MENU", 1);
    }
    if (event == input::kSelect) {
        if (s.find("Menu") < 0) return;
        size_t item = s.selectedMenuItem("MENU");
        if (item >= s.menuSize("MENU")) item = 0;
        if (s.menuSize("MENU") == 0 || !s.menuItemEnabled("MENU", item)) {
            host.sound("NO");
            return;
        }
        const std::string command = s.menuItemCommand("MENU", item);
        if (same(command, "split")) {
            if (data != nullptr) {
                data->race.players = 2;
                data->race.mode = RaceMode::SplitScreen;
                data->race.type = RaceType::Circuit;
            }
            host.sound("CLICK");
            host.request("instant_manu", SwitchMode::Replace);
        } else if (same(command, "link")) {
            if (!net::startWinsock()) {
                host.message(MessageType::Continue, texts.at(kTextNoNetwork), false, nullptr);
                return;
            }
            if (data != nullptr) {
                data->race.mode = RaceMode::Custom;
                data->race.type = RaceType::Circuit;
            }
            host.sound("CLICK");
            host.request("link_search", SwitchMode::Replace);
        }
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("menu", SwitchMode::Replace);
    }
}

// 0x0046FF10. Each slot: READY hidden, NOTREADY shown, NAME emptied, VOICE hidden; its car view (0x0045A2C0 on
// CAR3D<n> with CARNAME<n>, the first CARLOGO, the zoom 1, no mirror, the shadow) with the camera at (0, 180, -500);
// the PS2_ elements hidden; the config seen emptied and an empty list of cars. Each local player: the car at the place
// of the first player's group (the place back to 0 on an empty one) in its arcade cars (the first of the model, else
// the first) sent (0x00470A50, 0x004EA2E0; online its parts cleared first). "location" the city place of the
// session's place (the first when none), "route" "POINT-TO-POINT" for a point-to-point route (the host then sends one
// lap, 0x05) else the first of 1 .. 4 its name holds, " Reversed" after a reversed one; "laps" "-" for a
// point-to-point route, else the session's laps. Locked, Locked1, Locked2 and COUNTDOWNBOX hidden (none in the
// window).
void LinkSelCar::enter(CustomRaceHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    freeViews();
    *this = LinkSelCar{};
    for (int i = 0; i < 6; ++i) {
        Slot& slot = m_slots[static_cast<size_t>(i)];
        const int n = i + 1;
        hideAll(s, std::format("READY{}", n), true);
        hideAll(s, std::format("NOTREADY{}", n), false);
        if (const int e = s.find(std::format("NAME{}", n)); e >= 0) s.clearText(e);
        hideAll(s, std::format("VOICE{}", n), true);
        const std::string element = std::format("CAR3D{}", n);
        if (host.engine() != nullptr && data != nullptr && s.find(element) >= 0) {
            scene::CarView::Setup setup;
            setup.element = element;
            setup.name = std::format("CARNAME{}", n);
            setup.logo = i == 0 ? "CARLOGO" : "";
            setup.zoom = 1.0f;
            setup.shadow = true;
            slot.view = std::make_unique<scene::CarView>(*host.engine(), s, setup, data->catalogue, data->tables);
            slot.view->setCameraPlace({0.0f, 180.0f, -500.0f, 0.0f});
        }
        s.hideContaining("PS2_");
    }
    net::NetGame* net = net::netGame();
    if (data != nullptr) {
        GameData& d = *data;
        // Each local player its own save system object's arcade cars ([game + 0x64 + 4 j]+0x40) and groups
        // ([game + 0x84 + 4 j]), its place in the ids of the first player's group.
        for (int32_t j = 0; j < d.race.players && j < 2; ++j) {
            ArcadeCarGroups& groups = j == 0 ? d.arcadeGroups : d.arcadeGroups2;
            std::vector<Car>& cars = j == 0 ? d.arcadeCars : d.arcadeCars2;
            const auto group = static_cast<size_t>(std::clamp(d.arcadeGroups.group, 0, 7));
            if (groups.ids[group][static_cast<size_t>(std::clamp(groups.index, 0, 59))] == 0) groups.index = 0;
            const int32_t id = groups.ids[group][static_cast<size_t>(std::clamp(groups.index, 0, 59))];
            size_t k = 0;
            for (size_t c = 0; c < cars.size(); ++c) {
                if (cars[c].def != nullptr && cars[c].def->id == id) {
                    k = c;
                    break;
                }
            }
            // 0x0047052B: online outside an online career with a session of no mods ([0x00728D38] 0) the car's
            // parts cleared (0x004600C0) before it is sent.
            if (net::matchmaking() != nullptr && !host.onlineCareer() && net::onlineHosted().arcade.mods == 0 &&
                k < cars.size()) {
                customrace::clearParts(cars[k]);
            }
            if (net != nullptr && k < cars.size() && cars[k].def != nullptr) {
                const Car& car = cars[k];
                net->session.sendCar(net::CarConfig::of(car, car.def->name), j);
            }
        }
    }
    if (net != nullptr && data != nullptr) {
        const net::NetSession& session = net->session;
        const std::vector<ExeTables::CityLocation>& places = data->tables.cityLocations;
        std::wstring location = places.empty() ? std::wstring() : places.front().text;
        for (const ExeTables::CityLocation& place : places) {
            if (_stricmp(place.name.c_str(), session.place().c_str()) == 0) location = place.text;
        }
        if (s.find("location") >= 0) s.setText("location", location);
        const std::string route = session.route();
        const bool pointToPoint = route.find("_P2P") != std::string::npos || route.find("_p2p") != std::string::npos;
        std::wstring text;
        if (pointToPoint) {
            text = texts.at(kTextPointToPointUpper);
            if (net->transport.hosting()) net->session.sendLaps(1);
        } else {
            for (const char* digit : {"1", "2", "3", "4"}) {
                if (route.find(digit) != std::string::npos) {
                    text = widen(digit);
                    break;
                }
            }
        }
        if (route.find("_rev") != std::string::npos || route.find("_REV") != std::string::npos) {
            text += L" " + texts.at(kTextReversed);
        }
        if (s.find("route") >= 0) s.setText("route", text);
        if (s.find("laps") >= 0) {
            s.setText("laps", pointToPoint ? std::wstring(L"-") : std::format(L"{}", net->session.laps()));
        }
    }
    for (const char* name : {"Locked", "Locked1", "Locked2", "COUNTDOWNBOX"}) hideAll(s, name, true);
}

// 0x00471160. The shared checks (0x0046E0F0 with the stage 8 and loading). Each local player's event (0x00471040):
// SELECT its readiness turned (0x004EA370; Stock problem: always of the local index 0) and CLICK; 6 ("CHANGE CAR")
// while not ready instant_selcar (mode 3) and SELECT; BACK not ready; then the CARSEL_ elements hidden while ready,
// else shown. Each slot of a player (by the players' order): NAME its name, READY or NOTREADY by its readiness,
// CARNAME the display name of the slot's car; a slot without one: both hidden, the texts emptied, its car's CarDef
// none and the config seen emptied. The slots' cars (0x00470B80): each frame the player's paint into the slot's car,
// the config seen and the car shown (painted again); a config whose name or parts changed (the view loading nothing,
// a name) makes the slot's car anew (0x0048E7D0: its CarDef by the name, the config applied), a copy at the end of
// the slot's list shown turned 3 pi / 4 and turning 0.4 a second (the list kept to ten cars, its first dropped); the
// view's frame. A slot without a player has its view emptied. With the voice each slot of a player: voice<n> (its
// user's icon, ours by its devices). Then COUNTDOWN (0x00471740).
void LinkSelCar::update(CustomRaceHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    if (host.netKeys(event, net::SessionStage::Cars, true)) return;
    net::NetGame* net = net::netGame();
    if (net == nullptr || data == nullptr) return;
    GameData& d = *data;
    std::vector<net::NetPlayer>& players = net->session.players();
    auto readyOf = [&](int32_t j) {
        return static_cast<size_t>(j) < players.size() && players[static_cast<size_t>(j)].race.ready != 0;
    };
    // Each local player with its own event (the window's +0x88 + 4 j).
    for (int32_t j = 0; j < d.race.players && j < 2; ++j) {
        const bool ready = readyOf(j);
        const uint32_t e = j == 0 ? event : host.playerEvent(1);
        if (e == input::kSelect) {
            net->session.sendReady(ready ? 0 : 1, 0);
            host.sound("CLICK");
        } else if (e == input::kSelect2) {
            if (!ready) {
                host.request("instant_selcar", SwitchMode::ReplaceAfterTransitions);
                host.sound("SELECT");
            }
        } else if (e == input::kBack) {
            net->session.sendReady(0, 0);
        }
        if (readyOf(j)) {
            s.hideContaining("CARSEL_");
        } else {
            s.showContaining("CARSEL_", false);
        }
    }
    for (size_t i = 0; i < m_slots.size(); ++i) {
        Slot& slot = m_slots[i];
        const int n = static_cast<int>(i) + 1;
        const std::string name = std::format("NAME{}", n);
        const std::string carName = std::format("CARNAME{}", n);
        if (i < players.size()) {
            const net::NetPlayer& p = players[i];
            if (s.find(name) >= 0) s.setText(name, wide(p.name));
            hideAll(s, std::format("READY{}", n), p.race.ready == 0);
            hideAll(s, std::format("NOTREADY{}", n), p.race.ready != 0);
            if (s.find(carName) >= 0) {
                s.setText(carName, slot.car.def != nullptr ? widenSigned(slot.car.def->displayName) : std::wstring());
            }
            // The voice ([0x0075F044]): voice<n> with the player's user (0x004EA100: referenced, Stock problem: never
            // let go): ours shown with the icon on while it sends, off, when the user object's first local player's
            // devices are opened, else hidden; another's the user's icon.
            net::VoiceChat* voice = net::gameVoice();
            net::OnlineUser* user = net::onlineUser();
            const int e = s.find(std::format("voice{}", n));
            if (voice != nullptr && user != nullptr && e >= 0) {
                user->userAddRef(p.profile);
                const int32_t first = user->localPlayers.empty() ? -1 : user->localPlayers[0];
                if (p.profile != -1) {
                    if (p.peer == -1) {
                        if (voice->hasVoice(first) != 0) {
                            s.setHidden(e, false);
                            s.setTexture(e, voice->sending(first) != 0 ? net::voiceIcons().on : net::voiceIcons().off);
                        } else {
                            s.setHidden(e, true);
                        }
                    } else {
                        userVoiceIcon(s, e, *voice, first, p.profile);
                    }
                }
            }
        } else {
            hideAll(s, std::format("READY{}", n), true);
            hideAll(s, std::format("NOTREADY{}", n), true);
            if (const int e = s.find(name); e >= 0) s.clearText(e);
            if (const int e = s.find(carName); e >= 0) s.clearText(e);
            slot.car.def = nullptr;
            slot.seen.fill(0);
        }
    }
    for (size_t i = 0; i < m_slots.size(); ++i) {
        Slot& slot = m_slots[i];
        if (i >= players.size()) {
            if (slot.view) slot.view->clear();
            continue;
        }
        const net::CarConfig& config = players[i].car;
        std::array<uint8_t, 0x74> bytes{};
        config.write(bytes.data());
        // The paint of the config, every frame (the dwords from +0x2C): the slot's car, the config seen and the car
        // shown, painted again.
        auto paintInto = [&](Car& car) {
            const CarDef* def = car.def;
            const auto body = car.body;
            const auto mods = car.mods;
            config.apply(car);
            car.def = def;
            car.body = body;
            car.mods = mods;
        };
        paintInto(slot.car);
        std::memcpy(slot.seen.data() + 0x2C, bytes.data() + 0x2C, 0x74 - 0x2C);
        if (slot.view && slot.view->hasModel() && slot.current < slot.list.size()) {
            paintInto(slot.list[slot.current]);
            slot.view->repaint(slot.list[slot.current]);
        }
        if (slot.seen != bytes && slot.view && !slot.view->busy() && config.name[0] != 0) {
            slot.car = makeCar(findCarDef(d.defs, std::string(config.name.data())));
            config.apply(slot.car);
            slot.list.push_back(copyCar(slot.car));
            slot.view->show(slot.list.back(), &slot.current, static_cast<uint32_t>(slot.list.size() - 1),
                            std::bit_cast<float>(0x4016CBE4u), std::bit_cast<float>(0x3ECCCCCDu), true, host.viewLoading());
            if (slot.list.size() > 10) slot.list.pop_front();
            slot.seen = bytes;
        }
        if (slot.view) slot.view->frame(host.frameTime(), gameRandom());
    }
    host.netCountdown();
}

// 0x004709B0: the race's automatic gearbox (+0x54) the settings'; the views and the lists freed.
void LinkSelCar::leave(WindowHost& host) {
    GameData* data = host.gameData();
    if (data != nullptr) data->race.gearbox = static_cast<int32_t>(host.settingsValue(save::Settings::kAutomatic));
    freeViews();
}

void LinkSelCar::freeViews() {
    for (Slot& slot : m_slots) {
        if (slot.view) slot.view->clear();
        slot.view.reset();
        slot.list.clear();
    }
}

}  // namespace oj::frontend
