#pragma once

#include "Core/Career/Career.h"
#include "Core/Data/GameTexts.h"
#include "Core/Data/Language.h"

#include <algorithm>
#include <string>

namespace oj::frontend {

inline std::wstring teamRaceCaption(const TextTable& texts, const EventRound& round) {
    const int team = std::clamp<int>(round.teamSize, 0, 3);
    std::wstring caption = texts.at(kTextTeamSizes + static_cast<size_t>(team));
    if (team != 2) return caption;

    // Calendar/driver selection can precede adding the player's crew. Match
    // RaceSetupWindow::go: add it if there is room, otherwise replace a rival.
    int drivers = round.drivers;
    const int limit = round.type == EventKind::Sprint ? 4 : 6;
    if (round.crews[0] == 0 && drivers + team <= limit) drivers += team;
    if (drivers != 4) return caption;

    // Retail only supplies the three-crew caption. Retain its translated words
    // and punctuation, shortening the numeric list for a two-crew event.
    for (const std::wstring pattern : {L"2,2,2", L"2-2-2"}) {
        const auto at = caption.find(pattern);
        if (at != std::wstring::npos) {
            caption.replace(at, pattern.size(), pattern.substr(0, 3));
            break;
        }
    }
    return caption;
}

}  // namespace oj::frontend
