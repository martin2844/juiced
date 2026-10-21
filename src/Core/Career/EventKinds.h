#pragma once
// What an event's round races and a challenge's kind (the career's calendar, its crews' challenges, the
// arcade's races).

#include <cstdint>

namespace oj {

// What a calendar event's round races (the round's +0x04, the event's +0x10; the arcade's races too): the game's lines
// 140 + kind name it ("Crew challenge" .. "Mixed"); in the arcade Challenge is a challenge of its own.
enum class EventKind : int32_t {
    Challenge = 0,  // a crew's challenge
    PinkSlip = 1,
    Showoff = 2,
    PointToPoint = 3,
    Circuit = 4,
    Sprint = 5,
    Mixed = 6,
};

// A crew challenge's kind (CMSCrewChallenges.txt's word: LapTime, PerfectLap, Speed, Cruise; Other for another word);
// the game's lines kTextChallengeTypes + kind name it ("LAP TIME CHALLENGE" ..).
enum class ChallengeType : int32_t { LapTime = 0, PerfectLap = 1, Speed = 2, Showoff = 3, Other = 4 };

}  // namespace oj
