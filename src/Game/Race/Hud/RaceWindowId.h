#pragma once
// The race front end's windows by name (docs/spec/22-race-hud.md): the window manager knows them by their names
// (0x004563E0 keeps them), the front end switches on which they are.

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace oj {

// A window of the race's front end: None before any, Other a name it has no window for (asked, logged, not made).
enum class RaceWindowId : int32_t {
    None,
    Other,
    Blank,
    BlankSplit,
    HudSolo,
    HudRace,
    HudSprint,
    HudCruise,
    HudSplit,
    Pause,
    RaceComplete,
    RaceResults,
    ChallengeResults,
    SprintResults,
    CruiseResults,
    Replay,
    RestartRace,
    StartReplay,
    RestartReplay,
    Closedown,
    Spectator,
    OptionsInGame,
    OnlineInGame,
    Autosave,
};

// Which window a name is (ignoring case as the window manager does; "" None); its name ("" for None and Other).
RaceWindowId raceWindowId(std::string_view name);
const char* raceWindowName(RaceWindowId id);

// The window the race front end shows: the window manager's name for it and which it is.
struct RaceWindowShown {
    std::string name;
    RaceWindowId id = RaceWindowId::None;

    RaceWindowShown() = default;
    explicit RaceWindowShown(std::string shown) : name(std::move(shown)), id(raceWindowId(name)) {}
};

}  // namespace oj
