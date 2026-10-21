#include "Core/Career/CareerAdvice.h"

#include "Core/Career/CareerEvents.h"
#include "Core/Base/X87.h"

#include <bit>
#include <cstring>

namespace oj {

namespace {

// The type of an event's round r: the round's own for r 0; past it the game reads the bytes after the round at its
// place, here those of the calendar's events as they lie in it, 0 past them. No event keeps more than one round
// (0x004A33E0 adds one; a hosted event of two ends the game at 0x004CD6F0's return), so r is 0.
EventKind eventRoundType(const Calendar& calendar, size_t event, int r) {
    if (r == 0) return calendar.events[event].round.type;
    const size_t at = event * sizeof(CalendarEvent) + 0x20 + static_cast<size_t>(r) * sizeof(EventRound);
    if (at + 4 > calendar.events.size() * sizeof(CalendarEvent)) return EventKind::Challenge;
    int32_t v = 0;
    std::memcpy(&v, reinterpret_cast<const uint8_t*>(calendar.events.data()) + at, 4);
    return static_cast<EventKind>(v);
}

// A crew's steps as the advice weighs them: 4 without its second step, then `third` without its third, then `done`
// with its fourth, else `notDone`.
int32_t steps(const Crew& crew, int32_t third, int32_t done, int32_t notDone) {
    if (crew.stage[1] == 0) return 4;
    if (crew.stage[2] == 0) return third;
    return crew.stage[3] != 0 ? done : notDone;
}

// The first waiting event the player may enter with a reason other than 0, 1 and 7 (0x004A5ED0, 0x004A5FA0).
const CalendarEvent* firstOpen(const Career& career, const PartCatalogue& catalogue) {
    for (const CalendarEvent& e : career.calendar.events) {
        if (e.used == 0 || e.done != 0) continue;
        int reason = 0;
        if (!mayEnter(career, e, reason, catalogue)) continue;
        if (reason != 1 && reason != 0 && reason != 7) return &e;
    }
    return nullptr;
}

const CalendarEvent* findEither(const Career& career, int crew, EventKind a, EventKind b, bool byCrew,
                                const PartCatalogue& catalogue) {
    const CalendarEvent* e = findEvent(career, crew, a, false, byCrew, catalogue);
    return e != nullptr ? e : findEvent(career, crew, b, false, byCrew, catalogue);
}

// The candidates' scores (each its event in `event`).

// 8: crew 1's event with a round of type 3 or 4, with less than 10000: 6.
int32_t lowCash(const Career& career, const CalendarEvent*& event, const PartCatalogue& catalogue) {
    event = findEither(career, 1, EventKind::PointToPoint, EventKind::Circuit, true, catalogue);
    if (event == nullptr) return 0;
    return career.profile.money >= 10000 ? 0 : 6;
}

// 12: without the advisor's +0x60, over 100000 6, over 50000 5.
int32_t hosting(const Career& career) {
    if (career.advisor.hosted != 0) return 0;
    const auto money = static_cast<float>(career.profile.money);
    if (money > 100000.0f) return 6;
    if (money > 50000.0f) return 5;
    return 0;
}

// 0: an event with a round of type 4 or 3; crew 1's steps.
int32_t crewOne(const Career& career, const CalendarEvent*& event, const PartCatalogue& catalogue) {
    event = findEither(career, 1, EventKind::Circuit, EventKind::PointToPoint, false, catalogue);
    return event != nullptr ? steps(career.crews[1], 4, 1, 3) : 0;
}

// 1 (0x004A5DF0): with crew 2's respect under 350 an event with a round of type 4, else (or none) of type 5; crew
// 2's steps.
int32_t crewTwo(const Career& career, const CalendarEvent*& event, const PartCatalogue& catalogue) {
    event = nullptr;
    if (career.crews[2].respect < 350) event = findEvent(career, 2, EventKind::Circuit, false, false, catalogue);
    if (event == nullptr) event = findEvent(career, 2, EventKind::Sprint, false, false, catalogue);
    return event != nullptr ? steps(career.crews[2], 3, 1, 2) : 0;
}

// 2: an event with a round of type 2; crew 3's steps.
int32_t crewThree(const Career& career, const CalendarEvent*& event, const PartCatalogue& catalogue) {
    event = findEvent(career, 3, EventKind::Showoff, false, false, catalogue);
    return event != nullptr ? steps(career.crews[3], 3, 0, 2) : 0;
}

// 10 (0x004A6240): a prototype part event (+0x70) the player may enter with a garage car of its class whose part of
// the event's kind is none or not a prototype part: 5.
int32_t prototype(const Career& career, const CalendarEvent*& event, const PartCatalogue& catalogue) {
    for (const CalendarEvent& e : career.calendar.events) {
        if (e.used == 0 || e.done != 0 || e.asksPrototype == 0) continue;
        int reason = 0;
        if (!mayEnter(career, e, reason, catalogue) || reason == 1 || reason == 0 || reason == 7) continue;
        for (const Car& car : career.garage) {
            if (carClass(car) != e.carClass) continue;
            const auto kind = static_cast<size_t>(e.prototypeKind);
            const auto index = kind < 10 ? static_cast<int8_t>(car.mods[kind]) : int8_t{-1};
            if (index == -1) {
                event = &e;
                return 5;
            }
            // 0x00447610: the part's level (the list's first past its end; stock problem: an index below -1 reads
            // before the list, the first here).
            const std::vector<CarPart>& parts = catalogue.lists[kind].parts;
            if (parts.empty()) continue;
            const CarPart& part = index >= 0 && index < static_cast<int>(parts.size())
                                      ? parts[static_cast<size_t>(index)]
                                      : parts.front();
            if (part.level != 3) {
                event = &e;
                return 5;
            }
        }
    }
    return 0;
}

// 3 (0x004A5E60): an event with a round of type 4 and more than one crew member; crew 4's steps.
int32_t crewFour(const Career& career, const CalendarEvent*& event, const PartCatalogue& catalogue) {
    event = findEvent(career, 4, EventKind::Circuit, false, false, catalogue);
    if (event == nullptr || career.profile.memberCount <= 1) return 0;
    return steps(career.crews[4], 3, 0, 2);
}

// 4 (0x004A5ED0) and 5 (0x004A5FA0): the best car's worth (the profile's +0x4FC) or the garage's (+0x4F4) at least
// 5000 over the told one (+0x4F8, +0x4F0) and an open event; crew 5's or 6's steps.
int32_t worth(const Career& career, const CalendarEvent*& event, const PartCatalogue& catalogue, uint32_t told,
              uint32_t now, size_t crew) {
    if (static_cast<int32_t>(told) + 5000 > static_cast<int32_t>(now)) return 0;
    event = firstOpen(career, catalogue);
    return event != nullptr ? steps(career.crews[crew], 3, 0, 2) : 0;
}

// 6 (0x004A6070): an event with a round of type 4, 5 or 3 for any reason and at least 15000; crew 7's steps.
int32_t crewSeven(const Career& career, const CalendarEvent*& event, const PartCatalogue& catalogue) {
    event = findEvent(career, 7, EventKind::Circuit, true, false, catalogue);
    if (event == nullptr) event = findEvent(career, 7, EventKind::Sprint, true, false, catalogue);
    if (event == nullptr) event = findEvent(career, 7, EventKind::PointToPoint, true, false, catalogue);
    if (event == nullptr || career.profile.money < 15000) return 0;
    return steps(career.crews[7], 3, 1, 2);
}

// 7 (0x004A6100): three cars or more and a crew of 1 .. 8 at its third step with its +0x2D0 under 3 (kept in the
// advisor's +0x70); crew 8's steps.
int32_t crewEight(Career& career) {
    if (career.garage.size() < 3) return 0;
    for (int32_t k = 1; k < 9; ++k) {
        const Crew& crew = career.crews[static_cast<size_t>(k)];
        if (crew.stage[2] != 0 && crew.pinkSlipsWon < 3) {
            career.advisor.targetCrew = k;
            return steps(career.crews[8], 3, 1, 2);
        }
    }
    return 0;
}

}  // namespace

const CalendarEvent* findEvent(const Career& career, int crew, std::optional<EventKind> roundType, bool any,
                               bool byCrew, const PartCatalogue& catalogue) {
    const Calendar& calendar = career.calendar;
    for (size_t k = 0; k < calendar.events.size(); ++k) {
        const CalendarEvent& e = calendar.events[k];
        if (e.used == 0 || e.done != 0) continue;
        if (byCrew && e.crew != crew) continue;
        if (roundType) {
            bool found = false;
            for (int r = 0; r < e.roundCount; ++r) {
                if (eventRoundType(calendar, k, r) == *roundType) found = true;
            }
            if (!found) continue;
        }
        int reason = 0;
        if (!mayEnter(career, e, reason, catalogue)) continue;
        if (any) return &e;
        if (reason != 1 && reason != 0 && reason != 7) return &e;
    }
    return nullptr;
}

bool challengeCar(const Career& career, const Crew& crew, int challenge, int32_t* rating) {
    if (crew.number < 0 || crew.number >= 9 || challenge < 0 || challenge >= 3) return false;
    const CrewChallenge& c = career.crews[static_cast<size_t>(crew.number)].challenges[static_cast<size_t>(challenge)];
    if (c.done != 0) return false;
    bool contact = false;
    const Phone& phone = career.phone;
    for (int32_t i = 0; i < phone.contactCount && i < static_cast<int32_t>(phone.contacts.size()); ++i) {
        if (phone.contacts[static_cast<size_t>(i)].crew == crew.number) {
            contact = true;
            break;
        }
    }
    if (!contact) return false;
    for (const Car& car : career.garage) {
        if (car.def == nullptr || car.def->id != c.car) continue;
        if (rating != nullptr) *rating = x87::chop(std::bit_cast<float>(car.rating));
        return true;
    }
    return false;
}

int32_t challengeAdvice(Career& career) {
    for (int32_t k = 1; k < 9; ++k) {
        for (int32_t ch = 0; ch < 3; ++ch) {
            int32_t rating = 0;
            if (!challengeCar(career, career.crews[static_cast<size_t>(k)], ch, &rating)) continue;
            const int16_t told = career.advisor.toldRatings[static_cast<size_t>(3 * k + ch)];
            if (told == 0 || rating > told) {
                career.advisor.challenge = ch;
                career.advisor.crew = k;
                career.advisor.rating = rating;
                return told == 0 ? 5 : 4;
            }
        }
    }
    return 0;
}

bool adviceDue(Career& career) {
    const bool due = challengeAdvice(career) > 0;
    career.advisor.challengeDue = due ? 1 : 0;
    return due;
}

void chooseAdvice(Career& career, CrtRandom& random, const PartCatalogue& catalogue) {
    career.advisor.openPhone = 0;
    int32_t best = 0;
    int32_t advice = 13;
    const CalendarEvent* chosen = nullptr;
    const auto draw = [&](int32_t score, int32_t id) {
        const bool taken = static_cast<uint32_t>(random.next()) % 10u == 5u;
        return taken && score != 0 && career.advisor.lastAdvice != id && score > best;
    };
    const auto take = [&](int32_t score, int32_t id, const CalendarEvent* event) {
        best = score;
        advice = id;
        chosen = event;
    };
    for (int round = 0; round < 100; ++round) {
        const CalendarEvent* e = nullptr;
        if (draw(lowCash(career, e, catalogue), 8)) {
            const int32_t s = lowCash(career, e, catalogue);
            take(s, 8, e);
        }
        if (draw(hosting(career), 12)) take(hosting(career), 12, nullptr);
        if (draw(challengeAdvice(career), 9)) take(challengeAdvice(career), 9, nullptr);
        if (draw(crewOne(career, e, catalogue), 0)) {
            const int32_t s = crewOne(career, e, catalogue);
            take(s, 0, e);
        }
        if (draw(crewTwo(career, e, catalogue), 1)) {
            const int32_t s = crewTwo(career, e, catalogue);
            take(s, 1, e);
        }
        if (draw(crewThree(career, e, catalogue), 2)) {
            const int32_t s = crewThree(career, e, catalogue);
            take(s, 2, e);
        }
        if (draw(prototype(career, e, catalogue), 10)) {
            const int32_t s = prototype(career, e, catalogue);
            take(s, 10, e);
        }
        if (draw(crewFour(career, e, catalogue), 3)) {
            const int32_t s = crewFour(career, e, catalogue);
            take(s, 3, e);
        }
        const CareerProfile& p = career.profile;
        if (draw(worth(career, e, catalogue, p.bestCarWorthTold, p.bestCarWorth, 5), 4)) {
            const int32_t s = worth(career, e, catalogue, p.bestCarWorthTold, p.bestCarWorth, 5);
            take(s, 4, e);
        }
        if (draw(worth(career, e, catalogue, career.profile.garageWorth, career.profile.garageWorthBest, 6), 5)) {
            const int32_t s = worth(career, e, catalogue, career.profile.garageWorth, career.profile.garageWorthBest, 6);
            take(s, 5, e);
        }
        if (draw(crewSeven(career, e, catalogue), 6)) {
            const int32_t s = crewSeven(career, e, catalogue);
            take(s, 6, e);
        }
        if (draw(crewEight(career), 7)) take(crewEight(career), 7, nullptr);
        const int32_t empty = career.garage.empty() ? 6 : 0;
        if (draw(empty, 11)) take(empty, 11, nullptr);
    }
    if (career.advisor.challengeDue != 0) {
        career.advisor.tell(3 * career.advisor.crew + career.advisor.challenge,
                            static_cast<int16_t>(career.advisor.rating));
        advice = 9;
        career.advisor.challengeDue = 0;
        chosen = nullptr;
    }
    career.advisor.advice = advice;
    career.adviceEvent = chosen;
    career.advisor.chosen = 1;
}

}  // namespace oj
