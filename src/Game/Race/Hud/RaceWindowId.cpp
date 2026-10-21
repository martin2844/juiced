#include "Game/Race/Hud/RaceWindowId.h"

#include <windows.h>

#include <array>
#include <utility>

namespace oj {

namespace {

constexpr std::array<std::pair<RaceWindowId, const char*>, 22> kNames{{
    {RaceWindowId::Blank, "blank"},
    {RaceWindowId::BlankSplit, "blank_split"},
    {RaceWindowId::HudSolo, "hud_solo"},
    {RaceWindowId::HudRace, "hud_race"},
    {RaceWindowId::HudSprint, "hud_sprint"},
    {RaceWindowId::HudCruise, "hud_cruise"},
    {RaceWindowId::HudSplit, "hud_split"},
    {RaceWindowId::Pause, "hud_pause"},
    {RaceWindowId::RaceComplete, "hud_race_complete"},
    {RaceWindowId::RaceResults, "hud_race_results"},
    {RaceWindowId::ChallengeResults, "hud_challenge_results"},
    {RaceWindowId::SprintResults, "hud_sprint_results"},
    {RaceWindowId::CruiseResults, "hud_cruise_results"},
    {RaceWindowId::Replay, "hud_replay"},
    {RaceWindowId::RestartRace, "restart_race"},
    {RaceWindowId::StartReplay, "start_replay"},
    {RaceWindowId::RestartReplay, "restart_replay"},
    {RaceWindowId::Closedown, "closedown"},
    {RaceWindowId::Spectator, "hud_spectator"},
    {RaceWindowId::OptionsInGame, "opts_ingame_bg"},
    {RaceWindowId::OnlineInGame, "online_ingame_bg"},
    {RaceWindowId::Autosave, "autosave"},
}};

}  // namespace

RaceWindowId raceWindowId(std::string_view name) {
    if (name.empty()) return RaceWindowId::None;
    const std::string text(name);
    for (const auto& [id, known] : kNames) {
        if (_stricmp(text.c_str(), known) == 0) return id;
    }
    return RaceWindowId::Other;
}

const char* raceWindowName(RaceWindowId id) {
    for (const auto& [known, name] : kNames) {
        if (known == id) return name;
    }
    return "";
}

}  // namespace oj
