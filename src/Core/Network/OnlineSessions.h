#pragma once
// The online sessions' settings and the game's side of the matchmaking (docs/spec/28-network.md, "The session
// windows"): the profile's online block (the settings' +0x21C .. +0x2D3), the sessions hosted (0x00728CC0,
// 0x00728D08), the matchmaking's calls of the windows (host 0x004E1AE0 / 0x004E20F0, the searches 0x004E27D0 /
// 0x004E3300 and their filters, the re-query 0x004E4E90, the readers 0x004E6440 / 0x004E64F0 / 0x004E6570), the
// options' cycles and the online career's checks of the garage (0x004A4590 .. 0x004A48D0, 0x004A9A70, 0x004A9AF0).

#include "Core/Network/Matchmaking.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace oj {
struct Career;
}

namespace oj::save {
struct Settings;
}

namespace oj::net {

// "Any" of the options (9999).
constexpr int32_t kAny = 9999;

// An arcade session (0x38 bytes; the profile's +0x270, the settings' +0x21C; the hosted copy 0x00728D08).
struct ArcadeSession {
    int32_t access = 0;      // +0x00: 0 public, 1 private (key52)
    std::array<char16_t, 16> name{};  // +0x04: the host's nick, 0 at +0x22 (attribute 0)
    int32_t playing = 0;     // +0x24: the race on (attribute 1)
    int32_t career = 0;      // +0x28: 0 (attribute 2)
    int32_t level = kAny;    // +0x2C: the performance group 0 .. 7 (attribute 4)
    int32_t mods = 1;        // +0x30: 0 none, 1 standard, 2 prototype (attribute 3)
    int32_t collision = 1;   // +0x34 (attribute 11)

    static ArcadeSession read(const save::Settings& settings);  // +0x21C
    void write(save::Settings& settings) const;
};

// A career session (0x48 bytes; the profile's +0x2BC, the settings' +0x268; the hosted copy 0x00728CC0).
struct CareerSession {
    int32_t access = 0;        // +0x00
    std::array<char16_t, 16> name{};  // +0x04
    int32_t playing = 0;       // +0x24
    int32_t career = 0;        // +0x28: 1 when hosted (attribute 2)
    int32_t team = 1;          // +0x2C: 1 solo, 2 "2,2,2", 3 "3 on 3" (attribute 6)
    int32_t lower = 7;         // +0x30: the lower class 0 .. 7 (attribute 7)
    int32_t upper = 0;         // +0x34: the upper class (attribute 8)
    int32_t criteriaType = 0;  // +0x38: 0 none, 1 territory, 2 make, 3 model (attribute 9)
    int32_t criteria = 0;      // +0x3C (attribute 10)
    int32_t pinkslip = 0;      // +0x40 (attribute 5)
    int32_t collision = 1;     // +0x44 (attribute 11)

    static CareerSession read(const save::Settings& settings);  // +0x268
    void write(save::Settings& settings) const;
};

// The arcade search's settings (5 dwords; the profile's +0x2A8, the settings' +0x254).
struct ArcadeSearch {
    int32_t minPlayers = 1;    // +0x00: 1 .. 5
    int32_t status = kAny;     // +0x04: 0 waiting, 1 racing
    int32_t level = kAny;      // +0x08
    int32_t mods = kAny;       // +0x0C
    int32_t collision = kAny;  // +0x10

    static ArcadeSearch read(const save::Settings& settings);
    void write(save::Settings& settings) const;  // 0x004716D0
};

// The career search's settings (9 dwords; the profile's +0x304, the settings' +0x2B0).
struct CareerSearch {
    int32_t minPlayers = 1;       // +0x00
    int32_t status = kAny;        // +0x04
    int32_t team = kAny;          // +0x08
    int32_t lower = 7;            // +0x0C
    int32_t upper = 0;            // +0x10
    int32_t criteriaType = kAny;  // +0x14
    int32_t criteria = kAny;      // +0x18
    int32_t pinkslip = 0;         // +0x1C
    int32_t collision = kAny;     // +0x20

    static CareerSearch read(const save::Settings& settings);
    void write(save::Settings& settings) const;
};

// What the online windows keep that the session reads: [0x0075EF88] the online mode (0 arcade, 1 career) and the
// sessions hosted or joined (the arcade one 0x00728D08, its name 0x00728D0C; the career one 0x00728CC0, its name
// 0x00728CC4, its team 0x00728CEC and its pink slip 0x00728D00), written by the hosts (0x004E1AE0, 0x004E20F0) and
// the joins (0x00473190, 0x004732D0).
// The online kind of the session hosted or joined ([0x0075EF88]).
enum class OnlineKind : int32_t { Arcade = 0, Career = 1 };

struct OnlineHosted {
    OnlineKind mode = OnlineKind::Arcade;
    ArcadeSession arcade;
    CareerSession career;
};
OnlineHosted& onlineHosted();

// The GameSpy user object's side the session windows call ([0x0075F040], Core/Network/GameSpy/GameSpyUser): it is there; its local
// players (+0x14 .. +0x18); a player's nick (vt+0x34, 0x0055F5D0); the players' state (0x004EEAF0: vt+0x40 of each
// local player, 0x0055F680, with the state and the session being public — that third argument taken and unused).
bool onlineUsersPresent();
std::vector<int32_t> onlineUserIds();
std::u16string onlineNick(int32_t id);
void setOnlineUsersState(int32_t state, int32_t isPublic);

// The text of a name (to its first 0).
std::u16string nameText(const std::array<char16_t, 16>& name);
// A text as a name: 15 characters at most, the rest 0.
std::array<char16_t, 16> nameOf(std::u16string_view text);

// 0x0047FCA0: the bhp of a class: 1 .. 7 700..799 down to 0..199, 8 0..1000, anything else (0 too) 800..1200.
void classBhp(int32_t index, int32_t& lo, int32_t& hi);

// The searches' filters (0x004E27D0 and 0x004E3300 before 0x00564500).
std::string arcadeFilter(const ArcadeSearch& search, bool quick);
std::string careerFilter(const CareerSearch& search, bool quick);

// The attributes the hosts give (12: the name, then ids 1 .. 11) and the search's (the arcade's 6, the career's 10,
// the re-query's 12).
std::vector<MatchAttribute> arcadeAttributes(const ArcadeSession& session);
std::vector<MatchAttribute> careerAttributes(const CareerSession& session);
std::vector<SearchAttribute> arcadeSearchAttributes();
std::vector<SearchAttribute> careerSearchAttributes();
std::vector<SearchAttribute> requeryAttributes();

// 0x004E1AE0 / 0x004E20F0: the session hosted with its attributes (SetInfo with "Arcade" / "Career", Host: the arcade
// 1 player and 5 open; the career a pink slip 1 and 1, else its team and 6 less it), then copied as the hosted one
// (0x00728D08 / 0x00728CC0) whatever came of it; Host's answer (no matchmaking: false).
bool hostArcade(const ArcadeSession& session);
bool hostCareer(const CareerSession& session);
// 0x004E27D0 / 0x004E3300: the filter, the search's attributes (5 results at most for a quick match, else 25) and the
// search started (its arguments 2 / 3 and the values, unused by the GameSpy matchmaking).
void searchArcade(const ArcadeSearch& search, bool quick);
void searchCareer(const CareerSearch& search, bool quick);
// 0x004E4E90: the result at a sorted position asked again alone (12 attributes, 1 result).
void requery(int32_t position);
// 0x004E4460: with the matchmaking a friend's session asked (the twelve attributes of a re-query, one result; MM+0x1C
// with the friend's entry).
void searchFriendSession(int32_t entry);
// 0x004E6440, 0x004E64F0, 0x004E6570: a result's settings at a sorted position (key52 into the access, the attributes
// of the ids into their fields; the fields of absent attributes kept); false without the result.
bool readSession(int32_t position, int32_t& access, std::array<char16_t, 16>& name, int32_t& playing, int32_t& career);
bool readArcade(int32_t position, ArcadeSession& session);
bool readCareer(int32_t position, CareerSession& session);

// The options' left / right steps (dir -1 / +1) of the session windows.
// The performance group: Any -> right 0, left 7; else out of 0 .. 7 -> Any.
int32_t stepLevel(int32_t value, int32_t dir);
// The create's mods: 0 .. 2 round.
int32_t stepCreateMods(int32_t value, int32_t dir);
// The searches' status and collision: right 0 -> 1 -> Any -> 0, left the other way.
int32_t stepAnyOfTwo(int32_t value, int32_t dir);
// The arcade search's mods: right 0 -> 1 -> 2 -> Any -> 0, left the other way.
int32_t stepSearchMods(int32_t value, int32_t dir);
// The career create's team: 1 .. 3 round.
int32_t stepCreateTeam(int32_t value, int32_t dir);
// The career create's criteria of a type (1 .. 3, 1 .. 17, 1 .. 52 round; type 0 kept).
int32_t stepCreateCriteria(int32_t type, int32_t value, int32_t dir);
// The career search's team: right 1 -> 2 -> 3 -> Any -> 1, left the other way.
int32_t stepSearchTeam(int32_t value, int32_t dir);
// The career search's criteria type: right 0 -> 1 -> 2 -> 3 -> Any -> 0; left Any -> 3, 2 -> 1, 1 -> 0, 0 -> Any and
// (Stock problem) 3 -> 3.
int32_t stepSearchCriteriaType(int32_t value, int32_t dir);
// The career search's criteria of a type (types 1 .. 3 to 3, 17, 52; any other 0): Any -> left the most, right 1; out
// of 1 .. the most -> Any. Type 0 kept.
int32_t stepSearchCriteria(int32_t type, int32_t value, int32_t dir);

// The online career's checks of the garage (the career's +0x84).
// 0x004A4830: the garage's cars of a class within upper .. lower that match the criteria (1 the territory, 2 the make,
// 3 the model; others any) at least the team (over 100: 1).
bool garageHasRange(const Career& career, int32_t team, int32_t lower, int32_t upper, int32_t type, int32_t criteria);
// 0x004A48D0: with the type 1, 2 or 3 the garage's cars of the criteria at least the team (over 100: 1); others pass.
bool garageHasCriteria(const Career& career, int32_t team, int32_t criteria, int32_t type);
// 0x004A9AF0 / 0x004A9A70 on the player's crew (its +0x78 0: the garage) and the class of the rating (0x00447480): the
// least rating's (10 for none) and the most's (0 for none).
int32_t weakestClass(const Career& career);
int32_t strongestClass(const Career& career);
// 0x004A4590 (the career create's and a join's settings) and 0x004A46F0 (the career search's): the garage holds a
// car, the crew enough mates for the team (over 100: 1), the class range and the criteria the cars, and a pink slip
// fewer than 55 cars. The text of the note when one fails: 665 no car, 661 the crew, 662 the range, 663 the territory
// (type 1) or 664 (others), 1614 the 55 cars. Without notes (0x004A4590 with 0) the 55 cars pass (Stock problem).
// (Both pass at once when the front end's window manager is missing, [G+0x3C] else [G+0x38] +8: never in the menus.)
bool careerCheck(const Career& career, int32_t team, int32_t lower, int32_t upper, int32_t type, int32_t criteria,
                 int32_t pinkslip, bool notes, size_t* text);

}  // namespace oj::net
