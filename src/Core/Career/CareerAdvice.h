#pragma once
// The career's advisor (Career::advisor, the career's +0x78 object; docs/spec/08-career.md): which advice CMSAdvice
// gives and the calendar searches it rests on.

#include "Core/Career/Career.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Base/Random.h"

#include <cstdint>
#include <optional>

namespace oj {

// 0x004A6A70: the first waiting event (used, not raced) of a crew (byCrew) with a round of a kind (none: any, the
// stock's -1) the
// player may enter (0x004B7D80) for any reason (any) or with a reason other than 0, 1 and 7; none when there is none.
const CalendarEvent* findEvent(const Career& career, int crew, std::optional<EventKind> roundType, bool any,
                               bool byCrew, const PartCatalogue& catalogue);

// 0x004BC1D0: a crew's challenge not done, its leader in the phone's contacts and a garage car of the challenge's
// model: true, with that car's rating (cut toward zero) when asked.
bool challengeCar(const Career& career, const Crew& crew, int challenge, int32_t* rating);

// 0x004A6190: the first challenge of crews 1 .. 8 the player has a car for whose rating has not been told (5) or is
// better than the one told (4), kept in the advisor's +0x5C, +0x68 and +0x6C; 0 for none.
int32_t challengeAdvice(Career& career);

// 0x004A57B0: whether a challenge's advice is due (the advisor's +0x14).
bool adviceDue(Career& career);

// 0x004A57E0: the advice: 100 rounds of 13 candidates each worth a score; a candidate with a score over the best, not
// the advice given last, takes it when rand() % 10 is 5 (one rand() per candidate); a due challenge's advice then
// wins. The advice (13 for none) goes to the advisor's +0x1C with its event (event).
void chooseAdvice(Career& career, CrtRandom& random, const PartCatalogue& catalogue);

}  // namespace oj
