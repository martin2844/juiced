#pragma once
#include "Core/Career/Career.h"
#if defined(OJ_MACOS) || defined(OJ_LINUX) || defined(OJ_ANDROID)
#include "Mods/Runtime.h"
#endif

namespace oj::frontend {
// This package delegates a single car, never adds extra titles to a duel.
inline bool pinkSlipCrewSelection(const CalendarEvent& event, const EventRound& round) {
#if defined(OJ_MACOS) || defined(OJ_LINUX) || defined(OJ_ANDROID)
    return event.pinkSlip != 0 && round.teamSize == 1 &&
           mods::runtime::feature("crew_pink_slips");
#else
    return false;
#endif
}
} // namespace oj::frontend
