// CMSOnlineCrewRaceSetup: the online career's crew race's drivers.

#include "Game/Frontend/Network/OnlineCrewRaceSetup.h"

#include "Core/Base/Text.h"
#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

namespace {

// 0x0049A850: each byte widened with its sign. Stock problem: a byte from 0x80 up (a description in the ANSI code
// page) becomes 0xFF80 .. 0xFFFF, not its character. The fixed build reads it in the names' code page.
std::wstring widenSigned(const std::string& text) {
    return oj::widenSigned(text);
}

std::wstring wide(std::u16string_view text) {
    return {text.begin(), text.end()};
}

}  // namespace

// 0x004D6090: +0x18 0; for drivers 1 .. 4 driver<n>value (found shown, 0x005A9A40) and selection<n> hidden; for each
// member driver<n>value its crewmate's short name shown, border<n> and crewpic<n> (its ui_crewmember_<number>) shown,
// past the members both left as they are; the slots (+0x0C .. +0x14) -1;
// Type the team's line (491 + team); the player (member 0) taken at once (0x004D6930); Choose "SELECT DRIVERS: " and
// the drivers left.
void OnlineCrewRaceSetup::enter(NetworkHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    const CareerProfile& p = career.profile;
    m_chosen = 0;
    for (int n = 1; n <= 4; ++n) {
        const std::string value = std::format("driver{}value", n);
        s.setHidden(std::format("selection{}", n), true);
        const int32_t k = n - 1;
        const auto number = k < p.memberCount ? static_cast<size_t>(career.members[static_cast<size_t>(k)])
                                               : career.crewmates.size();
        if (number < career.crewmates.size()) {
            const Crewmate& mate = career.crewmates[number];
            s.setText(value, oj::widenSigned(std::string_view(mate.shortName.data(), mate.shortName.size())));
            s.setHidden(value, false);
            s.setHidden(std::format("border{}", n), false);
            s.setTexture(std::format("crewpic{}", n), std::format("ui_crewmember_{:02}", mate.number));
            s.setHidden(std::format("crewpic{}", n), false);
        } else {
            // Stock problem: border<n> and crewpic<n> past the members are left as they are (the .ui's: a white
            // crewpic box with no picture in its border), unlike the offline window's (0x004D8AC0) hidden ones.
            s.setHidden(value, true);
        }
    }
    m_members.fill(-1);
    s.setText("Type", texts.at(kTextTeamSizes + static_cast<size_t>(std::max(onlineTeam(), 0))));
    take(host, s, 0);
    s.setText("Choose", texts.at(kTextSelectDrivers) + toWide(std::format("{}", onlineTeam() - m_chosen)));
}

// 0x004D6930: a full team "NO"; else the member in the first free slot (+0x18 up) and its selection<n> shown.
void OnlineCrewRaceSetup::take(NetworkHost& host, ui::Screen& s, int32_t member) {
    if (m_chosen == onlineTeam()) {
        host.sound("NO", 1.0f, 0xF);
        return;
    }
    for (int k = 0; k < onlineTeam() && k < 3; ++k) {
        if (m_members[static_cast<size_t>(k)] != -1) continue;
        m_members[static_cast<size_t>(k)] = member;
        ++m_chosen;
        s.setHidden(std::format("selection{}", member + 1), false);
        return;
    }
}

// 0x004D6550: every 0 .. 4 hidden, the one of the highlighted button's seventh character (1 .. 4, else 0) shown.
// Unless the network's checks (0x0046E0F0 with the stage 8) left, SELECT: Finished (0x004D67D0) with the team complete
// the members into the profile (+0x5F4; member 0 sets +0x614) and CMSOnlineRaceSetup; a driver: taken (0x004D6930)
// and with the team complete Finished shown and highlighted, or given back (0x004D68A0: not the player, member 0);
// Choose the drivers left. (No CLICK as the offline window's.)
void OnlineCrewRaceSetup::update(NetworkHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    CareerProfile& p = data->career->profile;
    const std::string button = s.currentButton();
    for (const char* name : {"0", "1", "2", "3", "4"}) hideAll(s, name);
    const char c = button.size() > 6 ? button[6] : '\0';
    static constexpr const char* kDigits[] = {"1", "2", "3", "4"};
    s.show(c >= '1' && c <= '4' ? kDigits[c - '1'] : "0", false);
    if (net::netGame() != nullptr && host.netKeys(event, net::SessionStage::Cars, true)) return;
    if (event != input::kSelect) return;
    const int team = std::clamp(onlineTeam(), 0, 3);
    if (_stricmp(button.c_str(), "finished") == 0) {
        if (m_chosen != onlineTeam()) return;
        for (int k = 0; k < team; ++k) {
            p.raceMembers[static_cast<size_t>(k)] = m_members[static_cast<size_t>(k)];
            if (m_members[static_cast<size_t>(k)] == 0) p.raceCarsChosen = 1;
        }
        host.request("CMSOnlineRaceSetup", SwitchMode::Replace);
        return;
    }
    const int member = c - '1';
    bool taken = false;
    for (int k = 0; k < team; ++k) taken = taken || m_members[static_cast<size_t>(k)] == member;
    if (!taken) {
        take(host, s, member);
        if (m_chosen == onlineTeam()) {
            s.setHidden("Finished", false);
            s.setCurrentButton("Finished");
        }
    } else if (member != 0) {
        for (int k = 0; k < team; ++k) {
            if (m_members[static_cast<size_t>(k)] != member) continue;
            m_members[static_cast<size_t>(k)] = -1;
            --m_chosen;
            s.setHidden(std::format("selection{}", member + 1), true);
        }
    }
    s.setText("Choose", texts.at(kTextSelectDrivers) + toWide(std::format("{}", onlineTeam() - m_chosen)));
}

}  // namespace oj::frontend
