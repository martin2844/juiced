#pragma once
// What the friends' and players' windows share of their own helpers.

#include "Game/Frontend/Network/OnlineFriendWindows.h"

#include "Core/Base/Random.h"
#include "Core/Data/GameTexts.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/OnlineRecords.h"
#include "Core/Network/VoiceChat.h"
#include "Core/Race/RaceSession.h"
#include "Core/Data/JetTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <cmath>
#include <format>

namespace oj::frontend::onlinefriends {

// The presence icons (0x004E1670: the textures of [0x00728CA8] .. [0x00728CB8], made once).
constexpr const char* kIconFriendRequestFrom = "live_friendinvite_rec";
constexpr const char* kIconFriendRequestSent = "live_friendinvite_sent";
constexpr const char* kIconInviteFrom = "live_gameinvite_rec";
constexpr const char* kIconInviteSent = "live_gameinvite_sent";
constexpr const char* kIconFriendOnline = "live_friendonline";

// The friends' list refreshed each second ([0x006FF708]).
constexpr float kRefresh = 1.0f;

inline std::wstring wide(std::u16string_view text) {
    return std::wstring(text.begin(), text.end());
}

// The low bytes of a nick's characters (the names' item names, 0x0047AC70).
inline std::string lowBytes(std::u16string_view text) {
    std::string s;
    for (char16_t c : text) s.push_back(static_cast<char>(c & 0xFF));
    return s;
}

// A user kept as an item's value (+0x24, an int) and read back.
inline float userValue(int32_t user) {
    return std::bit_cast<float>(user);
}
inline int32_t itemUser(const ui::Screen& s, const std::string& menu, size_t index) {
    return std::bit_cast<int32_t>(s.menuItemValue(menu, index));
}

// At most 15 characters of a nick (0x0047A3B4's copy into 16).
inline std::wstring nick15(std::u16string_view nick) {
    return wide(nick.substr(0, std::min<size_t>(nick.size(), 15)));
}

// 0x0055BA30 / 0x0055BA00: the user object's local player of the index (-1 none); 0x0055BA00 references it.
inline int32_t localPlayer(const net::OnlineUser* user, size_t index) {
    return user != nullptr && index < user->localPlayers.size() ? user->localPlayers[index] : -1;
}
inline int32_t localPlayerRef(net::OnlineUser* user, size_t index) {
    const int32_t p = localPlayer(user, index);
    if (user != nullptr && p != -1) user->playerAddRef(p);
    return p;
}

// 0x004E2780: the matchmaking with a session's address (its +0x50; the stock references the address's entry each
// time and never lets it go).
inline bool matchmakingAddressed() {
    const net::Matchmaking* mm = net::matchmaking();
    return mm != nullptr && mm->presenceAddress.has_value();
}

// 0x00479B40's and 0x0047B2C0's: a user's record (asked for) in RATING, RANK ("-" for 0), WON, LOST (the races, won
// and lost) and DISCONNECTS (in percent of the races, unsigned, cut toward zero), else "-" in each.
inline void showRecord(ui::Screen& s, int32_t user) {
    const net::stats::UserRecord* r = net::stats::record(user);
    const auto set = [&](const char* name, const std::wstring& text) {
        if (const int e = s.find(name); e >= 0) s.setText(e, text);
    };
    if (r == nullptr || r->asked == 0) {
        for (const char* name : {"RATING", "RANK", "WON", "LOST", "DISCONNECTS"}) set(name, L"-");
        return;
    }
    set("RATING", std::format(L"{}", r->rating));
    set("RANK", r->rank == 0 ? std::wstring(L"-") : std::format(L"{}", r->rank));
    set("WON", std::format(L"{}", r->won));
    const int32_t races = r->won + r->lost;
    set("LOST", std::format(L"{}", races));
    int32_t percent = 0;
    if (races != 0) {
        const auto disconnects = static_cast<float>(static_cast<uint32_t>(r->disconnects));
        percent = static_cast<int32_t>(disconnects / static_cast<float>(static_cast<uint32_t>(races)) * 100.0f);
    }
    set("DISCONNECTS", std::format(L"{}", percent));
}

}  // namespace oj::frontend::onlinefriends
