// The friends' and players' window online_player_options of GameSpy (docs/spec/28-network.md), over online_bg.

#include "Game/Frontend/Network/OnlinePlayerOptions.h"

#include "Game/Frontend/Network/OnlineFriendInternal.h"

namespace oj::frontend {

using namespace onlinefriends;

// 0x0047A380. The windows' enter; player_name the target's nick (15 characters) or empty; "menu" emptied; with the
// friends "Send friend request" (send) for a user not among them, or "Cancel friend request" (cancel) for our request
// waiting with the friends' +0x34 (0); with the voice ([0x0075F044]) "Remove mute" (unmute) for a user the options'
// player muted (0x0055C360), else "Mute" (mute); with the user object's
// +0x58 (0) "Positive feedback" and "Complaints"; the first item highlighted; +0x0C, +0x10 -1.
void OnlinePlayerOptions::enter(OnlineHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    net::OnlineUser* u = net::onlineUser();
    if (host.shared().onlineForms.optionsUser != -1 && u != nullptr) {
        s.setText("player_name", nick15(u->userNick(host.shared().onlineForms.optionsUser)));
    } else {
        s.setText("player_name", L"");
    }
    if (s.find("menu") >= 0) {
        s.clearMenu("menu");
        if (net::OnlineFriends* f = net::onlineFriends()) {
            const int32_t entry = f->findUser(host.shared().onlineForms.optionsUser);
            if (entry == -1) {
                s.addCommandItem("menu", texts.at(kTextSendRequest), 0.0f, "send");
            } else if (f->canCancelRequest() != 0 && f->requestSent(entry) != 0) {
                s.addCommandItem("menu", texts.at(kTextCancelRequest), 0.0f, "cancel");
            }
            f->entryRelease(entry);
        }
        if (net::VoiceChat* voice = net::gameVoice()) {
            if (voice->isMuted(host.shared().onlineForms.optionsPlayer, host.shared().onlineForms.optionsUser)) {
                s.addCommandItem("menu", texts.at(kTextUnmute), 0.0f, "unmute");
            } else {
                s.addCommandItem("menu", texts.at(kTextMute), 0.0f, "mute");
            }
        }
        if (u != nullptr && u->feedbackOn() != 0) {
            s.addCommandItem("menu", texts.at(kTextPositive), 0.0f, "positive");
            s.addCommandItem("menu", texts.at(kTextComplaints), 0.0f, "complaints");
        }
        s.selectMenuItem("menu", 0);
    }
    m_voiceAnswer = -1;
    m_complaintAnswer = -1;
}

// 0x0047A540: the complaint's kind (+0x14) and "Send complaint. Are you sure?" (type 3, starting on no) into +0x10.
void OnlinePlayerOptions::complaint(OnlineHost& host, int32_t kind) {
    const TextTable& texts = host.gameTexts();
    m_complaint = kind;
    host.message(MessageType::YesNo, texts.at(kTextSendComplaint), false, &m_complaintAnswer, true);
}

// 0x0047A5C0. The transport failed with the user object: the local players' state 0. +0x0C (never asked): yes the
// voice window's mode 2 ([0x007217B4]) and user ([0x00715D3C]), CLICK, its way back online_players (0x00471700) and
// online_voice (mode 7); no our request sent (vt+0x28), CLICK, online_players (mode 7). +0x10 (the complaint): yes
// sent (the user object's +0x5C with the kind), CLICK, online_players; no CLICK. In the network game out of a race
// past the cars' stage: the target none, the friends' +0x24, loading. SELECT (CLICK): send: our request (vt+0x28),
// online_players (mode 7) and CLICK again; cancel: the friend gone (vt+0x30); with the voice mute / unmute the user
// for the options' player (vt+0x30, vt+0x34); with the user object positive / complaints the menu's items of their
// kinds (good_attitude 6,
// good_gameplay 7; bad_name 0, cheating 5, screaming 2, harassment 3, lewdness 4: the question 0x0047A540); any other
// online_players (mode 7). BACK: CLICK, online_players (mode 7).
void OnlinePlayerOptions::update(OnlineHost& host, ui::Screen& s, uint32_t event) {
    const TextTable& texts = host.gameTexts();
    net::NetGame* net = net::netGame();
    net::OnlineUser* u = net::onlineUser();
    if (net != nullptr && net->transport.state() != net::State::Normal && u != nullptr) net::setOnlineUsersState(0, 0);
    if (m_voiceAnswer == 1) {
        m_voiceAnswer = -1;
        host.onlineVoice().askRequest();
        if (u != nullptr) {
            u->userRelease(host.shared().onlineForms.voiceUser);
            u->userAddRef(host.shared().onlineForms.optionsUser);
            host.shared().onlineForms.voiceUser = host.shared().onlineForms.optionsUser;
        }
        host.sound("CLICK", 1.0f, 0xF);
        host.shared().onlineForms.voiceBack = "online_players";
        host.request("online_voice", SwitchMode::Swap);
        return;
    }
    if (m_voiceAnswer == 0) {
        m_voiceAnswer = -1;
        if (net::OnlineFriends* f = net::onlineFriends()) f->addFriend(host.shared().onlineForms.optionsUser, 0);
        host.sound("CLICK", 1.0f, 0xF);
        host.request("online_players", SwitchMode::Swap);
        return;
    }
    if (m_complaintAnswer == 1) {
        m_complaintAnswer = -1;
        if (u != nullptr) u->sendFeedback(host.shared().onlineForms.optionsPlayer, host.shared().onlineForms.optionsUser, m_complaint);
        host.sound("CLICK", 1.0f, 0xF);
        host.request("online_players", SwitchMode::Swap);
        return;
    }
    if (m_complaintAnswer == 0) {
        m_complaintAnswer = -1;
        host.sound("CLICK", 1.0f, 0xF);
        return;
    }
    if (net != nullptr && !host.racePause() && net->session.stage() > net::SessionStage::Cars) {
        onlineOptionsTarget(host, -1, -1);
        if (net::OnlineFriends* f = net::onlineFriends()) f->windowsLeft();
        host.request("loading", SwitchMode::Replace);
        return;
    }
    if (event == input::kSelect) {
        if (s.find("menu") < 0 || s.menuSize("menu") == 0) return;
        host.sound("CLICK", 1.0f, 0xF);
        const std::string item = s.menuItemCommand("menu", s.selectedMenuItem("menu"));
        if (net::OnlineFriends* f = net::onlineFriends()) {
            const int32_t entry = f->findUser(host.shared().onlineForms.optionsUser);
            if (item == "send") {
                f->addFriend(host.shared().onlineForms.optionsUser, 0);
                host.request("online_players", SwitchMode::Swap);
                host.sound("CLICK", 1.0f, 0xF);
                return;
            }
            if (item == "cancel") f->remove(entry);
            f->entryRelease(entry);
        }
        if (net::VoiceChat* voice = net::gameVoice()) {
            if (_stricmp(item.c_str(), "mute") == 0) {
                voice->mute(host.shared().onlineForms.optionsPlayer, host.shared().onlineForms.optionsUser);
            } else if (_stricmp(item.c_str(), "unmute") == 0) {
                voice->unmute(host.shared().onlineForms.optionsPlayer, host.shared().onlineForms.optionsUser);
            }
        }
        if (u != nullptr) {
            const auto kinds = [&](std::initializer_list<std::pair<size_t, const char*>> items) {
                s.clearMenu("menu");
                for (const auto& [text, command] : items) s.addCommandItem("menu", texts.at(text), 0.0f, command);
            };
            if (item == "positive") {
                kinds({{kTextGoodAttitude, "good_attitude"}, {kTextGoodGameplay, "good_gameplay"}});
                return;
            }
            if (item == "complaints") {
                kinds({{kTextBadName, "bad_name"}, {kTextCheating, "cheating"}, {kTextScreaming, "screaming"},
                       {kTextHarassment, "harassment"}, {kTextLewdness, "lewdness"}});
                return;
            }
            if (item == "good_attitude" || item == "good_gameplay") {
                u->sendFeedback(host.shared().onlineForms.optionsPlayer, host.shared().onlineForms.optionsUser, item == "good_attitude" ? 6 : 7);
            } else if (item == "bad_name") {
                complaint(host, 0);
                return;
            } else if (item == "cheating") {
                complaint(host, 5);
                return;
            } else if (item == "screaming") {
                complaint(host, 2);
                return;
            } else if (item == "harassment") {
                complaint(host, 3);
                return;
            } else if (item == "lewdness") {
                complaint(host, 4);
                return;
            }
        }
        host.request("online_players", SwitchMode::Swap);
    } else if (event == input::kBack) {
        host.sound("CLICK", 1.0f, 0xF);
        host.request("online_players", SwitchMode::Swap);
    }
}

}  // namespace oj::frontend
