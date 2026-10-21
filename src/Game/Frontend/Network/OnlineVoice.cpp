// The friends' and players' window online_voice of GameSpy (docs/spec/28-network.md), over online_bg.

#include "Game/Frontend/Network/OnlineVoice.h"

#include "Game/Frontend/Network/OnlineFriendInternal.h"

namespace oj::frontend {

using namespace onlinefriends;

// 0x0047DD50. The front end's music to nothing (its target, with the manager and its handle); the windows' enter;
// PLAYER_NAME the voice's user's nick; PROGRESS_BAR (+0x0C) at 0 (+0xEC, +0xF0), +0x1C 0. With the voice and the user
// object: outside a session (0x004E2780) our first local player (referenced) added to the voice (vt+0x18); the
// voice's +0x44 with it (referenced); mode 0 its +0x70. +0x14, +0x18 0, +0x20 -1.
void OnlineVoice::enter(OnlineHost& host, ui::Screen& s) {
    host.setMusicVolume(0.0f, true);
    net::OnlineUser* u = net::onlineUser();
    if (u != nullptr && host.shared().onlineForms.voiceUser != -1) s.setText("PLAYER_NAME", wide(u->userNick(host.shared().onlineForms.voiceUser)));
    m_progress = 0.0f;
    if (const int e = s.find("PROGRESS_BAR"); e >= 0) {
        s.setMeterShown("PROGRESS_BAR", 0.0f);
        s.setMeter(e, 0.0f);
    }
    if (net::VoiceChat* voice = net::gameVoice(); voice != nullptr && u != nullptr) {
        if (!matchmakingAddressed()) voice->addPlayer(localPlayerRef(u, 0));
        voice->openMail(localPlayerRef(u, 0));
        if (m_mode == VoiceMailMode::Heard) voice->stopMail(localPlayerRef(u, 0));
    }
    m_recorded = 0;
    m_sending = 0;
    m_unused = -1;
}

// 0x0047DCF0: a recording: with the voice and the user object its +0x70 with our first local player (referenced);
// the progress 0, PROGRESS_BAR at 0; not recorded.
void OnlineVoice::stop(ui::Screen& s) {
    if (m_recorded == 0) return;
    net::OnlineUser* u = net::onlineUser();
    if (net::VoiceChat* voice = net::gameVoice(); voice != nullptr && u != nullptr) {
        voice->stopMail(localPlayerRef(u, 0));
    }
    m_progress = 0.0f;
    if (const int e = s.find("PROGRESS_BAR"); e >= 0) {
        s.setMeterShown("PROGRESS_BAR", 0.0f);
        s.setMeter(e, 0.0f);
    }
    m_recorded = 0;
}

// 0x0047E020. Sent (+0x18): once nothing moves in over the window and the friends' list changed (+0x20) not sent, the
// message closed and the way back ([0x0075EEFC], mode 7). Else SEL shown; off mode 0 without a headset (the input
// manager's +0x30 and +0x34 of its device: none on the PC) the recording stopped (0x0047DCF0) and SEL hidden; the
// voice's +0x278 (a recording going on: never on the PC) adds the frame's time over its +0x58 milliseconds to the
// progress; PROGRESS_BAR the progress within 0 .. 1. Recorded and done: an invitation (mode 1, the friends' vt+0x6C)
// or a friend request (vt+0x28) with it, sent, "SENDING..." (1577, a note); sel_text "play" (mode 0), "send" while
// recording or "record"; back_text "cancel" when recorded, "back". SELECT (CLICK) with the voice and the user object:
// mode 0 not recording its +0x68 (play) and the progress 0; else recording its +0x70, or its +0x6C (record) starting:
// the progress 0, recorded. BACK (CLICK): recorded, stopped; else the way back (mode 7). (The voice's
// [0x0075F044] is read with no look at it: the window is the voice's.)
void OnlineVoice::update(OnlineHost& host, ui::Screen& s, uint32_t event) {
    const TextTable& texts = host.gameTexts();
    net::VoiceChat* voice = net::gameVoice();
    net::OnlineUser* u = net::onlineUser();
    net::OnlineFriends* f = net::onlineFriends();
    if (m_sending != 0) {
        if (host.onlineTopMoving()) return;
        if (f == nullptr || f->changed == 0) return;
        m_sending = 0;
        host.closeMessage();
        host.request(host.shared().onlineForms.voiceBack, SwitchMode::Swap);
        return;
    }
    s.showContaining("SEL", false);
    if (m_mode != VoiceMailMode::Heard) {
        stop(s);
        s.hideContaining("SEL");
    }
    const int32_t recording = voice != nullptr ? voice->recordingMail : 0;
    if (recording != 0) {
        const auto length = static_cast<uint32_t>(voice->mailLength());
        m_progress = host.frameTime() / (static_cast<float>(length) * 0.001f) + m_progress;
    }
    if (const int e = s.find("PROGRESS_BAR"); e >= 0) s.setMeter(e, std::clamp(m_progress, 0.0f, 1.0f));
    if (m_recorded != 0 && recording == 0) {
        if (f != nullptr) {
            if (m_mode == VoiceMailMode::Invitation) {
                f->invite(host.shared().onlineForms.voiceFriend, host.shared().onlineForms.voiceAddress, 1);
            } else {
                f->addFriend(host.shared().onlineForms.voiceUser, 1);
            }
        }
        m_sending = 1;
        host.message(MessageType::Note, texts.at(kTextSending), false, nullptr);
        return;
    }
    s.setText("sel_text", texts.at(m_mode == VoiceMailMode::Heard ? kTextPlay
                                   : recording != 0               ? kTextSend
                                                                  : kTextRecord));
    s.setText("back_text", texts.at(m_recorded != 0 ? kTextCancel : kTextBackWord));
    if (event == input::kSelect) {
        host.sound("CLICK", 1.0f, 0xF);
        if (voice == nullptr || u == nullptr) return;
        if (m_mode == VoiceMailMode::Heard) {
            if (recording != 0) return;
            voice->playMail(localPlayerRef(u, 0));
            m_progress = 0.0f;
            if (const int e = s.find("PROGRESS_BAR"); e >= 0) {
                s.setMeterShown("PROGRESS_BAR", 0.0f);
                s.setMeter(e, 0.0f);
            }
            return;
        }
        if (recording != 0) {
            voice->stopMail(localPlayerRef(u, 0));
            return;
        }
        if (voice->recordMail(localPlayerRef(u, 0)) != 0) {
            m_progress = 0.0f;
            if (const int e = s.find("PROGRESS_BAR"); e >= 0) {
                s.setMeterShown("PROGRESS_BAR", 0.0f);
                s.setMeter(e, 0.0f);
            }
            m_recorded = 1;
        }
    } else if (event == input::kBack) {
        host.sound("CLICK", 1.0f, 0xF);
        if (m_recorded != 0) {
            stop(s);
            return;
        }
        host.request(host.shared().onlineForms.voiceBack, SwitchMode::Swap);
    }
}

// 0x0047DEA0. The front end's music started again (0x0046BDD0) and, with the manager and its handle, its target the
// settings' music volume; the windows' leave (the Menu's item kept); the voice window's friend let go ([0x00715D34]
// -1), the address -1 ([0x00715D38]); with the voice and the user object: the user let go ([0x00715D3C] -1), the
// voice's +0x70 with our first local player (referenced), outside a session that player taken off the voice
// (vt+0x1C), its +0x48.
void OnlineVoice::leave(OnlineHost& host) {
    host.startMusic();
    host.setMusicVolume(std::bit_cast<float>(host.settingsValue(save::Settings::kMusicVolume)), true);
    if (net::OnlineFriends* f = net::onlineFriends()) {
        f->entryRelease(host.shared().onlineForms.voiceFriend);
        host.shared().onlineForms.voiceFriend = -1;
    }
    host.shared().onlineForms.voiceAddress.reset();
    net::OnlineUser* u = net::onlineUser();
    if (net::VoiceChat* voice = net::gameVoice(); voice != nullptr && u != nullptr) {
        u->userRelease(host.shared().onlineForms.voiceUser);
        host.shared().onlineForms.voiceUser = -1;
        voice->stopMail(localPlayerRef(u, 0));
        if (!matchmakingAddressed()) voice->removePlayer(localPlayerRef(u, 0));
        voice->closeMail(localPlayerRef(u, 0));
    }
}

// 0x0047DC20: the voice mail of the user to hear: the mode 0, the user ([0x00715D3C], referenced), the mail
// ([0x00715D40]).
void OnlineVoice::askMail(WindowHost& host, int32_t user, int32_t mail) {
    m_mode = VoiceMailMode::Heard;
    if (net::OnlineUser* u = net::onlineUser()) {
        u->userRelease(host.shared().onlineForms.voiceUser);
        u->userAddRef(user);
        host.shared().onlineForms.voiceUser = user;
    }
    host.shared().onlineForms.voiceMail = mail;
}

// 0x0047DC60: an invitation with a voice: the mode 1, the friend ([0x00715D34], referenced), the user (referenced) and
// our address ([0x00715D38]).
void OnlineVoice::askInvite(WindowHost& host, int32_t entry, int32_t user, const std::optional<net::Address>& address) {
    m_mode = VoiceMailMode::Invitation;
    if (net::OnlineFriends* f = net::onlineFriends()) {
        f->entryRelease(host.shared().onlineForms.voiceFriend);
        f->entryAddRef(entry);
        host.shared().onlineForms.voiceFriend = entry;
    }
    if (net::OnlineUser* u = net::onlineUser()) {
        u->userRelease(host.shared().onlineForms.voiceUser);
        u->userAddRef(user);
        host.shared().onlineForms.voiceUser = user;
    }
    host.shared().onlineForms.voiceAddress = address;
}

}  // namespace oj::frontend
