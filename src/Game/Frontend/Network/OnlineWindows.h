#pragma once
// The GameSpy Presence windows (docs/spec/28-network.md, "GameSpy Presence").

#include "Core/Network/NetSession.h"
#include "Game/Frontend/Network/OnlineHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <string>

namespace oj::frontend {

// The window of the record of online_logon_gs (0x007217C8, vtable 0x006F2E4C).
class OnlineLogon {
public:
    // 0x004719F0
    void enter(WindowHost& host, ui::Screen& s);
    // 0x00471A60
    void update(OnlineHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_back = -1;     // +0x0C: BACK pressed (acted on the next frame)
    int32_t m_box = 9999;    // +0x10: a message's number never given
    float m_wait = 1.0f;     // +0x14
    int32_t m_leaving = 0;   // +0x18
};

// The window of the record of online_create_gs (0x007217E4, vtable 0x006F2E3C).
class OnlineCreate {
public:
    // 0x00471D70
    void enter(WindowHost& host, ui::Screen& s);
    // 0x00472070
    void update(OnlineHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_connecting = 0;  // +0x0C: the login's loop
    int32_t m_box = 999999;    // +0x10: "Logging into account..."'s number
    int32_t m_store = -1;      // +0x14: the account to be stored (the question's answer)
    int32_t m_submit = -1;     // +0x18: SUBMIT accepted
};

// The window of the record of online_connect_gs (0x00721800, vtable 0x006F2E0C).
class OnlineConnect {
public:
    // 0x00472760
    void enter(WindowHost& host, ui::Screen& s);
    // 0x00472A90
    void update(OnlineHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_connecting = 0;  // +0x0C
    int32_t m_changed = 0;     // +0x10: the account not the one stored
    int32_t m_box = 999999;    // +0x14
    int32_t m_store = -1;      // +0x18
    int32_t m_submit = -1;     // +0x1C
};

// The window of the record of online_main (0x0072185C, vtable 0x006F2EBC).
class OnlineMain {
public:
    // 0x00473EA0
    void enter(OnlineHost& host, ui::Screen& s);
    // 0x004740D0
    void update(OnlineHost& host, ui::Screen& s, uint32_t event);

private:
    // online_main's join of a session at an address (-connect, an invitation): everything reset (0x004EA4E0 with 1),
    // the matchmaking's join (0x004E58C0), joining, "JOINING GAME..." (type 0).
    void joinAt(OnlineHost& host, const net::Address& address);
    // 0x004E58C0: the re-query's attributes (12, one result) and the address asked through the master (the
    // matchmaking's +0x24 with 1).
    void join(const net::Address& address);
    // online_main's +0x0C: once no message moves in (0x005A7150), with the matchmaking's answers (0x004E6300) or its
    // list in (+0x0C) the join no longer waited for, the search stopped (0x004E62F0) and either the session at position
    // 0 joined (0x004734E0; joined) or the message closed and "This game session is no longer available." (type 1).
    void joining(OnlineHost& host);

    int32_t m_joining = 0;        // +0x0C: the matchmaking's join waited for
    int32_t m_joined = 0;         // +0x10: the session waited for
    float m_unusedTimer = 5.0f;      // +0x14: 5 by the enter, read by nothing
    int32_t m_unusedMessage = 99999; // +0x18: a message number, 99999 by the enter, read by nothing
    int32_t m_signOutAnswer = -1;  // +0x1C
};

// 0x00471920: the online services brought up (online_logon_gs's and online_connect_gs's enters).
void onlineBringUp(WindowHost& host);
// 0x00472940: the message up closed, the note of the text, the user signed out, the login's loop ended.
void onlineFailed(WindowHost& host, size_t text, int32_t& connecting);
// The keyboard set up for an account's field (the buffer written back on DONE); keyboard.
void onlineKeyboard(OnlineHost& host, size_t title, std::wstring& buffer, size_t most, bool checkName,
                    const char* allowed, bool password, const char* window);
// The edit buffers kept as the driver's account and "Logging into account..." (type 0).
void onlineStore(WindowHost& host);
// 0x00472030, the leave of online_create_gs, online_connect_gs and online_main: a message up closed.
void leaveOnlineForm(WindowHost& host);

}  // namespace oj::frontend
