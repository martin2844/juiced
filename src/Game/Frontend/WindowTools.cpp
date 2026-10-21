// The windows' helpers.

#include "Game/Frontend/WindowTools.h"

#include "Core/Data/GameTexts.h"
#include "Core/Network/GameSpy/GameSpyUser.h"

#include <algorithm>

namespace oj::frontend {

// The game text of a route's name by its global: the text loader (0x0040DB30) gives lines 585 .. 601 to the 16-byte
// globals from route 0's on (0x00761F70 in the retail build) and line 1972 to the one of "Point-to-point (r)"
// (0x00767550). The globals' addresses differ between the game's exes; route 0's is the base.
size_t routeNameLine(uint32_t text, uint32_t base) {
    if (text >= base && (text - base) / 16 <= 16) return 585 + (text - base) / 16;
    return 1972;
}

// 0x00473CD0: every ONLINEICON hidden; with the user object ONLINE reads "ONLINE: " (1493) and our first player's nick
// (20 characters at most; none before the login) and GAMESPY is shown; without, ONLINE's text emptied (0x005AA1F0).
void showOnlineStatus(ui::Screen& s, const TextTable& texts) {
    for (int e = 0; e < s.size(); ++e) {
        if (_stricmp(s.nameOf(e).c_str(), "ONLINEICON") == 0) s.setHidden(e, true);
    }
    const net::OnlineUser* u = net::onlineUser();
    if (u == nullptr) {
        if (s.find("ONLINE") >= 0) s.setText("ONLINE", L"");
        return;
    }
    const std::u16string& nick = u->playerNick(u->localPlayers.empty() ? -1 : u->localPlayers[0]);
    std::wstring text = texts.at(kTextOnline) + L": ";
    text.append(nick.begin(), nick.begin() + static_cast<std::ptrdiff_t>(std::min<size_t>(nick.size(), 20)));
    if (s.find("ONLINE") >= 0) s.setText("ONLINE", text);
    s.show("GAMESPY", false);
}

// 0x004A4EE0 (the main menu's and online_main's frames): an online career's pink slip settled (the career's +0x94)
// shows its note (type 1: 670 won or 671 lost by +0x98) and asks for the autosave (+0x9C 1, +0x94 0); asked, the next
// frame opens autosave over the window (mode 5) when the save system saves and no switch waits, and +0x9C goes to 0.
void pinkSlipNote(WindowHost& host) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    if (career.onlineSettled != 0) {
        host.message(MessageType::Continue, host.gameTexts().at(career.onlineWon != 0 ? kTextPinkWon : kTextPinkLost),
                     false, nullptr);
        career.onlineAutosave = 1;
        career.onlineSettled = 0;
    } else if (career.onlineAutosave != 0) {
        if (data->saving != 0) host.request("autosave", SwitchMode::Push);
        career.onlineAutosave = 0;
    }
}

}  // namespace oj::frontend
