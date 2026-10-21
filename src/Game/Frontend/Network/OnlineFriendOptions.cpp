// The friends' and players' window online_friend_options of GameSpy (docs/spec/28-network.md), over online_bg.

#include "Game/Frontend/Network/OnlineFriendOptions.h"

#include "Game/Frontend/Network/OnlineFriendInternal.h"

namespace oj::frontend {

using namespace onlinefriends;

// 0x0047C0B0. The windows' enter; friend_name the target's nick (15 characters) or empty; the user object's +0x60 with
// our first local player (referenced: Stock problem, never let go); +0x3C -1; "menu" with the friends emptied and by
// the friend (vt+0x20): a request from them: (the voice mail of the console, +0x54 / +0x68: none;) "Accept friend
// request", "Decline friend request", with the friends' +0x38 (0) "Block friend requests"; an invitation from them
// (vt+0x44): (the voice mail: none;) "Accept invite", "Decline invite", "Remove friend" each with the help "<nick>
// invites you to join  <its game>" (the friends' +0x64: empty); else: without our request waiting (vt+0x40) "Join" for
// a friend playing Juiced (vt+0x4C) somewhere else than our session (0x00597790 with the matchmaking's address,
// 0x004E2750, and the friend's, vt+0x60), with our invitation waiting (vt+0x48) "Cancel invite" when the friends allow
// it (+0x78: 0), else "Send invite" with our address there and elsewhere; then "Remove friend" (remove). (The voice
// mail's "Complaints" never comes.) The friend let go. +0x0C, +0x10, +0x1C, +0x40 0, +0x14 .. +0x34 -1; [0x0075EEA8]
// and [0x0075EEF0] 0; the menu's help ticker.
void OnlineFriendOptions::enter(OnlineHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    net::OnlineUser* u = net::onlineUser();
    if (host.shared().onlineForms.optionsUser != -1 && u != nullptr) {
        s.setText("friend_name", nick15(u->userNick(host.shared().onlineForms.optionsUser)));
    } else {
        s.setText("friend_name", L"");
    }
    if (u != nullptr) u->askVoiceMail(localPlayerRef(u, 0));
    m_mail = -1;
    net::OnlineFriends* f = net::onlineFriends();
    if (s.find("menu") >= 0 && f != nullptr) {
        s.clearMenu("menu");
        const int32_t entry = f->findUser(host.shared().onlineForms.optionsUser);
        int32_t mail = 0;
        // The console's voice mail (the user object's +0x54 0, +0x68 -1 here): "Voice attachment" with it.
        const auto voiceMail = [&](int32_t which, const std::wstring* help) {
            if (u == nullptr || u->voiceMailWaiting(localPlayer(u, 0)) != 0) return;
            m_mail = u->voiceMail(localPlayerRef(u, 0), host.shared().onlineForms.optionsUser, which);
            if (m_mail == -1 || u->voiceMailNew(localPlayerRef(u, 0), m_mail) == 0) return;
            u->readVoiceMail(localPlayerRef(u, 0), m_mail);
            s.addCommandItem("menu", texts.at(kTextVoiceAttachment), 0.0f, "voice_mail");
            if (help != nullptr) s.setMenuItemHelp("menu", s.menuSize("menu") - 1, *help);
            mail = 1;
        };
        if (f->requestFrom(entry) != 0) {
            voiceMail(0, nullptr);
            s.addCommandItem("menu", texts.at(kTextAcceptRequest), 0.0f, "accept_request");
            s.addCommandItem("menu", texts.at(kTextDeclineRequest), 0.0f, "decline_request");
            if (f->canBlock() != 0) s.addCommandItem("menu", texts.at(kTextBlock), 0.0f, "block");
            if (mail != 0) s.addCommandItem("menu", texts.at(kTextComplaints), 0.0f, "complaints");
        } else if (f->inviteFrom(entry) != 0) {
            std::wstring help = u != nullptr ? wide(u->userNick(host.shared().onlineForms.optionsUser)) : std::wstring();
            help += L" ";
            help += texts.at(kTextInvitesYou);
            help += L" ";
            help += wide(f->gameName(entry));
            voiceMail(1, &help);
            for (const auto& [text, command] : {std::pair<size_t, const char*>{kTextAcceptInvite, "accept_invite"},
                                                {kTextDeclineInvite, "decline_invite"},
                                                {kTextRemove, "remove_invite"}}) {
                s.addCommandItem("menu", texts.at(text), 0.0f, command);
                s.setMenuItemHelp("menu", s.menuSize("menu") - 1, help);
            }
            if (mail != 0) s.addCommandItem("menu", texts.at(kTextComplaints), 0.0f, "complaints");
        } else {
            if (f->requestSent(entry) == 0) {
                const net::Matchmaking* mm = net::matchmaking();
                const std::optional<net::Address> ours = mm != nullptr ? mm->presenceAddress : std::nullopt;
                const std::optional<net::Address> theirs = f->address(entry);
                const bool same = (!ours && !theirs) || (ours && theirs && *ours == *theirs);
                if (f->playingJuiced(entry) != 0 && !same) {
                    s.addCommandItem("menu", texts.at(kTextJoin), 0.0f, "join");
                }
                if (f->inviteSent(entry) != 0) {
                    if (f->canCancelInvite() != 0) {
                        s.addCommandItem("menu", texts.at(kTextCancelInvite), 0.0f, "cancel");
                    }
                } else if (ours && !same) {
                    s.addCommandItem("menu", texts.at(kTextSendInvite), 0.0f, "invite");
                }
            }
            s.addCommandItem("menu", texts.at(kTextRemove), 0.0f, "remove");
        }
        f->entryRelease(entry);
    }
    m_searching = 0;
    m_joining = 0;
    m_removeAnswer = -1;
    m_declineAnswer = -1;
    m_voicePlaying = 0;
    m_voiceInviteAnswer = -1;
    m_blockAnswer = -1;
    m_addAnswer = -1;
    m_joinAnswer = -1;
    m_complaintAnswer = -1;
    m_acceptInviteAnswer = -1;
    m_inRace = 0;
    host.shared().onlineForms.friendsMadeMatchmaking = 0;
    host.shared().onlineForms.friendsMadeNetGame = 0;
    if (s.find("menu") >= 0) s.selectMenuItem("menu", s.selectedMenuItem("menu"));
}

// 0x0047C670: the windows' leave (the Menu's item kept); the matchmaking the window made let go ([0x0075EEA8]); the
// network game it made ended ([0x0075EEF0], 0x004E9DD0); [0x0075EEF4] 0.
void OnlineFriendOptions::leave(OnlineHost& host) {
    if (host.shared().onlineForms.friendsMadeMatchmaking != 0) net::releaseMatchmaking();
    if (host.shared().onlineForms.friendsMadeNetGame != 0) net::endNetGame();
    host.shared().onlineForms.friendJoining = 0;
}

// 0x0047C6D0 (accepting an invitation): the friend (vt+0x20); in the career with the current event's +0x03 the
// career's +0xB0 1; their invitation dealt with (vt+0x70); a friend in the same game: the network game made when
// there is none ([0x0075EEF0] 1), the matchmaking made anew when there is none (0x004E1A20, 0x00560D50, [0x0075EEA8]
// 1); in a session during a race +0x40 1; everything reset (0x004EA4E0 with 1); the friend's session asked
// (0x004E4460); +0x0C 1 and "JOINING GAME..." (type 0). The friend let go.
void OnlineFriendOptions::acceptInvite(OnlineHost& host) {
    GameData* data = host.gameData();
    net::OnlineFriends* f = net::onlineFriends();
    if (f == nullptr) return;
    const int32_t entry = f->findUser(host.shared().onlineForms.optionsUser);
    if (data != nullptr && data->race.mode == RaceMode::Career && data->career) {
        if (const CalendarEvent* e = currentEvent(data->career->calendar); e != nullptr && e->pinkSlip != 0) {
            data->career->onlineLeftPinkSlip = 1;
        }
    }
    f->clearInvite(entry, 0);
    if (f->sameGame(entry) != 0) {
        joinFriendSession(host, entry);
    }
    f->entryRelease(entry);
}

// The join of a friend's session (0x0047C6D0's and the join question's): the network game and the matchmaking made
// when there are none (their flags), +0x40 in a session during a race, everything reset, the session asked, +0x0C 1 and
// "JOINING GAME..." (type 0).
void OnlineFriendOptions::joinFriendSession(OnlineHost& host, int32_t entry) {
    const TextTable& texts = host.gameTexts();
    net::NetGame* net = net::netGame();
    if (net == nullptr) {
        host.shared().onlineForms.friendsMadeNetGame = 1;
        net = &net::startNetGame(gameRandom());
        host.netHook();
    } else {
        host.shared().onlineForms.friendsMadeNetGame = 0;
    }
    if (net::matchmaking() == nullptr) {
        host.shared().onlineForms.friendsMadeMatchmaking = 1;
        net::makeMatchmaking();
    } else {
        host.shared().onlineForms.friendsMadeMatchmaking = 0;
    }
    if (net->transport.inSession() && host.racePause()) m_inRace = 1;
    net->session.reset(true);
    net::searchFriendSession(entry);
    m_searching = 1;
    host.message(MessageType::Note, texts.at(kTextJoining), false, nullptr);
}

// 0x0047C870. The transport failed out of a join with the user object: the local players' state 0. A closedown asked
// ([0x0075EEF8]) once no message is up (or its number 0): the target none, the friends' +0x24, the message closed,
// closedown. The session asked (+0x0C): once nothing moves in, with valid results or the list in: +0x0C 0, the search
// stopped; a result joined (0x004734E0 with 0): +0x10 1, [0x0075EEF4] 1; not joined during a race: the closedown asked;
// no result: during a race the target none, the message closed and closedown, else the message closed, "This game
// session is no longer available." (type 1) and online_friends (mode 7). Joining (+0x10): once nothing moves in, the
// session lost (0x00471860): +0x10 and [0x0075EEF4] 0, the target none; during a race the closedown asked, else
// online_friends; in the session: +0x10 0, the message closed, the flags 0, the target none; in a race closedown, else
// link_lobby; [0x0075EE70] 0. The answers: remove (+0x14) yes the friend gone (vt+0x30), CLICK, online_friends; decline
// (+0x18) yes their invitation dealt with (vt+0x70 with 2), online_friends; no -1 alone; the invitation's voice (+0x20)
// yes the voice window's invitation (0x0047DC60), its way back online_friends and online_voice, no our invitation
// (vt+0x6C with our address), CLICK, online_friends; join (+0x34) yes a friend in the same game joined as above (else
// "This game session is no longer available." and online_friends), no CLICK; the complaint (+0x30) yes sent (+0x5C with
// +0x38), CLICK, online_friends, no CLICK; the voice mail (+0x1C) once nothing moves in and the user object's +0x28:
// the voice window's mail (0x0047DC20), its way back online_friend_options and online_voice; block (+0x24) yes the
// request denied (vt+0x2C with 2), CLICK, online_friends; accept request (+0x28) yes ours sent too (vt+0x28), CLICK,
// online_friends, no CLICK, online_friends; accept invite (+0x2C) yes 0x0047C6D0. In the network game out of a race
// past the cars' stage: the target none, the friends' +0x24, loading. SELECT (CLICK) on an item with the friends:
// accept_request their request accepted (vt+0x2C with 0) and for a temporary friend (vt+0x58) "Send friend request:
// <nick>?" (type 3 into +0x28), else online_friends; decline_request denied (vt+0x2C with 1), online_friends; block
// "Permanently block requests from  <nick>?" into +0x24; voice_mail with the user object's +0x28 the voice window,
// else +0x1C 1 and "please wait..." (type 0); accept_invite: in a session "Leaving the game now will end this session
// .." for the host or "Leave game session?", else "Any unsaved progress will be lost .."; in the career with the
// current event's +0x03 "Quitting will result in a race forfeit .." "\n" "ARE YOU SURE?" in its place (type 3 into
// +0x2C); decline_invite (vt+0x70 with 1), online_friends; remove_invite "Remove friend <nick>?" into +0x14; join: in a
// session the host "Leaving the game now .." into +0x34, a client +0x34 1 at once; else "Any unsaved progress .." into
// +0x34; invite (vt+0x6C with our address), online_friends; cancel (vt+0x74), online_friends; remove "<nick>: do you
// want to remove this Friend .." into +0x14; complaints the item "Offensive message"; offensive_message the kind 2 and
// "Send complaint. Are you sure?" into +0x30; any other online_friends. BACK: CLICK, online_friends.
void OnlineFriendOptions::update(OnlineHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    net::NetGame* net = net::netGame();
    net::OnlineUser* u = net::onlineUser();
    net::OnlineFriends* f = net::onlineFriends();
    if (m_joining == 0 && net != nullptr && net->transport.state() != net::State::Normal && u != nullptr) {
        net::setOnlineUsersState(0, 0);
    }
    const auto leaveTarget = [&] {
        onlineOptionsTarget(host, -1, -1);
        if (f != nullptr) f->windowsLeft();
    };
    if (host.shared().onlineForms.closedownAsked != 0) {
        if (host.messageUp() && host.messageNumber() != 0) return;
        host.shared().onlineForms.closedownAsked = 0;
        leaveTarget();
        host.closeMessage();
        host.request("closedown", SwitchMode::Replace);
        return;
    }
    if (m_searching != 0) {
        if (host.onlineTopMoving()) return;
        net::Matchmaking* mm = net::matchmaking();
        if (net::countValidResults() == 0 && (mm == nullptr || mm->listIn == 0)) return;
        m_searching = 0;
        if (mm != nullptr) mm->stopSearch();
        if (net::countValidResults() != 0) {
            if (host.onlineJoinSession(0)) {
                m_joining = 1;
                host.shared().onlineForms.friendJoining = 1;
            } else if (m_inRace != 0) {
                host.shared().onlineForms.closedownAsked = 1;
            }
            return;
        }
        if (m_inRace != 0) {
            leaveTarget();
            host.closeMessage();
            host.request("closedown", SwitchMode::Replace);
            return;
        }
        host.closeMessage();
        host.message(MessageType::Continue, host.jetText(kJetNoLongerAvailable), false, nullptr);
        host.request("online_friends", SwitchMode::Swap);
        return;
    }
    if (m_joining != 0) {
        if (host.onlineTopMoving()) return;
        if (host.netDropped()) {
            m_joining = 0;
            host.shared().onlineForms.friendJoining = 0;
            leaveTarget();
            if (m_inRace != 0) {
                host.shared().onlineForms.closedownAsked = 1;
                return;
            }
            host.request("online_friends", SwitchMode::Swap);
            return;
        }
        if (net == nullptr || !net->transport.inSession()) return;
        m_joining = 0;
        host.closeMessage();
        host.shared().onlineForms.friendsMadeMatchmaking = 0;
        host.shared().onlineForms.friendsMadeNetGame = 0;
        leaveTarget();
        host.request(host.racePause() ? "closedown" : "link_lobby", SwitchMode::Replace);
        host.shared().onlineForms.friendsFromMain = 0;
        return;
    }
    const auto toFriends = [&] {
        host.sound("CLICK", 1.0f, 0xF);
        host.request("online_friends", SwitchMode::Swap);
    };
    const auto withFriend = [&](auto&& act) {
        if (f == nullptr) return;
        const int32_t entry = f->findUser(host.shared().onlineForms.optionsUser);
        act(entry);
        f->entryRelease(entry);
    };
    if (m_removeAnswer == 1) {
        m_removeAnswer = -1;
        withFriend([&](int32_t entry) { f->remove(entry); });
        toFriends();
        return;
    }
    if (m_removeAnswer == 0) {
        m_removeAnswer = -1;
        return;
    }
    if (m_declineAnswer == 1) {
        m_declineAnswer = -1;
        withFriend([&](int32_t entry) { f->clearInvite(entry, 2); });
        toFriends();
        return;
    }
    if (m_declineAnswer == 0) {
        m_declineAnswer = -1;
        return;
    }
    if (m_voiceInviteAnswer == 1) {
        m_voiceInviteAnswer = -1;
        const net::Matchmaking* mm = net::matchmaking();
        const std::optional<net::Address> ours = mm != nullptr ? mm->presenceAddress : std::nullopt;
        if (f != nullptr) {
            const int32_t entry = f->findUser(host.shared().onlineForms.optionsUser);
            host.onlineVoice().askInvite(host, entry, host.shared().onlineForms.optionsUser, ours);
            f->entryRelease(entry);
        }
        host.shared().onlineForms.voiceBack = "online_friends";
        host.request("online_voice", SwitchMode::Swap);
        return;
    }
    if (m_voiceInviteAnswer == 0) {
        m_voiceInviteAnswer = -1;
        const net::Matchmaking* mm = net::matchmaking();
        const std::optional<net::Address> ours = mm != nullptr ? mm->presenceAddress : std::nullopt;
        withFriend([&](int32_t entry) { f->invite(entry, ours, 0); });
        toFriends();
        return;
    }
    if (m_joinAnswer == 1) {
        if (f == nullptr) return;
        const int32_t entry = f->findUser(host.shared().onlineForms.optionsUser);
        if (entry == -1) {
            m_joinAnswer = entry;
            toFriends();
            return;
        }
        m_joinAnswer = -1;
        if (f->sameGame(entry) != 0) {
            joinFriendSession(host, entry);
            f->entryRelease(entry);
            return;
        }
        host.message(MessageType::Continue, host.jetText(kJetNoLongerAvailable), false, nullptr);
        host.request("online_friends", SwitchMode::Swap);
        f->entryRelease(entry);
    } else if (m_joinAnswer == 0) {
        m_joinAnswer = -1;
        host.sound("CLICK", 1.0f, 0xF);
        return;
    }
    if (m_complaintAnswer == 1) {
        m_complaintAnswer = -1;
        if (u != nullptr) u->sendFeedback(host.shared().onlineForms.optionsPlayer, host.shared().onlineForms.optionsUser, m_complaint);
        toFriends();
        return;
    }
    if (m_complaintAnswer == 0) {
        m_complaintAnswer = -1;
        host.sound("CLICK", 1.0f, 0xF);
        return;
    }
    if (m_voicePlaying != 0) {
        if (host.onlineTopMoving()) return;
        if (u == nullptr || u->voiceMailRead == 0) return;
        m_voicePlaying = 0;
        host.closeMessage();
        host.onlineVoice().askMail(host, host.shared().onlineForms.optionsUser, m_mail);
        host.shared().onlineForms.voiceBack = "online_friend_options";
        host.request("online_voice", SwitchMode::Swap);
        return;
    }
    if (m_blockAnswer == 1) {
        m_blockAnswer = -1;
        withFriend([&](int32_t entry) { f->answer(entry, 2); });
        toFriends();
        return;
    }
    if (m_blockAnswer == 0) {
        m_blockAnswer = -1;
        return;
    }
    if (m_addAnswer == 1) {
        m_addAnswer = -1;
        if (f != nullptr) f->addFriend(host.shared().onlineForms.optionsUser, 0);
        toFriends();
        return;
    }
    if (m_addAnswer == 0) {
        m_addAnswer = -1;
        toFriends();
        return;
    }
    if (m_acceptInviteAnswer == 1) {
        m_acceptInviteAnswer = -1;
        acceptInvite(host);
        return;
    }
    if (m_acceptInviteAnswer == 0) {
        m_acceptInviteAnswer = -1;
        return;
    }
    if (net != nullptr && !host.racePause() && net->session.stage() > net::SessionStage::Cars) {
        leaveTarget();
        host.request("loading", SwitchMode::Replace);
        return;
    }
    if (event == input::kSelect) {
        if (s.find("menu") < 0) return;
        host.sound("CLICK", 1.0f, 0xF);
        if (f == nullptr) return;
        const int32_t entry = f->findUser(host.shared().onlineForms.optionsUser);
        if (s.menuSize("menu") == 0) {
            f->entryRelease(entry);
            host.request("online_friends", SwitchMode::Swap);
            return;
        }
        const std::string item = s.menuItemCommand("menu", s.selectedMenuItem("menu"));
        const std::wstring nick = u != nullptr ? wide(u->userNick(host.shared().onlineForms.optionsUser)) : std::wstring();
        const auto ask = [&](const std::wstring& text, int32_t& answer) {
            host.message(MessageType::YesNo, text, false, &answer, true);
            f->entryRelease(entry);
        };
        if (item == "accept_request") {
            f->answer(entry, 0);
            if (f->temporary(entry) != 0) {
                ask(texts.at(kTextSendRequest) + L": " + nick + L"?", m_addAnswer);
                return;
            }
        } else if (item == "decline_request") {
            f->answer(entry, 1);
        } else if (item == "block") {
            ask(texts.at(kTextBlockFrom) + L" " + nick + L"?", m_blockAnswer);
            return;
        } else if (item == "voice_mail") {
            if (u != nullptr && u->voiceMailRead != 0) {
                host.onlineVoice().askMail(host, host.shared().onlineForms.optionsUser, m_mail);
                host.shared().onlineForms.voiceBack = "online_friend_options";
                host.request("online_voice", SwitchMode::Swap);
                f->entryRelease(entry);
                return;
            }
            m_voicePlaying = 1;
            host.message(MessageType::Note, texts.at(kTextPleaseWait), false, nullptr);
            return;
        } else if (item == "accept_invite") {
            std::wstring text;
            if (net != nullptr && net->transport.inSession()) {
                text = texts.at(net->transport.hosting() ? kTextEndSession : kTextLeaveSession);
            } else {
                text = texts.at(kTextLoseProgress);
            }
            if (data != nullptr && data->race.mode == RaceMode::Career && data->career) {
                if (const CalendarEvent* e = currentEvent(data->career->calendar); e != nullptr && e->pinkSlip != 0) {
                    text = texts.at(kTextForfeit) + L"\n" + texts.at(kTextAreYouSure);
                }
            }
            // Stock problem: the friend's reference is not let go here.
            host.message(MessageType::YesNo, text, false, &m_acceptInviteAnswer, true);
            return;
        } else if (item == "decline_invite") {
            f->clearInvite(entry, 1);
        } else if (item == "remove_invite") {
            ask(texts.at(kTextRemove) + L" " + nick + L"?", m_declineAnswer);
            return;
        } else if (item == "join") {
            if (net != nullptr && net->transport.inSession()) {
                if (!net->transport.hosting()) {
                    m_joinAnswer = 1;
                    return;
                }
                host.message(MessageType::YesNo, texts.at(kTextEndSession), false, &m_joinAnswer, true);
                return;
            }
            host.message(MessageType::YesNo, texts.at(kTextLoseProgress), false, &m_joinAnswer, true);
            return;
        } else if (item == "invite") {
            const net::Matchmaking* mm = net::matchmaking();
            f->invite(entry, mm != nullptr ? mm->presenceAddress : std::nullopt, 0);
            host.request("online_friends", SwitchMode::Swap);
            f->entryRelease(entry);
            return;
        } else if (item == "cancel") {
            const net::Matchmaking* mm = net::matchmaking();
            f->cancelInvite(entry, mm != nullptr ? mm->presenceAddress : std::nullopt);
        } else if (item == "remove") {
            ask(nick + L": " + texts.at(kTextRemoveFriend), m_removeAnswer);
            return;
        } else if (item == "complaints") {
            s.clearMenu("menu");
            s.addCommandItem("menu", texts.at(kTextOffensive), 0.0f, "offensive_message");
            return;
        } else if (item == "offensive_message") {
            m_complaint = 2;
            host.message(MessageType::YesNo, texts.at(kTextSendComplaint), false, &m_complaintAnswer, true);
            return;
        }
        f->entryRelease(entry);
        host.request("online_friends", SwitchMode::Swap);
    } else if (event == input::kBack) {
        toFriends();
    }
}

}  // namespace oj::frontend
