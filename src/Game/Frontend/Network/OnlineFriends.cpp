// The friends' and players' window online_friends of GameSpy (docs/spec/28-network.md), over online_bg.

#include "Game/Frontend/Network/OnlineFriends.h"

#include "Game/Frontend/Network/OnlineFriendInternal.h"

namespace oj::frontend {

using namespace onlinefriends;

// 0x0047B4C0. The windows' enter; the friends' list changed; outside a session (no matchmaking's address, 0x004E2780)
// with the user object and the voice ([0x0075F044]) our first local player (referenced, vt+0x2C) added to the voice
// (vt+0x18); +0x0C 0.5, +0x10 1 (the friend named before highlighted again), +0x14 -1, +0x18 1, +0x1C 0;
// FRIEND_NAME_TITLE "NICKNAME", WON_LABEL "WINS", LOST_LABEL "RACES"; PLAYING_TITLE and COUNTDOWNBOX hidden;
// FRIENDS_LIST_EMPTY over several lines and hidden; CAN_JOIN "* in a game session"; GAME_INFO_TEXT and GAME_INFO_BUT
// hidden, GAME_INFO_TEXT "DETAILS"; PERCENT "%"; the list (0x0047AC70); [0x0075EEA8] 0.
void OnlineFriends::enter(OnlineHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    if (net::OnlineFriends* f = net::onlineFriends()) f->askList();
    if (!matchmakingAddressed()) {
        net::OnlineUser* u = net::onlineUser();
        if (net::VoiceChat* voice = net::gameVoice(); voice != nullptr && u != nullptr) voice->addPlayer(localPlayerRef(u, 0));
    }
    m_timer = 0.5f;
    m_again = 1;
    m_selected = -1;
    m_statsAgain = 1;
    m_searching = 0;
    s.setText("FRIEND_NAME_TITLE", texts.at(kTextNickname));
    s.setText("WON_LABEL", texts.at(kTextWins));
    s.setText("LOST_LABEL", texts.at(kTextRaces));
    const auto hideNamed = [&](const char* name) {
        for (int e = 0; e < s.size(); ++e) {
            if (_stricmp(s.nameOf(e).c_str(), name) == 0) s.setHidden(e, true);
        }
    };
    hideNamed("PLAYING_TITLE");
    hideNamed("COUNTDOWNBOX");
    s.setMultiline("FRIENDS_LIST_EMPTY", true);
    hideNamed("FRIENDS_LIST_EMPTY");
    s.setText("CAN_JOIN", L"* " + texts.at(kTextInSession));
    hideNamed("GAME_INFO_TEXT");
    hideNamed("GAME_INFO_BUT");
    s.setText("GAME_INFO_TEXT", texts.at(kTextDetails));
    s.setText("PERCENT", L"%");
    fillList(host, s);
    host.shared().onlineForms.friendsMadeMatchmaking = 0;
}

// 0x0047AC70: GAMESPY shown; with the user object and the friends, m_friend_name, m_state, m_friend_icon and
// m_voice_icon (all four there): the friends updated (vt+0x18); the names' users let go and the four emptied; each
// friend of the list (referenced) and its user (vt+0x1C, referenced): with the statistics and +0x18 its record made
// (0x004EDF20) or asked for again (+0x28 0); its nick's low bytes, and with +0x10 the one named before ([0x0075EEB0]) becomes the
// highlighted user (+0x14); the nick cut to the names' width (0x005B3260) less 10 pixels with the user as the value
// and the low bytes as the item's name; m_state "PLAYING" (vt+0x54), "ONLINE" (vt+0x50) or "OFFLINE"; m_friend_icon
// an empty item with its icon: "live_gameinvite_rec" (vt+0x44), "live_gameinvite_sent" (vt+0x48),
// "live_friendinvite_rec" (vt+0x3C), "live_friendinvite_sent" (vt+0x40), "live_friendonline" (vt+0x50); m_voice_icon an
// empty item, with the voice ([0x0075F044]) its icon: mute for a user our first local player muted, else
// "live_comm_off" for a friend's voice mail (vt+0x5C: none on the PC; 0x004475D0 by the name); the friend let go. A list: +0x10 and +0x18 0. The four menus'
// first item highlighted, then the place of +0x14's user (the last of the same). No names with the friends' +0x0C:
// FRIENDS_LIST_EMPTY over several lines, shown, its text (1973); else hidden.
void OnlineFriends::fillList(OnlineHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    s.show("GAMESPY", false);
    net::OnlineUser* u = net::onlineUser();
    net::OnlineFriends* f = net::onlineFriends();
    if (u == nullptr || f == nullptr) return;
    constexpr std::array<const char*, 4> kMenus = {"m_friend_name", "m_state", "m_friend_icon", "m_voice_icon"};
    for (const char* menu : kMenus) {
        if (s.find(menu) < 0) return;
    }
    f->update();
    for (size_t i = 0; i < s.menuSize("m_friend_name"); ++i) u->userRelease(itemUser(s, "m_friend_name", i));
    for (const char* menu : kMenus) s.clearMenu(menu);
    const int most = s.pixelWidth("m_friend_name") - 10;
    const std::vector<int32_t> list = f->list();
    for (const int32_t listed : list) {
        f->entryAddRef(listed);
        const int32_t entry = listed;
        const int32_t user = f->entryUser(entry);
        if (net::stats::gameStatistics() != nullptr && m_statsAgain != 0) {
            if (net::stats::UserRecord* r = net::stats::record(user)) {
                r->asked = 0;
            } else {
                net::stats::makeRecord(user);
            }
        }
        const std::u16string& nick = u->userNick(user);
        const std::string name = lowBytes(nick);
        if (m_again != 0 && name == host.shared().onlineForms.friendName) {
            u->userRelease(m_selected);
            u->userAddRef(user);
            m_selected = user;
        }
        s.addCommandItem("m_friend_name", s.fitTextWithin("m_friend_name", wide(nick), most), userValue(user), name);
        const size_t state = f->playing(entry) != 0 ? kTextPlaying : f->online(entry) != 0 ? kTextOnline : kTextOffline;
        s.addMenuItem("m_state", texts.at(state));
        s.addMenuItem("m_friend_icon", L"");
        const char* icon = f->inviteFrom(entry) != 0    ? kIconInviteFrom
                           : f->inviteSent(entry) != 0  ? kIconInviteSent
                           : f->requestFrom(entry) != 0 ? kIconFriendRequestFrom
                           : f->requestSent(entry) != 0 ? kIconFriendRequestSent
                           : f->online(entry) != 0      ? kIconFriendOnline
                                                        : nullptr;
        if (icon != nullptr) s.setMenuItemTextures("m_friend_icon", s.menuSize("m_friend_icon") - 1, icon, "");
        s.addMenuItem("m_voice_icon", L"");
        if (net::VoiceChat* voice = net::gameVoice()) {
            const size_t item = s.menuSize("m_voice_icon") - 1;
            if (voice->isMuted(localPlayer(u, 0), user)) {
                s.setMenuItemTextures("m_voice_icon", item, net::voiceIcons().mute, "");
            } else if (f->voiceMail(entry) != 0) {
                s.setMenuItemTextures("m_voice_icon", item, "live_comm_off", "");
            }
        }
        f->entryRelease(entry);
    }
    if (!list.empty()) {
        m_again = 0;
        m_statsAgain = 0;
    }
    for (const char* menu : kMenus) s.selectMenuItem(menu, 0);
    if (m_selected != -1) {
        for (size_t i = 0; i < s.menuSize("m_friend_name"); ++i) {
            if (!u->sameUser(itemUser(s, "m_friend_name", i), m_selected)) continue;
            for (const char* menu : kMenus) s.selectMenuItem(menu, i);
        }
    }
    if (s.menuSize("m_friend_name") == 0 && f->listAsked != 0) {
        s.setMultiline("FRIENDS_LIST_EMPTY", true);
        s.show("FRIENDS_LIST_EMPTY", false);
        s.setText("FRIENDS_LIST_EMPTY", texts.at(kTextNoFriends));
    } else {
        for (int e = 0; e < s.size(); ++e) {
            if (_stricmp(s.nameOf(e).c_str(), "FRIENDS_LIST_EMPTY") == 0) s.setHidden(e, true);
        }
    }
}

// 0x0047B2C0: with the statistics ([0x0075F030]) GAME the highlighted friend's game (the friends' entry of the user,
// vt+0x20 referenced, its +0x64: empty, let go) and the user's record as 0x00479B40 shows it.
void OnlineFriends::stats(ui::Screen& s) {
    if (net::stats::gameStatistics() == nullptr) return;
    if (net::OnlineFriends* friends = net::onlineFriends()) {
        const int32_t entry = friends->findUser(m_selected);
        if (const int e = s.find("GAME"); e >= 0) s.setText(e, std::wstring(friends->gameName(entry).begin(),
                                                                                 friends->gameName(entry).end()));
        friends->entryRelease(entry);
    }
    showRecord(s, m_selected);
}

// 0x0047B870: the windows' leave (the Menu's item kept); the matchmaking the window made let go ([0x0075EEA8],
// 0x004E1A20); the highlighted user let go, -1; outside a session (0x004E2780) with the user object and the voice our
// first local player (referenced, vt+0x2C) taken off the voice (vt+0x1C); M_FRIEND_NAME: the highlighted item's name
// kept ([0x0075EEB0]), the users let go, the menu emptied.
void OnlineFriends::leave(OnlineHost& host, ui::Screen& s) {
    if (host.shared().onlineForms.friendsMadeMatchmaking != 0) net::releaseMatchmaking();
    net::OnlineUser* u = net::onlineUser();
    if (u != nullptr) {
        u->userRelease(m_selected);
        m_selected = -1;
    }
    if (!matchmakingAddressed() && u != nullptr) {
        if (net::VoiceChat* voice = net::gameVoice()) voice->removePlayer(localPlayerRef(u, 0));
    }
    if (s.find("M_FRIEND_NAME") < 0) return;
    if (s.menuSize("M_FRIEND_NAME") != 0) {
        host.shared().onlineForms.friendName = s.menuItemCommand("M_FRIEND_NAME", s.selectedMenuItem("M_FRIEND_NAME"));
    }
    if (u != nullptr) {
        for (size_t i = 0; i < s.menuSize("M_FRIEND_NAME"); ++i) u->userRelease(itemUser(s, "M_FRIEND_NAME", i));
    }
    s.clearMenu("M_FRIEND_NAME");
}

// 0x0047B9D0. The transport failed with the user object: the local players' state 0; in a race [0x0075EE70] 0. Without
// M_FRIEND_NAME nothing more. Each second (the names' +0x14C aside) the list and the statistics again; the highlighted
// name's user unlike +0x14 (with names): kept and the statistics. SEL_ and ARROW as online_players'. In the network
// game out of a race past the cars' stage: loading. COUNTDOWN. GAME_INFO_TEXT and GAME_INFO_BUT shown for a friend
// playing (vt+0x54) the same game (vt+0x68), else hidden. +0x1C (a friend's session asked): once no message or window
// moves in, with valid results or the matchmaking's list in, +0x1C 0, the search stopped (vt+0x28), the message closed;
// a result: the details ([0x0075EEA4] 1) of its arcade or career create window (mode 7), else "NO GAMES FOUND" (type
// 1). SELECT on a name with +0x14: the options' target our first local player and the user, CLICK,
// online_friend_options (mode 7). BACK: CLICK, the target none, the friends' +0x24; in a race hud_pause; else with
// [0x0075EE70] online_main, in a session the host's stage's window (0x0046DE30), else online_main; [0x0075EE70] 0. 6 on
// a name: CLICK; a friend playing the same game: the matchmaking made ([0x0075EEA8] 1) when there is none, the friend's
// session asked (0x004E4460), "please wait..." (type 0), +0x1C 1; the friend let go.
void OnlineFriends::update(OnlineHost& host, ui::Screen& s, uint32_t event) {
    const TextTable& texts = host.gameTexts();
    net::NetGame* net = net::netGame();
    net::OnlineUser* u = net::onlineUser();
    net::OnlineFriends* f = net::onlineFriends();
    if (net != nullptr && net->transport.state() != net::State::Normal && u != nullptr) net::setOnlineUsersState(0, 0);
    if (host.racePause()) host.shared().onlineForms.friendsFromMain = 0;
    if (s.find("M_FRIEND_NAME") < 0) return;
    m_timer = host.frameTime() + m_timer;
    if (kRefresh < m_timer) {
        m_timer = 0.0f;
        fillList(host, s);
        stats(s);
    }
    int32_t user = -1;
    if (s.menuSize("M_FRIEND_NAME") != 0) {
        user = itemUser(s, "M_FRIEND_NAME", s.selectedMenuItem("M_FRIEND_NAME"));
        if (u != nullptr) u->userAddRef(user);
    }
    if (u != nullptr) {
        if (!u->sameUser(m_selected, user) && s.menuSize("M_FRIEND_NAME") != 0) {
            u->userRelease(m_selected);
            u->userAddRef(user);
            m_selected = user;
            stats(s);
        }
        u->userRelease(user);
    }
    if (s.menuSize("M_FRIEND_NAME") != 0) {
        s.showContaining("SEL_", false);
    } else {
        s.hideContaining("SEL_");
    }
    if (1 < s.menuSize("M_FRIEND_NAME")) {
        s.showContaining("ARROW", false);
    } else {
        s.hideContaining("ARROW");
    }
    if (net != nullptr && !host.racePause() && net->session.stage() > net::SessionStage::Cars) {
        host.request("loading", SwitchMode::Replace);
        return;
    }
    host.netCountdown();
    if (f != nullptr) {
        const int32_t entry = f->findUser(m_selected);
        if (f->playing(entry) != 0 && f->sameGame(entry) != 0) {
            s.show("GAME_INFO_TEXT", false);
            s.show("GAME_INFO_BUT", false);
        } else {
            for (const char* name : {"GAME_INFO_TEXT", "GAME_INFO_BUT"}) {
                for (int e = 0; e < s.size(); ++e) {
                    if (_stricmp(s.nameOf(e).c_str(), name) == 0) s.setHidden(e, true);
                }
            }
        }
        f->entryRelease(entry);
    }
    if (m_searching != 0) {
        if (host.onlineTopMoving()) return;
        net::Matchmaking* mm = net::matchmaking();
        if (net::countValidResults() == 0 && (mm == nullptr || mm->listIn == 0)) return;
        m_searching = 0;
        if (mm != nullptr) mm->stopSearch();
        host.closeMessage();
        if (net::countValidResults() != 0) {
            int32_t access = 0;
            std::array<char16_t, 16> name{};
            int32_t playing = 0;
            int32_t career = 0;
            net::readSession(0, access, name, playing, career);
            host.shared().onlineForms.details = 1;
            host.request(career == 0 ? "online_arcade_create" : "online_career_create", SwitchMode::Swap);
        } else {
            host.message(MessageType::Continue, texts.at(kTextNoGames), false, nullptr);
        }
        return;
    }
    if (event == input::kSelect) {
        if (s.menuSize("M_FRIEND_NAME") == 0 || m_selected == -1) return;
        onlineOptionsTarget(host, localPlayer(u, 0), m_selected);
        host.sound("CLICK", 1.0f, 0xF);
        host.request("online_friend_options", SwitchMode::Swap);
    } else if (event == input::kBack) {
        host.sound("CLICK", 1.0f, 0xF);
        onlineOptionsTarget(host, -1, -1);
        if (f != nullptr) f->windowsLeft();
        if (host.racePause()) {
            host.request("hud_pause", SwitchMode::Replace);
            return;
        }
        if (host.shared().onlineForms.friendsFromMain == 1) {
            host.request("online_main", SwitchMode::Replace);
        } else if (net != nullptr && net->transport.inSession()) {
            host.netFollow();
        } else {
            host.request("online_main", SwitchMode::Replace);
        }
        host.shared().onlineForms.friendsFromMain = 0;
    } else if (event == input::kSelect2) {
        if (s.menuSize("M_FRIEND_NAME") == 0) return;
        host.sound("CLICK", 1.0f, 0xF);
        if (f == nullptr) return;
        const int32_t entry = f->findUser(m_selected);
        if (f->playing(entry) != 0 && f->sameGame(entry) != 0) {
            if (net::matchmaking() == nullptr) {
                host.shared().onlineForms.friendsMadeMatchmaking = 1;
                net::makeMatchmaking();
            } else {
                host.shared().onlineForms.friendsMadeMatchmaking = 0;
            }
            net::searchFriendSession(entry);
            host.message(MessageType::Note, texts.at(kTextPleaseWait), false, nullptr);
            m_searching = 1;
        }
        f->entryRelease(entry);
    }
}

}  // namespace oj::frontend
