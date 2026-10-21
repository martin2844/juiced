// The friends' and players' window online_players of GameSpy (docs/spec/28-network.md), over online_bg.

#include "Game/Frontend/Network/OnlinePlayers.h"

#include "Game/Frontend/Network/OnlineFriendInternal.h"

namespace oj::frontend {

using namespace onlinefriends;

// 0x00479CF0. The windows' enter; the friends' list changed (vt+0x14); +0x10 -1 (the user highlighted) and the timer
// 0; PLAYER_NAME_TITLE "NICKNAME", WON_LABEL "WINS", LOST_LABEL "RACES"; COUNTDOWNBOX and NO_OTHER_PLAYERS hidden;
// PERCENT "%"; the list (0x004794A0).
void OnlinePlayers::enter(OnlineHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    if (net::OnlineFriends* f = net::onlineFriends()) f->askList();
    m_selected = -1;
    m_timer = 0.0f;
    s.setText("PLAYER_NAME_TITLE", texts.at(kTextNickname));
    s.setText("WON_LABEL", texts.at(kTextWins));
    s.setText("LOST_LABEL", texts.at(kTextRaces));
    for (int e = 0; e < s.size(); ++e) {
        if (_stricmp(s.nameOf(e).c_str(), "COUNTDOWNBOX") == 0) s.setHidden(e, true);
    }
    for (int e = 0; e < s.size(); ++e) {
        if (_stricmp(s.nameOf(e).c_str(), "NO_OTHER_PLAYERS") == 0) s.setHidden(e, true);
    }
    s.setText("PERCENT", L"%");
    fillList(host, s);
}

// 0x004791B0: a user (none: nothing) not yet among the names (vt+0x18; Stock problem: one already there keeps the
// reference its caller gave it) added to m_player_name: its nick (vt+0x1C) cut to the names' width less 10 pixels
// (0x005A8F50; the width to the nearest with SSE2, 0x0079E5F8) with the user as the item's value; with the friends an
// empty item on m_friend_icon, its icon "live_friendinvite_rec" for a request from the friend (vt+0x3C),
// "live_friendinvite_sent" for ours (vt+0x40), else "live_friendonline" online (vt+0x50); the friend's reference let
// go. With the voice ([0x0075F044]) an empty item on m_voice_icon with the user's icon for the user object's first
// local player (0x0047FC20): mute; for a user whose devices are opened on while it talks, off.
void OnlinePlayers::add(ui::Screen& s, int32_t user) {
    net::OnlineUser* u = net::onlineUser();
    if (user == -1 || u == nullptr) return;
    for (size_t i = 0; i < s.menuSize("m_player_name"); ++i) {
        if (u->sameUser(itemUser(s, "m_player_name", i), user)) return;
    }
    const int most = s.pixelWidthNearest("m_player_name") - 10;
    s.addMenuItem("m_player_name", s.fitTextWithin("m_player_name", wide(u->userNick(user)), most), userValue(user));
    if (net::OnlineFriends* f = net::onlineFriends()) {
        const int32_t entry = f->findUser(user);
        s.addMenuItem("m_friend_icon", L"");
        const size_t item = s.menuSize("m_friend_icon") - 1;
        if (entry != -1) {
            const char* icon = f->requestFrom(entry) != 0   ? kIconFriendRequestFrom
                               : f->requestSent(entry) != 0 ? kIconFriendRequestSent
                               : f->online(entry) != 0      ? kIconFriendOnline
                                                            : nullptr;
            if (icon != nullptr) s.setMenuItemTextures("m_friend_icon", item, icon, "");
        }
        f->entryRelease(entry);
    }
    if (net::VoiceChat* voice = net::gameVoice()) {
        s.addMenuItem("m_voice_icon", L"");
        const size_t item = s.menuSize("m_voice_icon") - 1;
        const std::string& icon = net::voiceIcon(*voice, localPlayer(u, 0), user);
        if (!icon.empty()) s.setMenuItemTextures("m_voice_icon", item, icon, "");
    }
}

// 0x004794A0, with the network game and the user object: m_player_name, m_friend_icon and m_voice_icon (all three
// there, else nothing): the names' users let go (vt+0x14) and the three emptied (0x005AEEF0); each player of another
// machine (its user referenced, 0x004EA100) and each recent player (referenced) added (0x004791B0); the three menus'
// first item highlighted (with their help tickers); the user highlighted before (+0x10) highlighted again where its
// name is (a place past the end: the first). No names: NO_OTHER_PLAYERS over several lines, shown, "There are no
// other players."; else hidden.
void OnlinePlayers::fillList(OnlineHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    net::NetGame* net = net::netGame();
    net::OnlineUser* u = net::onlineUser();
    if (net == nullptr || u == nullptr) return;
    if (s.find("m_player_name") < 0 || s.find("m_friend_icon") < 0 || s.find("m_voice_icon") < 0) return;
    for (size_t i = 0; i < s.menuSize("m_player_name"); ++i) u->userRelease(itemUser(s, "m_player_name", i));
    for (const char* menu : {"m_player_name", "m_friend_icon", "m_voice_icon"}) s.clearMenu(menu);
    const std::vector<net::NetPlayer>& players = net->session.players();
    for (size_t i = 0; i < players.size(); ++i) {
        if (players[i].peer == -1) continue;
        u->userAddRef(players[i].profile);
        add(s, players[i].profile);
    }
    const net::RecentPlayers& recent = net::recentPlayers();
    for (uint32_t i = 0; i < recent.count; ++i) {
        u->userAddRef(recent.users[i]);
        add(s, recent.users[i]);
    }
    for (const char* menu : {"m_player_name", "m_friend_icon", "m_voice_icon"}) s.selectMenuItem(menu, 0);
    if (m_selected != -1) {
        for (size_t i = 0; i < s.menuSize("m_player_name"); ++i) {
            if (!u->sameUser(itemUser(s, "m_player_name", i), m_selected)) continue;
            for (const char* menu : {"m_player_name", "m_friend_icon", "m_voice_icon"}) s.selectMenuItem(menu, i);
        }
    }
    if (s.menuSize("m_player_name") == 0) {
        s.setMultiline("NO_OTHER_PLAYERS", true);
        s.show("NO_OTHER_PLAYERS", false);
        s.setText("NO_OTHER_PLAYERS", texts.at(kTextNoOtherPlayers));
    } else {
        for (int e = 0; e < s.size(); ++e) {
            if (_stricmp(s.nameOf(e).c_str(), "NO_OTHER_PLAYERS") == 0) s.setHidden(e, true);
        }
    }
}

// 0x00479B40: with the statistics ([0x0075F030]) the highlighted user's record (0x004EDFD0) shown (showRecord).
void OnlinePlayers::stats(ui::Screen& s) {
    if (net::stats::gameStatistics() == nullptr) return;
    showRecord(s, m_selected);
}

// 0x00479EB0: the windows' leave (the Menu's item kept); M_PLAYER_NAME's users let go (with the user object) and the
// menu emptied; the user highlighted (+0x10) let go, -1.
void OnlinePlayers::leave(ui::Screen& s) {
    net::OnlineUser* u = net::onlineUser();
    if (s.find("M_PLAYER_NAME") >= 0) {
        if (u != nullptr) {
            for (size_t i = 0; i < s.menuSize("M_PLAYER_NAME"); ++i) u->userRelease(itemUser(s, "M_PLAYER_NAME", i));
        }
        s.clearMenu("M_PLAYER_NAME");
    }
    if (u != nullptr) {
        u->userRelease(m_selected);
        m_selected = -1;
    }
}

// 0x00479F80. The transport failed (its state) with the user object: the local players' state 0 (0x004EEAF0). Without
// m_player_name nothing more. Each second (the names' +0x14C, never set, aside) the list again (0x004794A0) and the
// statistics (0x00479B40). The highlighted name's user (referenced for the while) unlike +0x10: it kept instead and
// the statistics. SEL_ shown with names, else hidden; ARROW shown with more than one. In the network game out of a race
// past the cars' stage: loading. COUNTDOWN (0x00471740). SELECT on a name: a user not among the friends (vt+0x20; the
// friends' +0x34, 0, aside; with the voice ([0x0075F044]) always) or the user object's +0x58 (0): the options' target
// our first local player and the user (0x0047A330), CLICK, online_player_options (mode 7). BACK: CLICK, the target
// none, the friends' +0x24; in a race hud_pause, else the host's stage's window (0x0046DE30).
void OnlinePlayers::update(OnlineHost& host, ui::Screen& s, uint32_t event) {
    net::NetGame* net = net::netGame();
    net::OnlineUser* u = net::onlineUser();
    if (net != nullptr && net->transport.state() != net::State::Normal && u != nullptr) net::setOnlineUsersState(0, 0);
    if (s.find("m_player_name") < 0) return;
    m_timer = host.frameTime() + m_timer;
    if (kRefresh < m_timer) {
        m_timer = 0.0f;
        fillList(host, s);
        stats(s);
    }
    int32_t user = -1;
    if (s.find("M_PLAYER_NAME") >= 0 && s.menuSize("M_PLAYER_NAME") != 0) {
        user = itemUser(s, "M_PLAYER_NAME", s.selectedMenuItem("M_PLAYER_NAME"));
        if (u != nullptr) u->userAddRef(user);
    }
    if (u != nullptr) {
        if (!u->sameUser(m_selected, user)) {
            u->userRelease(m_selected);
            u->userAddRef(user);
            m_selected = user;
            stats(s);
        }
        u->userRelease(user);
    }
    if (s.menuSize("M_PLAYER_NAME") != 0) {
        s.showContaining("SEL_", false);
    } else {
        s.hideContaining("SEL_");
    }
    if (1 < s.menuSize("M_PLAYER_NAME")) {
        s.showContaining("ARROW", false);
    } else {
        s.hideContaining("ARROW");
    }
    if (net != nullptr && !host.racePause() && net->session.stage() > net::SessionStage::Cars) {
        host.request("loading", SwitchMode::Replace);
        return;
    }
    host.netCountdown();
    if (event == input::kSelect) {
        if (s.menuSize("m_player_name") == 0) return;
        int32_t options = 0;
        if (net::OnlineFriends* f = net::onlineFriends()) {
            const int32_t entry = f->findUser(m_selected);
            if (entry == -1 || (f->canCancelRequest() != 0 && f->requestSent(entry) != 0)) options = 1;
            f->entryRelease(entry);
        }
        if (net::gameVoice() != nullptr) options = 1;
        if ((u != nullptr && u->feedbackOn() != 0) || options != 0) {
            onlineOptionsTarget(host, localPlayer(u, 0), m_selected);
            host.sound("CLICK", 1.0f, 0xF);
            host.request("online_player_options", SwitchMode::Swap);
        }
    } else if (event == input::kBack) {
        host.sound("CLICK", 1.0f, 0xF);
        onlineOptionsTarget(host, -1, -1);
        if (net::OnlineFriends* f = net::onlineFriends()) f->windowsLeft();
        if (host.racePause()) {
            host.request("hud_pause", SwitchMode::Replace);
        } else {
            host.netFollow();
        }
    }
}

}  // namespace oj::frontend
