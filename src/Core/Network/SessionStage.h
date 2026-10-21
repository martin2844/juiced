#pragma once
// The network session's stage (Core/Network/NetSession.h), which the race's rules read too.

#include <cstdint>

namespace oj::net {

// The session's stage ([0x0075EF90]; sent with the setup, 0x0A): the lobby, the race's kind and the setup's pages
// (instant_manu, instant_setup), the cars (link_selcar; the host starts 30 s after the first ready), loading, the
// machines loaded, the count (the clocks met, 3 s), the race.
enum class SessionStage : int32_t {
    Lobby = 0,
    RaceKind = 1,
    Location = 2,
    Difficulty = 3,
    Route = 4,
    Laps = 5,
    TimeOfDay = 6,
    Weather = 7,
    Cars = 8,
    Loading = 9,
    Loaded = 10,
    Countdown = 11,
    Racing = 12,
};

}  // namespace oj::net
