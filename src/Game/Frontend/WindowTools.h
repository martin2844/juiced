#pragma once
// What the front end's windows share of their own helpers: the stock code's static variables more than one window
// reads or writes and small functions on the windows and the game's data.

#include "Core/Data/GameTexts.h"

#include "Audio/SoundSystem.h"
#include "Core/Base/GameFiles.h"
#include "Core/Base/Random.h"
#include "Core/Base/Text.h"
#include "Core/Base/X87.h"
#include "Core/Career/CareerAdvice.h"
#include "Core/Career/CareerEvents.h"
#include "Core/Career/CareerInit.h"
#include "Core/Career/Money.h"
#include "Core/Career/Phone.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Cars/CarRepair.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/OnlineSessions.h"
#include "Core/Race/Arcade.h"
#include "Core/Race/RaceSession.h"
#include "Core/Saves/GraphicsConfig.h"
#include "Game/Frontend/Career/CharacterScreen.h"
#include "Game/Media/Movies.h"
#include "Game/Media/Music.h"
#include "Game/Platform/Platform.h"
#include "Game/Race/Hud/RaceWindows.h"
#include "Game/Frontend/WindowHost.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/UiSound.h"

#include <bit>
#include <format>

namespace oj::frontend {

// The pictures of the crews' leaders (0x007187B8).
inline constexpr const char* kLeaderPictures[] = {"ui_crewmember_00", "ui_leader_tk",    "ui_leader_biggi",
                                           "ui_leader_shystie", "ui_leader_maria", "ui_leader_jack",
                                           "ui_leader_melody",  "ui_leader_poppa", "ui_leader_carlos"};

inline bool is(const std::string& a, const char* b) {
    return _stricmp(a.c_str(), b) == 0;
}

inline Car* g_statsCar = nullptr;  // [0x00728C60]: the car Car_Stats2purple shows

// 0x0048CCD0: the make of each car model id (0 for an id past 52).
inline constexpr std::array<int, 53> kModelMakes = {
    0, 2, 1, 11, 1, 7, 3, 5, 6, 1, 10, 4, 14, 13, 15, 9, 8, 9, 12, 3, 1, 2, 1, 4, 1, 7, 15,
    16, 6, 3, 2, 2, 3, 1, 5, 5, 3, 1, 7, 13, 15, 2, 15, 13, 2, 13, 7, 17, 17, 17, 4, 9, 7,
};

// 0x0048CF20: the logo of each make 1 .. 17 ("ui_None" otherwise).
inline constexpr std::array<const char*, 18> kMakeLogos = {
    "ui_None",    "ui_Honda", "ui_Toyota", "ui_Mitsubishi", "ui_Mazda", "ui_Nissan", "ui_Subaru",
    "ui_Ford",    "ui_Fiat",  "ui_VW",     "ui_Peugeot",    "ui_Renault", "ui_opel", "ui_Chevy",
    "ui_Pontiac", "ui_Dodge", "ui_Holden", "ui_Acura",
};

inline int modelMake(int id) {
    return id >= 0 && id < static_cast<int>(kModelMakes.size()) ? kModelMakes[static_cast<size_t>(id)] : 0;
}

// The calendar's globals.
inline int32_t g_selectedDay = -1;       // [0x00728C5C]
inline int32_t g_selectedMonth = -1;     // [0x00728C58]

// Every element of the name hidden (the window's list in order).
inline void hideAll(ui::Screen& s, const char* name) {
    for (int i = 0; i < s.size(); ++i) {
        if (_stricmp(s.nameOf(i).c_str(), name) == 0) s.setHidden(i, true);
    }
}

// 0x0047FCA0: the ratings of a car class.
inline void classRatings(int cls, int& low, int& high) {
    static constexpr int kRanges[8][2] = {{700, 799}, {600, 699}, {500, 599}, {400, 499},
                                          {300, 399}, {200, 299}, {0, 199},   {0, 1000}};
    if (cls >= 1 && cls <= 8) {
        low = kRanges[cls - 1][0];
        high = kRanges[cls - 1][1];
    } else {
        low = 800;
        high = 1200;
    }
}

inline bool specialEvent(const CalendarEvent& e) {
    return e.challenge != 0 || e.pinkSlip != 0 || e.hostedByPlayer != 0;
}

// The GO item's value.
inline constexpr float kItemGo = 999.0f;

// The car's damage shown in the car menu (" (D)", 0x004D7470) and asked to be repaired: its grid damaged (0x0048EF50),
// or (0x0048EF10) its +0x74 or +0x7C over 0.1, or its nitro time not 15 with a nitro part.
inline bool gridDamaged(const Car& car) {
    return car.grid && damageLevel(*car.grid) > 0.0f;
}

inline bool partsWorn(const Car& car) {
    if (std::bit_cast<float>(car.tyreWear) > 0.1f || std::bit_cast<float>(car.brakeWear) > 0.1f) return true;
    return car.mods[kNitro] != 0xFF && std::bit_cast<float>(car.nitro) != 15.0f;
}

// 0x004B9C50: a crew's odds "<against>:<stake>", each cut.
inline std::string oddsText(const RaceSession& session, int crew) {
    const CrewOdds& o = session.odds[static_cast<size_t>(crew)];
    return std::format("{}:{}", x87::chop(o.against), x87::chop(o.stake));
}

// The online career's team (the session's +0x2C, [0x00728CEC]).
inline int onlineTeam() {
    return net::onlineHosted().career.team;
}

                                            // "This car's performance is maxed"
inline constexpr float kByteShare = 0.003921568859368563f;  // 1 / 255

// The level (+0x2B) of a car's part of a list; -1 for none (0xFF), the list's first past its end.
inline int modLevel(const CarPartList& list, uint8_t part) {
    if (part == 0xFF || list.parts.empty()) return -1;
    const auto i = static_cast<int8_t>(part);
    return list.parts[i >= 0 && static_cast<size_t>(i) < list.parts.size() ? static_cast<size_t>(i) : 0].level;
}

// The customising camera's offsets from the car (0x007203E8 the garage's, 0x00720408 .. 0x007204B8).
inline constexpr oj::Vec4 kCameraGarage{-233.37f, 115.84f, 493.664f, 0.0f};   // 0x007203E8

// 0x004990F0: a supplier's parts of a kind and level.
inline std::vector<const CarPart*> modBrandParts(const CarPartList& list, int brand, int level) {
    std::vector<const CarPart*> parts;
    for (const CarPart& part : list.parts) {
        if (static_cast<int8_t>(part.brand) == brand && part.level == level) parts.push_back(&part);
    }
    return parts;
}

template <size_t N>
bool listed(const std::string& state, const char* const (&names)[N]) {
    for (const char* n : names) {
        if (_stricmp(state.c_str(), n) == 0) return true;
    }
    return false;
}

// Every element of the name (case aside) hidden or shown.
inline void hideNamed(ui::Screen& s, const std::string& name, bool hidden) {
    for (int i = 0; i < s.size(); ++i) {
        if (_stricmp(s.nameOf(i).c_str(), name.c_str()) == 0) s.setHidden(i, hidden);
    }
}

// The front end's music down to a quarter of the settings' volume ([0x006FFBEC]) under a window's speech.
inline void musicQuarter(WindowHost& host) {
    host.setMusicVolume(host.settingsMusic() * 0.25f, false);
}

// The game text line of a route's name by its global (Route::text) and route 0's (the base).
size_t routeNameLine(uint32_t text, uint32_t base);

// 0x00473CD0: every ONLINEICON hidden; with the user object ONLINE reads "ONLINE: " (1493) and our first player's nick
// (20 characters at most; none before the login) and GAMESPY is shown; without, ONLINE's text emptied (0x005AA1F0).
void showOnlineStatus(ui::Screen& s, const TextTable& texts);

// 0x004A4EE0 (the main menu's and online_main's frames): an online career's pink slip settled shows its note and asks
// for the autosave, which the next frame opens over the window.
void pinkSlipNote(WindowHost& host);

}  // namespace oj::frontend
