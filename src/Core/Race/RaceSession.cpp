#if defined(OJ_MACOS) || defined(OJ_LINUX)
#include "Mods/Creative.h"
#endif
#include "Core/Race/RaceSession.h"

#include "Core/Career/CareerEvents.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/OnlineRecords.h"
#include "Core/Race/Racer.h"
#include "Core/Base/Text.h"
#include "Core/Base/X87.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace oj {

namespace {

// The fractions as the executable keeps them (0x00718D40): against, stake.
constexpr std::array<std::array<int32_t, 2>, 33> kFractions = {{
    {100, 1}, {33, 1}, {25, 1}, {15, 1}, {14, 1}, {12, 1}, {10, 1}, {9, 1}, {8, 1}, {7, 1}, {6, 1},
    {5, 1},   {4, 1},  {3, 1},  {2, 1},  {1, 1},  {5, 2},  {6, 4},  {6, 5}, {7, 2}, {7, 4}, {8, 5},
    {9, 2},   {9, 4},  {11, 2}, {11, 4}, {13, 2}, {13, 8}, {13, 10}, {15, 8}, {4, 5}, {8, 13}, {8, 11},
}};

// The weights of the seven bet amounts (0x007189F8).
constexpr std::array<float, 7> kBetWeights = {0.3f, 0.6f, 0.9f, 1.0f, 0.9f, 0.6f, 0.3f};

// What a crew leader's odds start from by the laps of the round (more laps, longer odds), less the grid place's
// share (0x00718D24).
constexpr std::array<float, 7> kPlaceShare = {10.0f, 9.0f, 4.0f, 3.0f, 0.1f, 0.05f, 0.0f};

// 0x0047FCA0: the lowest rating of a class (0 for class 8, any).
int classBottom(int cls) {
    switch (cls) {
        case 1: return 700;
        case 2: return 600;
        case 3: return 500;
        case 4: return 400;
        case 5: return 300;
        case 6: return 200;
        case 7: return 0;
        case 8: return 0;
        default: return 800;
    }
}

}  // namespace

void applyCrewScheme(Career& career, const Crew& crew, const CarDefs& defs, const PartCatalogue& catalogue) {
    std::vector<Car>& cars = crew.id == 0 ? career.garage : career.opponentCars;
    for (Car& car : cars) {
        // Stock problem: the amounts 0.5 and 0.1 are written over by the crew's, which nothing ever sets: the metallic
        // amount the bits of the new career's fourth white (0x80FFFFFF, about -0), the pearl's 0. The fixed build
        // keeps 0.5 and 0.1 (the Xbox builds of 2004 do the same as the PC).
        car.metallicAmount = Car::bitsOf(0.5f);
        car.pearlAmount = Car::bitsOf(0.1f);
        car.base = crew.baseColour;
        car.metallic = crew.metallicColour;
        car.pearl = crew.pearlColour;
        car.mods[kWheel] = static_cast<uint8_t>(crew.wheels);
        car.body[9] = 1;
        if (!defs.neons.empty()) {
            const auto n = static_cast<size_t>(crew.neonColour);
            car.neon = crew.neonColour >= 0 && n < defs.neons.size() ? defs.neons[n] : defs.neons[0];
        }
        car.wheelsColoured = 1;
        if (crew.wheelColour >= 0 && static_cast<size_t>(crew.wheelColour) < defs.wheels.size()) {
            car.wheelColour = defs.wheels[static_cast<size_t>(crew.wheelColour)];
        }
        if (heavilyTuned(car, catalogue)) {
            car.body[8] = 0;
            car.decalColour = 0;
            continue;
        }
        int less = 2;
        switch (crew.id) {
            case 2: less = 8; break;
            case 3: less = 7; break;
            case 4: less = 1; break;
            case 5: less = 4; break;
            case 6: less = 3; break;
            case 7: less = 5; break;
            case 8: less = 6; break;
            default: break;
        }
        size_t decals = car.def != nullptr ? car.def->parts[kDecal].parts.size() : 0;
#if defined(OJ_MACOS) || defined(OJ_LINUX)
        if (car.def != nullptr) decals = mods::decalCount(*car.def, decals);
#endif
        car.decalColour = 0;
        // Add-on cars need not supply the retail eight crew liveries.
        car.body[8] = decals >= 8 ? static_cast<uint8_t>(decals - static_cast<size_t>(less)) : 0x7F;
    }
}

bool heavilyTuned(const Car& car, const PartCatalogue& catalogue) {
    int prototypes = 0;
    for (size_t k = 0; k < kModKinds; ++k) {
        const auto index = static_cast<int8_t>(car.mods[k]);
        const std::vector<CarPart>& parts = catalogue.lists[k].parts;
        if (index == -1 || parts.empty()) continue;
        const CarPart& part =
            index >= 0 && index < static_cast<int>(parts.size()) ? parts[static_cast<size_t>(index)] : parts.front();
        if (part.level == 3) ++prototypes;
    }
    return prototypes > 3;
}

namespace {

// 0x004A9540: a car of the opponents' list for a crew in the current event; its index, also kept in the crew's
// first free race car slot.
uint32_t pickOpponentCar(Career& career, CareerContext& context, Crew& crew) {
    CrtRandom& random = context.making.random;
    const CalendarEvent* event = currentEvent(career.calendar);
    const CareerProfile& profile = career.profile;
    float threshold = 0.0f;
    int drive = 3;  // any
    if (profile.raceCarsChosen == 0) {
        threshold = static_cast<float>(classBottom(event != nullptr ? event->carClass : 0) + 50);
    } else {
        // The best of the player's cars of the race less 50, and its drive.
        for (int32_t k = 0; k < profile.raceCarCount && k < 3; ++k) {
            const auto index = static_cast<size_t>(profile.raceCars[static_cast<size_t>(k)]);
            if (index >= career.garage.size()) continue;
            const Car& car = career.garage[index];
            const float rating = Car::asFloat(car.rating);
            if (threshold < rating) {
                threshold = rating - 50.0f;
                drive = driveOf(car, context.tables);
            }
        }
    }
    const std::vector<Car>& cars = carsOf(career, crew.id);
    const auto count = static_cast<uint32_t>(cars.size());
    for (int pass = 2; pass >= 0; --pass) {
        for (uint32_t attempt = 0; attempt < 1000; ++attempt) {
            if (attempt > 200 && pass == 0) threshold = threshold - 0.1f;
            uint32_t index = attempt < 701 ? static_cast<uint32_t>(random.next()) : attempt;
            // Stock problem: an empty list divides by zero; 0 here.
            index = count != 0 ? index % count : 0;
            if (index >= count || event == nullptr) continue;
            const Car& car = cars[index];
            const CarDef* def = car.def;
            if (def == nullptr) continue;
            if (pass == 2 && ((crew.preferredRegion != 0 && crew.preferredRegion != def->region) ||
                              (crew.preferredMake != 0 && crew.preferredMake != def->make))) {
                continue;
            }
            if ((event->region != def->region && event->region != 0) ||
                (event->make != def->make && event->make != 0) ||
                (event->model != def->id && event->model != 0)) {
                continue;
            }
            const bool extreme = event->extreme != 0;
            if (extreme && !heavilyTuned(car, context.making.catalogue)) continue;
            if (pass != 0 && !extreme && heavilyTuned(car, context.making.catalogue)) continue;
            const int cls = carClass(car);
            if (cls != event->carClass && event->carClass != 8) continue;
            if (!(threshold <= Car::asFloat(car.rating))) continue;
            if (event->type == EventKind::Sprint &&
                (def->id == 40 || (driveOf(car, context.tables) != drive && drive != 3))) {
                continue;
            }
            for (int32_t& slot : crew.raceCars) {
                if (slot == -1) {
                    slot = static_cast<int32_t>(index);
                    break;
                }
            }
            return index;
        }
    }
    return 0;
}

// 0x004A98C0: a member of the crew (rand() & 3) that no racer of the session has yet.
int32_t pickMember(const Career& career, const Crew& crew, CrtRandom& random) {
    const RaceSession& session = career.session;
    for (;;) {
        const auto member = static_cast<int32_t>(random.next() & 3);
        bool taken = false;
        for (uint32_t i = 0; i < session.count && i < session.racers.size(); ++i) {
            const Racer& r = session.racers[i];
            if (r.crew == crew.id && r.member == member) taken = true;
        }
        if (!taken) return member;
    }
}

// 0x004BDCC0: the skills and nerves of the racers' drivers, their first level (high for a rival crew, medium for
// the player's) and a first time of 1 .. 5 s until it is chosen again.
void setupDrivers(Career& career, CrtRandom& random) {
    RaceDrivers& d = career.drivers;
    RaceSession& session = career.session;
    d.slotCar0 = 0;
    d.slotCar1 = 0;
    d.slotCar2 = 0;
    d.slotCount = 0;
    d.session = &session;
    d.namesTime = 0.0f;
    for (uint32_t i = 0; i < session.count && i < session.racers.size(); ++i) {
        Racer& r = session.racers[i];
        if (r.crew >= 0 && r.crew < static_cast<int32_t>(career.crews.size())) {
            const Crew& crew = career.crews[static_cast<size_t>(r.crew)];
            if (r.crew == 0 && career.profile.memberCount != 0) {
                // A member of the player's crew: its crewmate (none past the members).
                const Crewmate* mate = nullptr;
                if (r.member >= 0 && r.member < career.profile.memberCount && r.member < 4) {
                    const auto number = static_cast<size_t>(career.members[static_cast<size_t>(r.member)]);
                    if (number < career.crewmates.size()) mate = &career.crewmates[number];
                }
                if (mate != nullptr) {
                    r.skill = mate->drivingAbility * 0.01f;
                    r.composure = mate->recover * 0.01f;
                    r.nerveGain = mate->recover == 0.0f ? 0.0f : 1.0f / mate->recover;
                    r.nerveGain = r.nerveGain * 1.5f;
                    r.nerveLoss = mate->recover * 0.001f;
                }
            } else {
                // The member is not checked (0x004BDDA8), but it is always 0 .. 3: rand() & 3 (0x004A98C0), 0 for
                // a leader, 1 for an arcade opponent of crew 0, a slot of the members menu; the first member past
                // them here.
                const size_t m = r.member >= 0 && r.member < 4 ? static_cast<size_t>(r.member) : 0;
                const CrewMemberDef& member = crew.members[m];
                r.skill = member.ability * 0.01f;
                r.composure = member.concentration * 0.01f;
                r.nerveGain = 1.4f / member.concentration;
                r.nerveLoss = member.concentration * 0.0005f;
            }
        }
        r.panicTime = 0.0f;
        r.nerves = 0.0f;
        r.order = 0;
        r.orderTime = 0.0f;
        r.level = (r.crew != 0) + 1;
        r.levelTimer = static_cast<float>(static_cast<uint32_t>(random.next()) % 5 + 1);
    }
    d.pad = 0;
    d.withMembers = 0;
    d.ordersShown = 0;
    for (uint32_t i = 0; i < session.count && i < session.racers.size(); ++i) {
        if (session.racers[i].crew == 0 && session.racers[i].member != 0) {
            d.withMembers = 1;
            d.ordersShown = 1;
            d.slotsTaken = 0;
            break;
        }
    }
}

}  // namespace

namespace {

// 0x004B9150 and 0x004B93C0 after their racers: the odds cleared (stake 1), the crews' counts 0, the order -1, the
// first finisher none; the drivers (0x004BDCC0); the bet none.
void finishSession(Career& career, CrtRandom& random) {
    RaceSession& s = career.session;
    s.placed = 0;
    for (CrewOdds& o : s.odds) {
        o.stake = 1.0f;
        o.raceTime = 0.0f;
        o.against = 0.0f;
        o.staked = 0;
        o.winnings = 0.0f;
    }
    s.finished.fill(0);
    s.order.fill(-1);
    s.winner = -1;
    s.winnerCrew = -1;
    setupDrivers(career, random);
    s.bet = 0;
    s.betRacer = 0;
}

// A racer of these sessions: placed and numbered i, no car, no name.
void sessionRacer(Racer& r, size_t i, bool player, int32_t crew, int32_t member) {
    r.raceCar = 0;
    r.driver = player ? 1 : 0;
    r.crew = crew;
    r.place = static_cast<int32_t>(i);
    r.number = static_cast<int32_t>(i);
    r.cars = nullptr;
    r.finished = 0;
    r.rammed = 0;
    r.crashed = 0;
    r.member = member;
    r.car = 0;
    r.name.clear();
}

}  // namespace

void setupTestTrackSession(Career& career, CrtRandom& random) {
    RaceSession& s = career.session;
    s.count = 1;
    s.round = nullptr;
    s.event = nullptr;
    for (Racer& r : s.racers) {
        r.car = -1;
        r.member = -1;
    }
    career.crews[0].raceCars.fill(-1);
    Racer& r = s.racers[0];
    r.raceCar = 0;
    r.crew = 0;
    r.number = 0;
    r.cars = &career.garage;
    r.finished = 0;
    r.driver = 1;
    r.rammed = 0;
    r.crashed = 0;
    r.member = 0;
    r.car = career.dealer.chosen;
    const auto& shortName = career.crews[0].memberShortNames[0];
    r.name = widenSigned(std::string_view(shortName.data(), shortName.size()));
    s.placed = 0;
    for (CrewOdds& o : s.odds) {
        o.raceTime = 0.0f;
        o.against = 0.0f;
        o.stake = 1.0f;
        o.staked = 0;
        o.winnings = 0.0f;
    }
    s.finished.fill(0);
    s.order.fill(0);
    s.winner = -1;
    s.winnerCrew = -1;
    CareerProfile& p = career.profile;
    const int32_t one = 1;
    p.raceChoiceCount = one;
    p.raceChoices[0] = r.car;
    p.raceMembers[0] = 0;
    p.raceCars[0] = r.car;
    p.raceCarsChosen = 1;
    setupDrivers(career, random);
    s.bet = 0;
    s.betRacer = 0;
}

void setupCustomSession(Career& career, int32_t players, int32_t opponents, CrtRandom& random) {
    RaceSession& s = career.session;
    s.count = static_cast<uint32_t>(opponents + players);
    for (Racer& r : s.racers) {
        r.member = -1;
        r.car = -1;
    }
    for (uint32_t i = 0; i < s.count && i < s.racers.size(); ++i) {
        const bool player = i < static_cast<uint32_t>(players);
        sessionRacer(s.racers[i], i, player, player ? 0 : static_cast<int32_t>(i), 0);
    }
    finishSession(career, random);
}

void setupNetSession(Career& career, const std::vector<NetEntry>& entries, CrtRandom& random) {
    RaceSession& s = career.session;
    for (Racer& r : s.racers) {
        r.member = -1;
        r.car = -1;
    }
    s.count = 0;
    for (const NetEntry& e : entries) {
        if (s.count >= s.racers.size()) break;
        Racer& r = s.racers[s.count];
        sessionRacer(r, s.count, e.ours && e.localIndex == 0, e.ours ? 0 : e.localIndex + 1, 0);
        r.number = e.gridPlace;
        r.name = e.name;
        ++s.count;
    }
    finishSession(career, random);
}

void setupArcadeSession(Career& career, const std::vector<ArcadeEntry>& entries, CrtRandom& random) {
    RaceSession& s = career.session;
    s.count = static_cast<uint32_t>(entries.size());
    for (Racer& r : s.racers) {
        r.member = -1;
        r.car = -1;
    }
    for (size_t i = 0; i < entries.size() && i < s.racers.size(); ++i) {
        const ArcadeEntry& e = entries[i];
        sessionRacer(s.racers[i], i, e.player, e.player ? 0 : e.number, !e.player && e.number == 0 ? 1 : 0);
    }
    finishSession(career, random);
}

namespace {

// The key the racers are sorted by: the car's value and 1000 for each crew number.
float sortKey(const Racer& r, const PartCatalogue& catalogue) {
    int value = 0;
    if (r.cars != nullptr && r.car >= 0 && static_cast<size_t>(r.car) < r.cars->size()) {
        value = carValue((*r.cars)[static_cast<size_t>(r.car)], catalogue);
    }
    return static_cast<float>(value) + static_cast<float>(r.crew * 1000);
}

}  // namespace

namespace {

int32_t g_crewPlace = 0;       // [0x007255B4]
int32_t g_leadRespect = 0;     // [0x0079E840]
int32_t g_leadRespect5 = 0;    // [0x0079E83C]

int32_t chopRespect(float v) {
    return static_cast<int32_t>(v);
}

const Competitor* competitorOf(const std::vector<const RaceCar*>& cars, size_t i) {
    return i < cars.size() && cars[i] != nullptr ? cars[i]->competitor : nullptr;
}

// The crew's respect (+0x70) lowers what it gives: x 0.6 over 700, x 0.8 over 400.
float byCrewRespect(float v, const Crew& crew) {
    if (crew.respect > 700) return v * 0.6f;
    if (crew.respect > 400) return v * 0.8f;
    return v;
}

// 0x004A5110: the lead of the player's crew while it is first (the race distances, cm x 0.01: the rivals' farthest
// less the crew's nearest), or its gap while it is last (the rivals' nearest less the crew's nearest, at most 1000);
// else 0.
int32_t crewLead(Career& career, const std::vector<const RaceCar*>& cars) {
    const RaceSession& s = career.session;
    // The crews of the session's event (+0x04).
    const int32_t opponents = s.event != nullptr ? roundOf(career, *s.event)->opponents : 0;
    int32_t place = crewPlace(career, cars, 0);
    if (place == -1) place = opponents - 1;
    const auto distanceOf = [&](size_t i) {
        const Competitor* c = competitorOf(cars, i);
        return c != nullptr ? c->raceDistance : 0.0f;
    };
    if (place == 0) {
        float own = 10000000.0f;
        for (uint32_t i = 0; i < s.count && i < s.racers.size(); ++i) {
            if (s.racers[i].crew == 0 && distanceOf(i) < own) own = distanceOf(i);
        }
        float rival = 0.0f;
        for (uint32_t i = 0; i < s.count && i < s.racers.size(); ++i) {
            if (s.racers[i].crew != 0 && rival < distanceOf(i)) rival = distanceOf(i);
        }
        return chopRespect((rival - own) * 0.01f);
    }
    if (place != opponents - 1) return 0;
    float own = 10000000.0f;
    for (uint32_t i = 0; i < s.count && i < s.racers.size(); ++i) {
        if (s.racers[i].crew == 0 && distanceOf(i) < own) own = distanceOf(i);
    }
    float rival = 10000000.0f;
    for (uint32_t i = 0; i < s.count && i < s.racers.size(); ++i) {
        if (s.racers[i].crew != 0 && rival > distanceOf(i)) rival = distanceOf(i);
    }
    float v = (rival - own) * 0.01f;
    if (v > 1000.0f) v = 1000.0f;
    return chopRespect(v);
}

// 0x004A5670: the place of the player's crew by the points (+0x138, with 6, 4 and 2 for the places 1 .. 3): the
// racers sorted by them, the most first (the sort starts again after each swap), and the first of crew 0's place.
int32_t pointsPlace(Career& career, const std::vector<const RaceCar*>& cars) {
    const RaceSession& s = career.session;
    std::vector<size_t> order;
    for (uint32_t i = 0; i < s.count && i < s.racers.size(); ++i) order.push_back(i);
    const auto score = [&](size_t i) {
        const Competitor* c = competitorOf(cars, i);
        if (c == nullptr) return 0.0f;
        float v = c->points;
        if (c->place == 1) v = v + 6.0f;
        if (c->place == 2) v = v + 4.0f;
        if (c->place == 3) v = v + 2.0f;
        return v;
    };
    for (size_t k = 0; k + 1 < order.size(); ++k) {
        if (score(order[k]) < score(order[k + 1])) {
            std::swap(order[k], order[k + 1]);
            k = static_cast<size_t>(-1);
        }
    }
    for (size_t k = 0; k < order.size(); ++k) {
        const RaceCar* car = order[k] < cars.size() ? cars[order[k]] : nullptr;
        if (car != nullptr && car->crew == 0) return static_cast<int32_t>(k);
    }
    return 0;
}

// The tables of the places' respect for 2 .. 6 crews (n the crews less one): 80 for the first, then by n, -20 for
// the last.
float placeTable(int32_t n, int32_t place, float second, float third4, float third5, float fourth5) {
    if (place == 0 && n >= 1 && n <= 5) return 80.0f;
    switch (n) {
    case 1: return place == 1 ? -20.0f : 0.0f;
    case 2: return place == 2 ? -20.0f : 0.0f;
    case 3: return place == 1 ? second : place == 3 ? -20.0f : 0.0f;
    case 4: return place == 1 ? second : place == 3 ? third4 : place == 4 ? -20.0f : 0.0f;
    case 5: return place == 1 ? second : place == 3 ? third5 : place == 4 ? fourth5 : place == 5 ? -20.0f : 0.0f;
    default: return 0.0f;
    }
}

// 0x004BB520: the places (80, 50 / 60, -8, -15, -20) and the lead ([0x0079E840]: the lead x -1/100 cut, x 20, worked
// out again only while measuring with nobody finished: leadCars given); halved when positive in a current event of
// type 3; lowered by the crew's respect; a crew other than crew 1 with respect from 350 gives none (and takes
// nothing), below it a loss x 0.8. Applying adds it x the crew's factor 4 to its gain 4. Returns that cut.
int32_t respectByPlace(Career& career, Crew& crew, int32_t place, int32_t n, bool apply,
                       const std::vector<const RaceCar*>* leadCars) {
    const CalendarEvent* event = currentEvent(career.calendar);
    float v = placeTable(n, place, n == 5 ? 60.0f : 50.0f, -8.0f, -8.0f, -15.0f);
    if (!apply && leadCars != nullptr) g_leadRespect = (-crewLead(career, *leadCars) / 100) * 20;
    v = static_cast<float>(g_leadRespect) + v;
    if (event != nullptr && event->type == EventKind::PointToPoint && v > 0.0f) v = v * 0.5f;
    v = byCrewRespect(v, crew);
    if (crew.number != 1) {
        if (crew.respect >= 350) return 0;
        if (v < 0.0f) v = v * 0.8f;
    }
    if (apply) crew.respectGain[4] = v * crew.respectFactor[4] + crew.respectGain[4];
    return chopRespect(v * crew.respectFactor[4]);
}

// 0x004BB740: the places of 2 .. 6 crews (80, 50, -10 / -5 and -10, -20) and the lead ([0x0079E83C]: x -1/20 cut,
// x 20, as 0x004BB520's), lowered by the crew's respect; applying adds it x the factor 5 to the gain 5.
int32_t respectByPoints(Career& career, Crew& crew, int32_t place, int32_t n, bool apply,
                        const std::vector<const RaceCar*>* leadCars) {
    float v = placeTable(n, place, 50.0f, -10.0f, -5.0f, -10.0f);
    if (!apply && leadCars != nullptr) g_leadRespect5 = (-crewLead(career, *leadCars) / 20) * 20;
    v = static_cast<float>(g_leadRespect5) + v;
    v = byCrewRespect(v, crew);
    if (apply) crew.respectGain[5] = v * crew.respectFactor[5] + crew.respectGain[5];
    return chopRespect(v * crew.respectFactor[5]);
}

// 0x004BB9A0: the places (80, 50, -10 / -8 and -12, -20) lowered by the crew's respect; applying adds it x the factor
// 2 to the gain 2.
int32_t respectByStanding(Crew& crew, int32_t place, int32_t n, bool apply) {
    const float v = byCrewRespect(placeTable(n, place, 50.0f, -10.0f, -8.0f, -12.0f), crew);
    if (apply) crew.respectGain[2] = v * crew.respectFactor[2] + crew.respectGain[2];
    return chopRespect(v * crew.respectFactor[2]);
}

// 0x004BBB00: below the crew's mark (+0x6C) the shortfall x the factor 7 x -0.001; else the excess x the factor 7 x
// 0.001 held to 30 x the current event's crew + 20, doubled and lowered by the crew's respect. Applying adds it to the
// gain 7, and above the mark doubles the mark (at most 250000).
int32_t respectByMoney(Career& career, Crew& crew, int32_t money, bool apply) {
    const CalendarEvent* event = currentEvent(career.calendar);
    const auto m = static_cast<float>(money);
    if (m < crew.moneyMark) {
        const float v = (crew.moneyMark - m) * crew.respectFactor[7] * -0.001f;
        if (apply) crew.respectGain[7] = v + crew.respectGain[7];
        return chopRespect(v);
    }
    float v = (m - crew.moneyMark) * crew.respectFactor[7] * 0.001f;
    const auto cap = static_cast<float>((event != nullptr ? event->crew : 0) * 30 + 20);
    if (v > cap) v = cap;
    v = byCrewRespect(v + v, crew);
    if (apply) {
        crew.respectGain[7] = v + crew.respectGain[7];
        crew.moneyMark = crew.moneyMark + crew.moneyMark;
        if (crew.moneyMark > 250000.0f) crew.moneyMark = 250000.0f;
    }
    return chopRespect(v);
}

// 0x004BB8E0: the bet by the profile's bet share: over 7.5 50 won, 10 lost; over 3 25 and 5; else 10 and -20;
// lowered by the crew's respect; applying adds it x the factor 6 to the gain 6.
int32_t respectByBet(const Career& career, Crew& crew, bool won, bool apply) {
    const float share = career.profile.betShare;
    float v = 0.0f;
    if (share > 7.5f) {
        v = won ? 50.0f : 10.0f;
    } else if (share > 3.0f) {
        v = won ? 25.0f : 5.0f;
    } else {
        v = won ? 10.0f : -20.0f;
    }
    v = byCrewRespect(v, crew);
    if (apply) crew.respectGain[6] = v * crew.respectFactor[6] + crew.respectGain[6];
    return chopRespect(v * crew.respectFactor[6]);
}

}  // namespace

int32_t respectByCollection(Career& career, Crew& crew, const PartCatalogue& catalogue, bool apply) {
    CareerProfile& p = career.profile;
    if (p.introduction != 0) return 0;
    float sum = 0.0f;
    for (const Car& car : career.garage) sum = static_cast<float>(cappedCarValue(car, catalogue)) + sum;
    const float v = (sum - std::bit_cast<float>(crew.collectionMark)) * crew.respectFactor[0] * 0.001f;
    if (apply) {
        crew.respectGain[0] = v + crew.respectGain[0];
        p.garageWorthBest = static_cast<uint32_t>(x87::chop(sum));
        crew.collectionMark = std::bit_cast<int32_t>(sum);
    }
    return x87::chop(v);
}

int32_t respectByBestCar(Career& career, Crew& crew, const PartCatalogue& catalogue, bool apply) {
    CareerProfile& p = career.profile;
    if (p.introduction != 0) return 0;
    float best = 0.0f;
    float bestValue = 0.0f;
    for (const Car& car : career.garage) {
        const int weight = 8 - carClass(car);
        if (static_cast<float>(weight * cappedCarValue(car, catalogue)) > best) {
            bestValue = static_cast<float>(cappedCarValue(car, catalogue));
            best = static_cast<float>(8 - carClass(car)) * bestValue;
        }
    }
    float v = best - std::bit_cast<float>(crew.bestCarMark);
    if (apply) {
        p.bestCarWorth = static_cast<uint32_t>(x87::chop(bestValue));
        crew.bestCarMark = std::bit_cast<int32_t>(best);
    }
    const bool rose = v > 0.0f;
    v = v * crew.respectFactor[1];
    v = rose ? v * 0.0022222222f : v * 0.00044444444f;
    if (apply) crew.respectGain[1] = v + crew.respectGain[1];
    return x87::chop(v);
}

namespace {


// 0x004B5DE0: with the profile's +0x630 what crewmates 1 .. 3 learnt is cleared (the stats' +0x30 .. +0x77); else
// each of them with a value learnt not 0 (the first in the order) is noted in the career's +0x38 list: 0x1F for a
// gain, 0x20 for a loss, with the crewmate's number.
void noteLearning(Career& career) {
    CareerStats& stats = career.stats;
    if (career.profile.outOfRace != 0) {
        for (size_t m = 1; m < 4; ++m) stats.learnt[m].fill(0.0f);
        return;
    }
    for (size_t m = 1; m < 4; ++m) {
        for (const float v : stats.learnt[m]) {
            if (v > 0.0f) {
                career.meeting.add(0x1F, static_cast<float>(m), 0.0f, 0.0f);
                break;
            }
            if (v < 0.0f) {
                career.meeting.add(0x20, static_cast<float>(m), 0.0f, 0.0f);
                break;
            }
        }
    }
}

}  // namespace

float raceMoney(const Career& career) {
    return career.cruise.total;
}

namespace {

// The profile's counters of 16 bits from +0x18 to +0x4F as the game reaches them by their offsets.
void countProfile(CareerProfile& p, int32_t offset) {
    static_assert(offsetof(CareerProfile, winsByType) == 0x18 && offsetof(CareerProfile, respectLevel) == 0x4E);
    if (offset < 0x18 || offset > 0x4E) return;
    uint8_t* at = reinterpret_cast<uint8_t*>(&p) + offset;
    int16_t v = 0;
    std::memcpy(&v, at, 2);
    v = static_cast<int16_t>(v + 1);
    std::memcpy(at, &v, 2);
}

// 0x004AA3B0: a crewmate learns from a race: its driving ability (+0x94) and its recovery (+0x9C) grow by its learn
// rate (+0xA4), each held to its limit (+0x98, +0xA0); a growth under the limit is added to the stats' (+0x18 / +0x20
// + 0x18 x the crewmate's number).
void learnFromRace(Career& career, Crewmate& mate) {
    const auto number = static_cast<size_t>(mate.number);
    std::array<float, 6>* learnt = number < career.stats.learnt.size() ? &career.stats.learnt[number] : nullptr;
    mate.drivingAbility = mate.learnRate + mate.drivingAbility;
    if (mate.drivingAbility > mate.drivingLimit) {
        mate.drivingAbility = mate.drivingLimit;
    } else if (learnt != nullptr) {
        (*learnt)[0] = mate.learnRate + (*learnt)[0];
    }
    mate.recover = mate.learnRate + mate.recover;
    if (mate.recover > mate.recoverLimit) {
        mate.recover = mate.recoverLimit;
    } else if (learnt != nullptr) {
        (*learnt)[2] = mate.learnRate + (*learnt)[2];
    }
}

}  // namespace

bool challengeMet(const Career& career, const RaceSetup& race) {
    const CalendarEvent* event = career.session.event;
    if (event == nullptr || event->crew < 0 || event->crew >= 9 || event->challengeIndex < 0 ||
        event->challengeIndex >= 3) {
        return false;
    }
    const CrewChallenge& c =
        career.crews[static_cast<size_t>(event->crew)].challenges[static_cast<size_t>(event->challengeIndex)];
    if (c.type < ChallengeType::LapTime) return false;
    if (c.type <= ChallengeType::Speed) return race.challenge == ChallengeOutcome::Met;
    if (c.type == ChallengeType::Showoff) return x87::chop(raceMoney(career)) > c.value;
    return false;
}

namespace {

// 0x004990F0: the first part of a kind of the catalogue of a supplier (+0x2A) at the prototype level (3; the
// catalogue's +0x100 set to it); none when the kind has none.
const CarPart* prototypePart(const PartCatalogue& catalogue, int32_t kind, int32_t supplier) {
    if (kind < 0 || kind >= kModKinds) return nullptr;
    for (const CarPart& part : catalogue.lists[static_cast<size_t>(kind)].parts) {
        if (static_cast<int8_t>(part.brand) == supplier && part.level == 3) return &part;
    }
    return nullptr;
}

}  // namespace

int32_t crewPlace(Career& career, const std::vector<const RaceCar*>& racerCars, int32_t crew) {
    const RaceSession& s = career.session;
    const CalendarEvent* event = currentEvent(career.calendar);
    if (event == nullptr) return -1;
    const EventRound& round = *roundOf(career, *event);
    std::vector<int32_t> byPlace(s.count, -1);
    for (uint32_t i = 0; i < s.count && i < s.racers.size(); ++i) {
        const RaceCar* car = i < racerCars.size() ? racerCars[i] : nullptr;
        if (car == nullptr) return -1;
        const Competitor* c = car->competitor;
        if (c == nullptr) continue;
        if (c->place >= 1 && static_cast<uint32_t>(c->place) <= s.count) {
            byPlace[static_cast<size_t>(c->place - 1)] = s.racers[i].crew;
        }
        if (c->crossings == 0) return -1;
    }
    for (int32_t p : byPlace) {
        if (p == -1) return g_crewPlace;
    }
    std::array<int32_t, 9> reached{};
    std::vector<int32_t> crews;
    for (int32_t p : byPlace) {
        if (p < 0 || p >= 9) continue;
        if (++reached[static_cast<size_t>(p)] == round.teamSize) crews.push_back(p);
    }
    // The crews of the session's event (+0x04), the drivers of each by the calendar's.
    const int32_t opponents = s.event != nullptr ? roundOf(career, *s.event)->opponents : 0;
    for (int32_t k = 0; k < opponents && static_cast<size_t>(k) < crews.size(); ++k) {
        if (crews[static_cast<size_t>(k)] == crew) {
            g_crewPlace = k;
            return k;
        }
    }
    g_crewPlace = 0;
    return 0;
}

float raceRespect(Career& career, const std::vector<const RaceCar*>& racerCars, size_t finishers, bool lastPlace,
                  bool collection, const PartCatalogue& catalogue) {
    // The career's +0x34 object's +0x30 holds the last respect worked out.
    const auto keep = [&](float v) {
        career.news.respect = v;
        return v;
    };
    keep(0.0f);
    const CalendarEvent* event = currentEvent(career.calendar);
    if (event == nullptr) return 0.0f;
    if (career.profile.raceCarsChosen == 0 && career.profile.racing == 0) return 0.0f;
    const EventRound& round = *roundOf(career, *event);
    const int32_t n = round.opponents - 1;
    int32_t place = round.type == EventKind::Sprint ? pointsPlace(career, racerCars) : crewPlace(career, racerCars, 0);
    if (place == -1 || lastPlace) place = n;
    if (event->challenge != 0) return 0.0f;
    int32_t total = 0;
    const std::vector<const RaceCar*>* leadCars = finishers == 0 ? &racerCars : nullptr;
    for (size_t k = 1; k < 9; ++k) {
        Crew& crew = career.crews[k];
        if (event->pinkSlip != 0) {
            float v = 0.0f;
            if (place == 0) {
                v = static_cast<float>((event->crew + 1) * 20);
            } else if (place == 1) {
                v = -30.0f;
            }
            total += chopRespect(v * crew.respectFactor[8]);
        } else if (round.type == EventKind::Showoff) {
            total += respectByMoney(career, crew, chopRespect(raceMoney(career)), false);
        } else if (round.type == EventKind::Sprint) {
            total += respectByPoints(career, crew, place, n, false, leadCars);
        } else {
            total += respectByPlace(career, crew, place, n, false, leadCars);
        }
        if (career.profile.racing == 0) total += respectByStanding(crew, place, n, false);
        total += chopRespect(crew.rams * crew.respectFactor[3] * -0.3f);
    }
    if (career.bets.placed != 0 && career.bets.pinkSlip == 0) {
        // The bet's respect is crew 7's (the career's +0x24), not the crew bet with: in CMSCrews.txt crew 7 alone
        // weighs bets (its RespectFactor 6), as crew 5 alone the best car and crew 6 alone the collection.
        const int32_t betCrew = crewPlace(career, racerCars, career.bets.crew);
        const int32_t own = crewPlace(career, racerCars, 0);
        total += respectByBet(career, career.crews[7], own < betCrew, false);
    }
    if (lastPlace || collection) {
        total += respectByBestCar(career, career.crews[5], catalogue, false);
        total += respectByCollection(career, career.crews[6], catalogue, false);
    }
    if (std::abs(total) > 10) total = total / 5 * 5;
    return keep(static_cast<float>(total));
}

int cappedCarValue(const Car& car, const PartCatalogue& catalogue) {
    const auto capped = [](int v) {
        const auto f = static_cast<float>(v);
        return f > 15000.0f ? 15000.0f : f;
    };
    float total = static_cast<float>(car.def != nullptr ? car.def->value : 0);
    for (uint8_t b : car.body) {
        const auto level = static_cast<int8_t>(b);
        total = total + capped(level == -1 || level == 0 || level > 0x7E ? 0 : (level + 3) * 500);
    }
    for (size_t k = 0; k < kModKinds; ++k) {
        const auto index = static_cast<int8_t>(car.mods[k]);
        int v = 0;
        const std::vector<CarPart>& parts = catalogue.lists[k].parts;
        if (index != -1 && !parts.empty()) {
            // Stock problem: an index below -1 reads before the list; the first part here.
            v = index >= 0 && index < static_cast<int>(parts.size()) ? parts[static_cast<size_t>(index)].cost
                                                                      : parts.front().cost;
        }
        total = total + capped(v);
    }
    if (total < 199.0f) total = 199.0f;
    return x87::chop(total);
}

void sessionResults(Career& career, CareerContext& context, const RaceSetup& race) {
    if (race.type == RaceType::TestTrack) return;
    RaceSession& s = career.session;
    if (s.event == nullptr || s.round == nullptr) return;
    const CalendarEvent& event = *s.event;
    const EventRound& round = *s.round;
    CareerProfile& p = career.profile;
    const PartCatalogue& catalogue = context.making.catalogue;
    const auto crewAt = [&](int32_t k) -> Crew* {
        return k >= 0 && k < 9 ? &career.crews[static_cast<size_t>(k)] : nullptr;
    };
    Crew* host = crewAt(event.crew);
    const int32_t opponents = std::min<int32_t>(round.opponents, 6);
    const int32_t racers = std::min<int32_t>(round.drivers, 6);
    // The cheat "win all races" ([career + 0x64] +0x04) makes the order's first the player's crew (0).
    if (career.cheatFlags[kCheatWinAllRaces] != 0) s.order[0] = 0;
    // The order into the stats' table by the event's round (+0x55).
    for (int32_t i = 0; i < opponents; ++i) {
        // A round past the first (+0x55 over 0) would write past the stats' 0x90 bytes (0x004B9E9F), but the event's
        // +0x55 is always 0 (its builders clear it, nothing else writes it); not kept here.
        if (event.roundIndex == 0) career.stats.order[static_cast<size_t>(i)] = s.order[static_cast<size_t>(i)];
    }
    if (round.type == EventKind::Showoff) {
        const int32_t money = x87::chop(raceMoney(career));
        for (Crew& crew : career.crews) respectByMoney(career, crew, money, true);
    }
    respectByBestCar(career, career.crews[5], catalogue, true);
    respectByCollection(career, career.crews[6], catalogue, true);

    // The places: after the player's crew each crew placed counts in the profile's +0x38 words; the player's crew
    // placed counts in +0x4E and every crew's +0x2CC; first, the round's type's word (+0x18), the event's +0x0D word
    // (+0x26) and +0x628 by 0.04 (none above 0), else +0x4C; the host crew's +0x2C4 (first) or +0x2C8 with the
    // event's +0x02; then each crew's respect of the place: the event's +0x03 (first (crew + 1) x 20, second -30) x
    // its factor 8 to its gain 8, else the standing (with the round's +0x08), the place (round types 3 and 4) or the
    // points' place (type 5).
    bool ownPlaced = false;
    const int32_t n = round.opponents - 1;
    for (int32_t i = 0; i < opponents; ++i) {
        const int32_t crew = s.order[static_cast<size_t>(i)];
        // Stock problem: a crew -1 counts in the word before (+0x36, the +0x26 words' last).
        if (ownPlaced) countProfile(p, 0x38 + crew * 2);
        if (crew != 0) continue;
        countProfile(p, 0x4E);
        for (Crew& c : career.crews) ++c.racesSinceCall;
        ownPlaced = true;
        if (i == 0) {
            countProfile(p, 0x18 + static_cast<int32_t>(round.type) * 2);
            countProfile(p, 0x26 + event.location * 2);
            p.rivalsSkill = p.rivalsSkill + 0.04f;
            if (p.rivalsSkill > 0.0f) p.rivalsSkill = 0.0f;
        } else {
            countProfile(p, 0x4C);
        }
        if (event.hostedByPlayer != 0 && host != nullptr) {
            if (i == 0) {
                host->hostWon = 1;
            } else {
                host->hostLost = 1;
            }
        }
        for (Crew& c : career.crews) {
            if (event.pinkSlip != 0) {
                float v = 0.0f;
                if (i == 0) {
                    v = static_cast<float>((event.crew + 1) * 20);
                } else if (i == 1) {
                    v = -30.0f;
                }
                c.respectGain[8] = v * c.respectFactor[8] + c.respectGain[8];
                continue;
            }
            if (round.teams != 0) respectByStanding(c, i, n, true);
            if (round.type == EventKind::PointToPoint || round.type == EventKind::Circuit) {
                respectByPlace(career, c, i, n, true, nullptr);
            } else if (round.type == EventKind::Sprint) {
                respectByPoints(career, c, i, n, true, nullptr);
            }
        }
    }

    // A bet: the first of the player's crew and the crew bet with in the order: the player's crew sets the bets'
    // +0x04 and the rival crew's money x 0.8 (+0x338 0), the rival x 1.2 (+0x338 1, +0x04 0); at least 2500.
    CareerBets& bets = career.bets;
    if (bets.placed != 0) {
        for (int32_t i = 0; i < opponents; ++i) {
            const int32_t crew = s.order[static_cast<size_t>(i)];
            if (crew != 0 && crew != bets.crew) continue;
            Crew* rival = crewAt(bets.crew);
            if (crew == 0) {
                bets.won = 1;
                if (rival != nullptr) {
                    rival->lastBetWon = 0;
                    rival->money = rival->money * 0.8f;
                }
            } else {
                bets.won = 0;
                if (rival != nullptr) {
                    rival->lastBetWon = 1;
                    rival->money = rival->money * 1.2f;
                }
            }
            if (rival != nullptr && rival->money < 2500.0f) rival->money = 2500.0f;
            break;
        }
    }

    // The first crew's odds: a stake (+0x0C) wins stake x (against / for + 1) into +0x10.
    const int32_t first = s.order[0];
    if (first >= 0 && first < 9) {
        CrewOdds& o = s.odds[static_cast<size_t>(first)];
        if (o.staked != 0) o.winnings = static_cast<float>(o.staked) * (o.against / o.stake + 1.0f) + o.winnings;
    }
    // Stock problem: with no first (-1: none placed, as after a cruise, whose racers 0x004BA8A0 leaves unfinished)
    // the odds before the table are read and written (0x004BA195 .. 0x004BA1CA): the last racer's level timer (+0x260)
    // is the stake, its order (+0x258, a whole number read as a float) and order time (+0x25C) against and for, and
    // the session's +0x264 (the set-up flag) the winnings, which takes the float (any stake sets it: the next
    // 0x004B8570 sets the session up again). The fixed build reads and writes nothing then.
    if (first == 0) {
        ++career.stats.wins;
        if (host != nullptr) ++host->racesWonHosted;
    }
    // 0x004AA440: each racer of the player's crew: its crewmate's +0xB0 counts, +0xBC 0, +0xB4 counts a win; it
    // learns (0x004AA3B0).
    for (uint32_t r = 0; r < s.count && r < s.racers.size(); ++r) {
        const Racer& racer = s.racers[r];
        if (racer.crew != 0) continue;
        // Stock problem: a member past the crewmates the profile has reads an old pointer; none here.
        if (racer.member < 0 || racer.member >= p.memberCount || racer.member >= 4) continue;
        const auto number = static_cast<size_t>(career.members[static_cast<size_t>(racer.member)]);
        if (number >= career.crewmates.size()) continue;
        Crewmate& mate = career.crewmates[number];
        ++mate.races;
        mate.daysIdle = 0;
        if (first == 0) ++mate.wins;
        learnFromRace(career, mate);
    }
    for (size_t k = 1; k < 9; ++k) {
        Crew& c = career.crews[k];
        c.respectGain[3] = c.rams * c.respectFactor[3] * -0.3f;
    }

    // The event's prototype part (+0x70, the round's +0x02, won): each car of the player's crew takes the part of the
    // event's kind (+0x74) of its supplier (+0x78), is rated again (0x0048E8A0) and noted (0x1C: the supplier, the
    // kind, the car's model); the profile's part state of the model and kind 3.
    const auto ownCar = [&](int32_t i) -> Car* {
        const Racer& racer = s.racers[static_cast<size_t>(i)];
        if (racer.crew != 0 || racer.car < 0 || static_cast<size_t>(racer.car) >= career.garage.size()) return nullptr;
        return &career.garage[static_cast<size_t>(racer.car)];
    };
    if (event.asksPrototype != 0 && round.prototypePrize != 0 && first == 0) {
        for (int32_t i = 0; i < racers; ++i) {
            Car* car = ownCar(i);
            if (car == nullptr || car->def == nullptr) continue;
            // Stock problem: with no such part the game stops; the event is only made with one.
            const CarPart* part = prototypePart(catalogue, event.prototypeKind, event.prototypeSupplier);
            if (part == nullptr || event.prototypeKind < 0 || event.prototypeKind >= 10) continue;
            car->mods[static_cast<size_t>(event.prototypeKind)] = static_cast<uint8_t>(part->index);
            rateCar(*car, context.dynamics.of(*car->def), catalogue);
            const int id = car->def->id;
            career.meeting.add(0x1C, static_cast<float>(event.prototypeSupplier),
                               static_cast<float>(event.prototypeKind), static_cast<float>(id));
            if (id >= 0 && id < int(CareerProfile::kModelCapacity)) p.performance(static_cast<size_t>(id))[static_cast<size_t>(event.prototypeKind)] = 3;
        }
    }

    // The garage cars' counters (+0x98 .. +0xA6) by the event's type, its +0x03 and a win.
    for (int32_t i = 0; i < racers; ++i) {
        Car* car = ownCar(i);
        if (car == nullptr) continue;
        Car& c = *car;
        if (event.type != EventKind::Showoff) ++c.races(CarRaces::Races);
        if (event.type == EventKind::Circuit || event.type == EventKind::PointToPoint) ++c.races(CarRaces::Circuits);
        if (event.type == EventKind::Sprint) ++c.races(CarRaces::Sprints);
        if (event.type == EventKind::Showoff) ++c.races(CarRaces::Showoffs);
        if (event.pinkSlip != 0) ++c.races(CarRaces::PinkSlips);
        if (first == 0) {
            ++c.races(CarRaces::Wins);
            if (event.type == EventKind::Circuit || event.type == EventKind::PointToPoint) {
                ++c.races(CarRaces::CircuitWins);
            }
            if (event.type == EventKind::Sprint) ++c.races(CarRaces::SprintWins);
        }
    }

    // The unlocks of each car's model (0x004B8230, 10 changes from the profile's +0x4E over 10, else 4; 8 for a race
    // lost), noted (0x1D: the model): a challenge met; a cruise whose money (cut) is over crew 3's mark (+0x6C, cut);
    // else a race without the profile's +0x00 and +0x630.
    const int32_t many = p.respectLevel > 10 ? 10 : 4;
    const auto unlock = [&](const Car& car, int changes) {
        const int id = car.def != nullptr ? car.def->id : -1;
        if (id < 0 || id >= int(CareerProfile::kModelCapacity)) return;
        if (unlockSteps(p, static_cast<size_t>(id), changes, context.tables)) {
            career.meeting.add(0x1D, static_cast<float>(id), 0.0f, 0.0f);
        }
    };
    for (int32_t i = 0; i < racers; ++i) {
        Car* car = ownCar(i);
        if (car == nullptr) continue;
        if (event.challenge != 0) {
            if (challengeMet(career, race)) unlock(*car, many);
        } else if (event.type == EventKind::Showoff) {
            if (x87::chop(raceMoney(career)) > x87::chop(career.crews[3].moneyMark)) unlock(*car, many);
        } else if (p.introduction == 0 && p.outOfRace == 0) {
            if (first == 0) {
                unlock(*car, many);
            } else {
                unlock(*car, p.respectLevel > 10 ? 8 : 4);
            }
        }
    }

    if (bets.placed != 0 && bets.pinkSlip == 0) {
        for (Crew& c : career.crews) respectByBet(career, c, bets.won != 0, true);
    }
    if (p.outOfRace != 0) {
        const int32_t best = respectByBestCar(career, career.crews[5], catalogue, true);
        const int32_t collection = respectByCollection(career, career.crews[6], catalogue, true);
        career.news.respect = static_cast<float>(x87::chop(career.news.respect) + collection + best);
    }
}

void endEvent(Career& career, CalendarEvent& event, const ExeTables& tables) {
    event.done = 1;
    if (career.stats.oddsWon > career.stats.oddsStaked) {
        for (Crew& crew : career.crews) respectByBet(career, crew, true, true);
    }
    if (career.profile.introduction != 0) {
        clearNotes(career, career.meeting);
        clearNotes(career, career.respectChanges);
        return;
    }
    for (size_t k = 1; k < 9; ++k) updateRespect(career, career.crews[k], tables);
    noteLearning(career);
}

void racerFinished(Career& career, int32_t racer, float raceTime, float bestLap, bool other) {
    RaceSession& s = career.session;
    if (racer < 0 || static_cast<size_t>(racer) >= s.racers.size()) return;
    Racer& r = s.racers[static_cast<size_t>(racer)];
    const int32_t crew = r.crew;
    if (crew < 0 || static_cast<size_t>(crew) >= s.finished.size()) return;
    ++s.finished[static_cast<size_t>(crew)];
    r.finished = 1;
    if (const CalendarEvent* event = currentEvent(career.calendar); event != nullptr && event->challenge != 0) {
        const auto host = static_cast<size_t>(event->crew);
        const auto index = static_cast<size_t>(event->challengeIndex);
        if (host < career.crews.size() && index < career.crews[host].challenges.size()) {
            const float time = other ? -1.0f : bestLap;
            std::memcpy(&career.crews[host].challenges[index].result, &time, 4);
        }
    }
    if (s.winner == -1) {
        s.winner = racer;
        s.winnerCrew = crew;
    }
    const int32_t needed = s.round != nullptr && s.round->teams == 1 ? s.round->teamSize : 1;
    if (s.finished[static_cast<size_t>(crew)] == needed && s.placed >= 0 &&
        static_cast<size_t>(s.placed) < s.order.size()) {
        s.order[static_cast<size_t>(s.placed)] = crew;
        if (static_cast<size_t>(crew) < s.odds.size()) s.odds[static_cast<size_t>(crew)].raceTime = raceTime;
        ++s.placed;
    }
}

void finishSession(Career& career, RaceType raceType, float routeRecord, CrtRandom& random) {
    RaceSession& s = career.session;
    bool ownInFirst = false;
    for (size_t k = 0; k < 5; ++k) {
        if (s.order[k] == 0) ownInFirst = true;
    }
    if (s.order[5] != 0 && !ownInFirst) {
        career.profile.outOfRace = 1;
    }
    if (s.order[0] == -1) {
        for (CrewOdds& o : s.odds) {
            o.against = 0.0f;
            o.stake = 1.0f;
            o.staked = 0;
            o.winnings = 0.0f;
        }
    }
    if (raceType == RaceType::Cruise) return;
    std::vector<uint32_t> list;
    for (uint32_t number = 0; number < s.count; ++number) {
        for (uint32_t r = 0; r < s.count && r < s.racers.size(); ++r) {
            const Racer& racer = s.racers[r];
            if (racer.finished == 0 && racer.number == static_cast<int32_t>(number)) {
                list.push_back(r);
                break;
            }
        }
    }
    if (list.size() > 1) {
        const auto r1 = static_cast<uint32_t>(random.next());
        const auto r2 = static_cast<uint32_t>(random.next());
        const auto laps = static_cast<uint32_t>(static_cast<int32_t>(s.round != nullptr ? s.round->laps : 0));
        // Stock problem: a round of no laps divides by 0 there (the game stops); no swaps from it here.
        uint32_t swaps = r2 % 6u + 2u * (laps == 0 ? 0u : r1 % laps);
        const auto last = static_cast<uint32_t>(list.size() - 1);
        while (swaps > 0) {
            const uint32_t k = static_cast<uint32_t>(random.next()) % last;
            std::swap(list[k], list[k + 1]);
            --swaps;
        }
    }
    auto t = static_cast<uint32_t>(x87::chop(routeRecord));
    if (t < 10u) {
        t = 180u;
    } else {
        t = t + static_cast<uint32_t>(random.next()) % 15u;
    }
    for (const int32_t crew : s.order) {
        if (crew == -1) continue;
        const float time = s.odds[static_cast<size_t>(crew)].raceTime;
        if (x87::asUnsigned(t) < time) t = static_cast<uint32_t>(x87::chop(time));
    }
    const auto finish = [&](bool ownCrew) {
        for (size_t i = 0; i < list.size(); ++i) {
            const uint32_t r = list[i];
            if ((s.racers[r].crew == 0) != ownCrew) continue;
            const auto drawn = static_cast<float>(static_cast<uint32_t>(random.next()) % 20u);
            const float time = (drawn * 0.1f + static_cast<float>(i) * 3.2f) + x87::asUnsigned(t);
            racerFinished(career, static_cast<int32_t>(r), time, 0.0f, true);
        }
    };
    finish(false);
    finish(true);
}

int32_t raceRespectPlace(Career& career, const std::vector<const RaceCar*>& racerCars) {
    return pointsPlace(career, racerCars);
}

int32_t raceRespectLead(Career& career, const std::vector<const RaceCar*>& racerCars) {
    return crewLead(career, racerCars);
}

const EventRound* roundOf(const Career& career, const CalendarEvent& event) {
    const auto index = static_cast<int8_t>(event.roundIndex);
    // An event keeps one round (+0x1C, 0x38 bytes); a later index would read the bytes after it (the next events),
    // but the index (+0x55) is always 0: the event builders clear it and nothing else writes it. Within the calendar
    // the same bytes are read here.
    const auto* base = reinterpret_cast<const uint8_t*>(&event.round);
    const auto* end = reinterpret_cast<const uint8_t*>(&career.calendar) + sizeof(Calendar);
    const ptrdiff_t at = static_cast<ptrdiff_t>(index) * static_cast<ptrdiff_t>(sizeof(EventRound));
    const auto* begin = reinterpret_cast<const uint8_t*>(&career.calendar);
    if (base + at < begin || base + at + sizeof(EventRound) > end) return &event.round;
    return reinterpret_cast<const EventRound*>(base + at);
}

const std::vector<Car>& carsOf(const Career& career, int32_t crew) {
    return crew == 0 ? career.garage : career.opponentCars;
}

uint32_t betRacer(const Career& career) {
    const RaceSession& s = career.session;
    for (uint32_t i = 0; i < s.count && i < s.racers.size(); ++i) {
        if (s.racers[i].crew == career.bets.crew && s.racers[i].member == 0) return i;
    }
    return 0;
}

Car copyCar(const Car& car) {
    Car copy = car;
    copy.grid.reset();
    copy.grid.emplace();
    return copy;
}

void pinkSlipCar(Career& career, const PartCatalogue& catalogue) {
    const CareerBets& bets = career.bets;
    const auto crewIndex = static_cast<size_t>(bets.crew);
    if (crewIndex >= career.crews.size()) return;
    Crew& crew = career.crews[crewIndex];
    if (bets.won == 0) {
        if (!career.garage.empty()) career.garage.pop_back();
        crew.pinkSlipsWon = 0;
        career.profile.raceCarKept = 0;
        return;
    }
    if (career.garage.size() >= 55) return;
    const std::vector<Car>& cars = carsOf(career, bets.crew);
    const auto index = static_cast<size_t>(leaderCar(career, bets.crew));
    if (index >= cars.size()) return;
    career.garage.push_back(copyCar(cars[index]));
    ++crew.pinkSlipsWon;
    const Car& won = career.garage.back();
    if (won.def == nullptr) return;
    const auto model = static_cast<size_t>(won.def->id);
    if (model >= CareerProfile::kModelCapacity) return;
    for (size_t kind = 0; kind < kModKinds; ++kind) {
        const auto part = static_cast<int8_t>(won.mods[kind]);
        const std::vector<CarPart>& parts = catalogue.lists[kind].parts;
        if (part < 0 || parts.empty()) continue;
        const CarPart& p = static_cast<size_t>(part) < parts.size() ? parts[static_cast<size_t>(part)] : parts[0];
        int8_t& known = career.profile.performance(model)[kind];
        if (known < p.level) known = static_cast<int8_t>(p.level);
    }
    for (size_t category = 0; category < 10; ++category) {
        const auto part = static_cast<int8_t>(won.body[category]);
        int8_t& known = career.profile.bodyParts(model)[category];
        if (part >= 0 && known < part) known = category == 3 ? 100 : part;
    }
}

namespace {

// 0x004EA100: the session player's user referenced (-1 past them).
int32_t referencedUser(const net::NetPlayer& p) {
    if (net::OnlineUser* users = net::onlineUser()) users->userAddRef(p.profile);
    return p.profile;
}

// The fixed build lets the user go after its report (the stock kept the reference).
void releaseUser(int32_t user) {
    if (net::OnlineUser* users = net::onlineUser()) users->userRelease(user);
}

std::vector<net::NetPlayer>& netPlayers() {
    static std::vector<net::NetPlayer> none;
    net::NetGame* game = net::netGame();
    return game != nullptr ? game->session.players() : none;
}

}  // namespace

void onlineStakes(Career& career, const Car& own, const Car& rival) {
    career.onlineOwnStake.assign(1, copyCar(own));
    career.onlineRivalStake.assign(1, copyCar(rival));
    if (net::stats::gameStatistics() != nullptr) {
        bool ok = false;
        for (const net::NetPlayer& p : netPlayers()) {
            if (p.peer == -1 && p.localIndex == 0) {
                const int32_t user = referencedUser(p);
                ok = net::stats::reportCareerRaceBegun(user);
                releaseUser(user);
            }
        }
        if (ok) net::stats::reporter()->send();
    }
    career.onlinePinkSlip = 1;
    career.valueAC = 0;
    career.onlineSettled = 0;
    career.onlineAutosave = 0;
}

void onlineSettle(Career& career, bool won, bool tie, const PartCatalogue& catalogue, DynamicsStore* dynamics) {
    if (career.onlinePinkSlip == 0) return;
    int value = 0;
    if (!tie) {
        if (!won) {
            if (!career.garage.empty()) career.garage.pop_back();
        } else if (!career.onlineRivalStake.empty()) {
            Car car = copyCar(career.onlineRivalStake.front());
            if (!car.grid) car.grid.emplace();
            if (car.def != nullptr) rateCar(car, dynamics != nullptr ? dynamics->of(*car.def) : nullptr, catalogue);
            career.garage.push_back(std::move(car));
            const Car& taken = career.garage.back();
            value = carValue(taken, catalogue);  // 0x0048CFE0, for the statistics
            const auto model = taken.def != nullptr ? static_cast<size_t>(taken.def->id) : CareerProfile::kModelCapacity;
            for (size_t kind = 0; kind < kModKinds && model < CareerProfile::kModelCapacity; ++kind) {
                const auto part = static_cast<int8_t>(taken.mods[kind]);
                const std::vector<CarPart>& parts = catalogue.lists[kind].parts;
                if (part < 0 || parts.empty()) continue;
                const CarPart& p = static_cast<size_t>(part) < parts.size() ? parts[static_cast<size_t>(part)] : parts[0];
                int8_t& known = career.profile.performance(model)[kind];
                if (known < p.level) known = static_cast<int8_t>(p.level);
            }
        }
        if (net::stats::gameStatistics() != nullptr) {
            bool reported = false;
            for (const net::NetPlayer& p : netPlayers()) {
                if (reported) break;
                if (p.peer == -1 && p.localIndex == 0) {
                    const int32_t user = referencedUser(p);
                    net::stats::reportPinkSlip(user, won, static_cast<uint32_t>(value));
                    releaseUser(user);
                    reported = true;
                }
            }
            if (reported) net::stats::reporter()->send();
        }
    }
    career.onlineOwnStake.clear();
    career.onlineRivalStake.clear();
    career.onlinePinkSlip = 0;
    career.valueAC = 0;
}

int32_t leaderCar(const Career& career, int32_t crew) {
    const RaceSession& s = career.session;
    for (uint32_t i = 0; i < s.count && i < s.racers.size(); ++i) {
        if (s.racers[i].crew == crew && s.racers[i].member == 0) return s.racers[i].car;
    }
    return 0;
}

bool mayRace(const Career& career, const CalendarEvent& event, const PartCatalogue& catalogue) {
    const CareerProfile& p = career.profile;
    const int team = event.round.teamSize;
    const int32_t listed = p.raceChoiceCount;
    if (listed < team || p.memberCount < team) return false;
    const Crew& crew = career.crews[static_cast<size_t>(event.crew)];
    if (crew.stage[1] == 0 && event.type != EventKind::Showoff && event.challenge == 0) return false;
    int reason = 0;
    mayEnter(career, event, reason, catalogue);
    return reason != 0 && reason != 1 && reason != 7;
}

void attendEvent(Career& career, CalendarEvent& event, const PartCatalogue& catalogue) {
    event.attend[0] = 1;
    if (event.crew == 0) return;
    int reason = 0;
    if (!mayEnter(career, event, reason, catalogue)) return;
    if (reason == 0 || reason == 1 || reason == 7) return;
    pay(career, event.entranceFee);
}

void clearBets(CareerBets& bets) {
    bets.talkedTo.fill(0);
    bets.rivalAsked = 0;
    bets.placed = 0;
    bets.won = 0;
    bets.amount = 0.0f;
    bets.pinkSlip = 0;
}

void pay(Career& career, int32_t amount) {
    const int32_t back = -amount;
    if (back < 1) {
        career.stats.moneyPaid += amount;
    } else {
        career.stats.moneyReceived += back;
    }
    career.profile.money += back;
    if (career.profile.money < 0) career.profile.money = 0;
}

void placeBet(Career& career, int32_t amount) {
    career.bets.placed = 1;
    career.bets.amount = static_cast<float>(amount);
    pay(career, amount);
}

void changeRespect(Crew& crew, float change) {
    float respect = static_cast<float>(crew.respect) + change;
    if (respect < 0.0f) respect = 0.0f;
    crew.respect = static_cast<int32_t>(respect);  // cut (the code sets the FPU to chop)
}

void keepHighBets(CareerBets& bets) {
    bets.amountCount = 1;
    bets.amounts[0] = bets.amounts[4];
    ++bets.amountCount;
    bets.amounts[1] = bets.amounts[5];
    ++bets.amountCount;
    bets.amounts[2] = bets.amounts[6];
}

void enterCareer(Career& career, CareerContext& context, const std::string& driverName) {
    career.head = 0;
    newCareer(career, context, driverName);
    CareerBets& bets = career.bets;
    bets.crew = 1;
    if (bets.pinkSlip == 0) {
        bets.rivalAsked = 1;
        bets.kind = BetKind::Money;
        bets.firstStage = 3;
    } else {
        bets.kind = BetKind::PinkSlip;
        bets.firstStage = 0xD;
    }
    setupBets(career, 1);
    bets.talkedTo[1] = 1;
    bets.amount = 0.0f;
    CareerProfile& profile = career.profile;
    profile.raceMembers[0] = 0;
    profile.raceCars[0] = 0;
    profile.raceCarCount = 1;
    profile.raceCarsChosen = 1;
    career.session.needsSetup = 1;
    setupRace(career, context);
}

void startRace(Career& career, CareerContext& context, RaceSetup& race, int32_t automaticGearbox,
               int32_t networkGame) {
    CalendarEvent* event = currentEvent(career.calendar);
    if (event == nullptr) {
        // Stock problem: without an event of the current day the stock code reads through a null pointer.
        return;
    }
    // Older native builds left player-hosted meetings pointing at challenge zero.
    // Repair that sentinel before choosing rules (otherwise its zero-second limit
    // ends the race immediately). Genuine crew challenges retain their index.
    if (event->crew == 0 && event->challenge == 0 &&
        (event->type == EventKind::Sprint || event->type == EventKind::Circuit ||
         event->type == EventKind::PointToPoint)) {
        event->challengeIndex = -1;
    }
    const EventRound& round = *roundOf(career, *event);
    for (Crew& crew : career.crews) crew.rams = 0.0f;
    career.news = RaceNews{};
    career.news.place = -1;
    if (career.profile.racing != 0) ++career.stats.racesEntered;
    const auto eventCrew = static_cast<size_t>(static_cast<uint8_t>(event->crew));
    if (event->challengeIndex == -1) {
        switch (round.type) {
        case EventKind::Showoff: race.type = RaceType::Cruise; break;
        case EventKind::PointToPoint: race.type = RaceType::PointToPoint; break;
        case EventKind::Circuit: race.type = RaceType::Circuit; break;
        case EventKind::Sprint:
            race.type = RaceType::Sprint;
            race.heat = 0;
            race.heats = networkGame == 0 ? 3 : 1;
            break;
        default: break;
        }
    } else if (eventCrew < career.crews.size() && event->challengeIndex >= 0 && event->challengeIndex < 3) {
        CrewChallenge& challenge = career.crews[eventCrew].challenges[static_cast<size_t>(event->challengeIndex)];
        challenge.result = static_cast<int32_t>(0xBF800000u);  // -1.0f
        switch (challenge.type) {
        case ChallengeType::LapTime:
        case ChallengeType::PerfectLap:
            race.type = static_cast<RaceType>(challenge.type);
            race.lapTime = static_cast<float>(challenge.value);
            race.challenge = ChallengeOutcome::Met;
            break;
        case ChallengeType::Speed:
            race.type = RaceType::Speed;
            race.speed = static_cast<float>(challenge.value);
            race.challenge = ChallengeOutcome::Met;
            break;
        case ChallengeType::Showoff: race.type = RaceType::Cruise; break;
        default: break;
        }
    }
    race.laps = round.laps;
    const auto route = static_cast<size_t>(round.route);
    const std::vector<ExeTables::Route>& routes = context.tables.routes;
    race.track = route < routes.size() ? routes[route].track.substr(0, 31) : std::string();
    race.route = route < routes.size() ? routes[route].name.substr(0, 31) : std::string();
    race.players = 1;
    // A round whose +0x18 is above 4 is not raced at night (4) or at time 0; wet (2) only for a +0x18 up to 4, and
    // not at night above 2.
    while (round.drivers > 4 && (event->timeOfDay == 4 || event->timeOfDay == 0)) {
        event->timeOfDay = static_cast<int32_t>(static_cast<uint32_t>(context.making.random.next()) % 5);
    }
    if (round.drivers > 2 && event->timeOfDay == 4 && event->weather == 2) event->weather = 1;
    if (round.drivers > 4 && event->weather == 2) event->weather = 1;
    race.timeOfDay = event->timeOfDay;
    race.weather = event->weather;
    race.gearbox = race.type == RaceType::Sprint ? 0 : automaticGearbox;
    // A round of type 2 sets the cruise up for 3 minutes (0x004AA730); then the cruise's +0x10 = 1.
    if (round.type == EventKind::Showoff) career.cruise.setup(3, race.mode, race.type);
    career.cruise.started = 1;
}

void updateBetShare(Career& career) {
    const RaceSession& s = career.session;
    CareerProfile& profile = career.profile;
    profile.betShare = 0.0f;
    float total = static_cast<float>(s.odds[0].staked);
    for (size_t i = 1; i < s.odds.size(); ++i) total = total + static_cast<float>(s.odds[i].staked);
    if (career.bets.placed != 0) {
        total = total + static_cast<float>(static_cast<int32_t>(career.bets.amount));  // cut
    }
    if (total > 0.0f) {
        // Stock problem: with no money left the share is a division by zero (infinity).
        profile.betShare = total / static_cast<float>(profile.money) * 100.0f;
    }
}

const std::array<OddsFraction, 33>& oddsFractions() {
    static const std::array<OddsFraction, 33> table = [] {
        std::array<OddsFraction, 33> t{};
        for (size_t i = 0; i < t.size(); ++i) {
            t[i].against = kFractions[i][0];
            t[i].stake = kFractions[i][1];
            t[i].value = static_cast<float>(kFractions[i][0]) / static_cast<float>(kFractions[i][1]);
        }
        // Neighbours swap when the first is larger, and the pass starts again.
        for (size_t i = 0; i + 1 < t.size();) {
            if (t[i + 1].value < t[i].value) {
                std::swap(t[i], t[i + 1]);
                i = 0;
            } else {
                ++i;
            }
        }
        return t;
    }();
    return table;
}

int carValue(const Car& car, const PartCatalogue& catalogue) {
    float total = static_cast<float>(car.def != nullptr ? car.def->value : 0);
    for (uint8_t b : car.body) {
        const auto level = static_cast<int8_t>(b);
        const int v = level == -1 || level == 0 || level > 0x7E ? 0 : (level + 3) * 500;
        total = total + static_cast<float>(v);
    }
    for (size_t k = 0; k < kModKinds; ++k) {
        const auto index = static_cast<int8_t>(car.mods[k]);
        int v = 0;
        const std::vector<CarPart>& parts = catalogue.lists[k].parts;
        if (index != -1 && !parts.empty()) {
            // Stock problem: an index below -1 reads before the list; the first part here.
            v = index >= 0 && index < static_cast<int>(parts.size()) ? parts[static_cast<size_t>(index)].cost
                                                                      : parts.front().cost;
        }
        total = total + static_cast<float>(v);
    }
    if (total < 199.0f) total = 199.0f;
    return x87::chop(total);
}

CalendarEvent* currentEvent(Calendar& calendar) {
    for (CalendarEvent& e : calendar.events) {
        if (e.used != 0 && e.dayOfMonth == calendar.dayOfMonth && e.month == calendar.month) return &e;
    }
    return nullptr;
}

void setupBets(Career& career, int crew) {
    CareerBets& b = career.bets;
    b.rivalMoney = crew >= 0 && crew < static_cast<int>(career.crews.size())
                       ? career.crews[static_cast<size_t>(crew)].money
                       : 0.0f;
    if (career.profile.introduction != 0) b.rivalMoney = 2000.0f;
    b.amountCount = 0;
    const float step = b.rivalMoney * 0.2f;
    const float first = b.rivalMoney - 3.0f * step;
    for (int i = 0; i < 7; ++i) {
        const float amount = static_cast<float>(i) * step + first;
        b.amounts[static_cast<size_t>(i)].amount = static_cast<int32_t>(amount);  // cut
        b.amounts[static_cast<size_t>(i)].weight = kBetWeights[static_cast<size_t>(i)];
        ++b.amountCount;
    }
    for (BetAmount& a : b.amounts) a.amount = a.amount / 100 * 100;
}

void setupRace(Career& career, CareerContext& context) {
    RaceSession& s = career.session;
    CalendarEvent* event = currentEvent(career.calendar);
    if (s.needsSetup != 0 && event != nullptr) {
        const EventRound& round = *roundOf(career, *event);
        s.event = event;
        s.round = &round;
        const auto eventCrew = static_cast<size_t>(static_cast<uint8_t>(event->crew));
        if (eventCrew < career.crews.size()) {
            const auto route = static_cast<size_t>(career.crews[eventCrew].circuitRoute[0]);
            s.location = route < context.tables.routes.size() ? context.tables.routes[route].location : 0;
        }
        s.count = static_cast<uint32_t>(std::min(round.opponents * round.teamSize, 6));
        for (Racer& r : s.racers) {
            r.crew = -1;
            r.member = -1;
            r.car = -1;
        }
        uint32_t number = 0;
        for (size_t c = 0; c < 9 && number < s.count; ++c) {
            if (round.crews[c] == 0) continue;
            Crew& crew = career.crews[c];
            crew.raceCars.fill(-1);
            for (int32_t k = 0; k < round.teamSize && number < s.count; ++k) {
                Racer& r = s.racers[number];
                r.raceCar = 0;
                r.crew = static_cast<int32_t>(c);
                r.number = static_cast<int32_t>(number);
                r.cars = c == 0 ? &career.garage : &career.opponentCars;
                r.finished = 0;
                r.driver = 0;
                r.rammed = 0;
                r.crashed = 0;
                if (c == 0) {
                    const auto slot = static_cast<size_t>(k);
                    r.member = slot < 3 ? career.profile.raceMembers[slot] : 0;
                    if (r.member == 0) r.driver = 1;
                    r.car = slot < 3 ? career.profile.raceCars[slot] : 0;
                } else {
                    r.car = static_cast<int32_t>(pickOpponentCar(career, context, crew));
                    if (career.profile.introduction != 0) {
                        // The first event: the first opponents' car of model 50.
                        r.car = 0;
                        for (size_t i = 0; i < r.cars->size(); ++i) {
                            const CarDef* def = (*r.cars)[i].def;
                            if (def != nullptr && def->id == 0x32) {
                                r.car = static_cast<int32_t>(i);
                                break;
                            }
                        }
                    }
                    r.member = k == 0 ? 0 : pickMember(career, crew, context.making.random);
                }
                const auto m = static_cast<size_t>(r.member);
                r.name = m < 4 ? widenSigned(std::string_view(crew.memberShortNames[m].data(),
                                                              crew.memberShortNames[m].size()))
                               : L"";
                ++number;
            }
        }
        // The racers by their key, largest first: neighbours swap when the second is larger, and the pass starts
        // again.
        for (uint32_t i = 0; i + 1 < s.count;) {
            const float a = sortKey(s.racers[i], context.making.catalogue);
            const float b = sortKey(s.racers[i + 1], context.making.catalogue);
            if (a < b) {
                std::swap(s.racers[i], s.racers[i + 1]);
                i = 0;
            } else {
                ++i;
            }
        }
        for (uint32_t i = 0; i < s.count; ++i) {
            s.racers[i].place = static_cast<int32_t>(i);
            s.racers[i].number = static_cast<int32_t>(i);
        }
        s.placed = 0;
        s.odds.fill(CrewOdds{});
        s.finished.fill(0);
        s.order.fill(-1);
        s.winner = -1;
        s.winnerCrew = -1;
        setupOdds(career);
        setupDrivers(career, context.making.random);
    } else if (s.needsSetup != 0) {
        // Stock problem: without an event of the current day the stock code reads through a null pointer; nothing
        // is set up here.
        return;
    }
    s.bet = career.bets.placed;
    if (s.bet != 0) s.betRacer = betRacer(career);
    s.needsSetup = 0;
}

void setupOdds(Career& career) {
    RaceSession& s = career.session;
    if (s.count < 2 || career.profile.raceCarsChosen != 0 || career.profile.racing != 0 || s.round == nullptr) {
        return;
    }
    const auto& table = oddsFractions();
    // A crew leader's odds by the laps and the grid place.
    for (uint32_t i = 0; i < s.count; ++i) {
        const Racer& r = s.racers[i];
        if (r.member != 0) continue;
        const int laps = s.round->laps;
        float base;
        if (laps < 2) base = 6.0f;
        else if (laps < 4) base = 10.0f;
        else if (laps < 6) base = 12.0f;
        else if (laps < 8) base = 15.0f;
        else if (laps < 11) base = 20.0f;
        else if (laps < 26) base = 30.0f;
        else continue;
        // The place is not checked (0x004B99D9), but it is the racer's number, 0 .. 5 (0x004B8A70, at most six
        // racers); 0 past the table's seven values here.
        const float share = r.number >= 0 && r.number < 7 ? kPlaceShare[static_cast<size_t>(r.number)] : 0.0f;
        s.odds[static_cast<size_t>(r.crew)].against = base - share;
    }
    // Each to the first fraction of the table at least as long.
    for (uint32_t i = 0; i < s.count; ++i) {
        const Racer& r = s.racers[i];
        if (r.member != 0) continue;
        CrewOdds& o = s.odds[static_cast<size_t>(r.crew)];
        const float value = o.against / o.stake;
        for (const OddsFraction& f : table) {
            if (f.value >= value) {
                o.against = static_cast<float>(f.against);
                o.stake = static_cast<float>(f.stake);
                break;
            }
        }
    }
    // While the chances (stake / (against + stake)) of the leaders add up to less than 1.05, the next leader in turn
    // gets the next shorter fraction (at most 1000 times).
    uint32_t turn = 0;
    for (uint32_t times = 0;;) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < s.count; ++i) {
            const Racer& r = s.racers[i];
            if (r.member != 0) continue;
            const CrewOdds& o = s.odds[static_cast<size_t>(r.crew)];
            sum = o.stake / (o.against + o.stake) + sum;
        }
        if (!(sum < 1.05f) || times >= 1000) return;
        ++times;
        do {
            turn = (turn + 1) % s.count;
        } while (s.racers[turn].member != 0);
        CrewOdds& o = s.odds[static_cast<size_t>(s.racers[turn].crew)];
        const float value = o.against / o.stake;
        for (size_t j = 1; j < table.size(); ++j) {
            if (table[j].value >= value) {
                o.against = static_cast<float>(table[j - 1].against);
                o.stake = static_cast<float>(table[j - 1].stake);
                break;
            }
        }
    }
}

int driveOf(const Car& car, const ExeTables& tables) {
    if (car.def == nullptr) return 0;
    const auto id = static_cast<uint32_t>(car.def->id);
    return id < tables.modelClasses.size() ? tables.modelClasses[id] : 0;
}

}  // namespace oj
