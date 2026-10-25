#include "Core/Career/CareerEvents.h"

#include "Core/Career/Phone.h"

#include "Core/Career/CareerInit.h"
#include "Core/Base/RawFields.h"
#include "Core/Base/X87.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace oj {

namespace {

uint32_t draw(CrtRandom& random) {
    return static_cast<uint32_t>(random.next());
}

// rand() / 32767 as the x87 at single precision gives it.
float fraction(CrtRandom& random) {
    return static_cast<float>(random.next()) * 3.0518509e-05f;
}

// rand() % count of a crew's routes. Stock problem: a crew without such a route divides by zero and the game stops;
// 0 here.
uint32_t pick(CrtRandom& random, int32_t count) {
    const uint32_t r = draw(random);
    return count != 0 ? r % static_cast<uint32_t>(count) : 0;
}

int nearest(float v) {
    return static_cast<int>(std::nearbyint(v));
}

const std::vector<Car>& carsOf(const Career& career, const Crew& crew) {
    return crew.id == 0 ? career.garage : career.opponentCars;
}

bool classMatches(const Car& car, int cls) {
    const int c = carClass(car);
    return c == cls || cls == -1 || cls == 8;
}

// 0x004A8B40 (region), 0x004A8BE0 (make), 0x004A8C80 (model): the crew's cars of that CarDef value and the class;
// 100 when the value is 0 (any).
int countByRegion(const Career& career, const Crew& crew, int region, int cls) {
    if (region == 0) return 100;
    int n = 0;
    for (const Car& car : carsOf(career, crew)) {
        if (car.def != nullptr && car.def->region == region && classMatches(car, cls)) ++n;
    }
    return n;
}

int countByMake(const Career& career, const Crew& crew, int make, int cls) {
    if (make == 0) return 100;
    int n = 0;
    for (const Car& car : carsOf(career, crew)) {
        if (car.def != nullptr && car.def->make == make && classMatches(car, cls)) ++n;
    }
    return n;
}

int countByModel(const Career& career, const Crew& crew, int model, int cls) {
    if (model == 0) return 100;
    int n = 0;
    for (const Car& car : carsOf(career, crew)) {
        if (car.def != nullptr && car.def->id == model && classMatches(car, cls)) ++n;
    }
    return n;
}

// 0x004A8EE0: the crew's cars of the class; 100 for class 8 (any).
int countByClass(const Career& career, const Crew& crew, int cls) {
    if (cls == 8) return 100;
    int n = 0;
    for (const Car& car : carsOf(career, crew)) {
        if (carClass(car) == cls) ++n;
    }
    return n;
}

// 0x004A8D20: the crew's cars of the class (8: any) with more than three prototype parts fitted.
int countPrototypeCars(const Career& career, const Crew& crew, int cls, const PartCatalogue& catalogue) {
    int n = 0;
    for (const Car& car : carsOf(career, crew)) {
        const int c = carClass(car);
        if (c != cls && cls != 8) continue;
        int prototypes = 0;
        for (size_t k = 0; k < kModKinds; ++k) {
            const auto index = static_cast<int8_t>(car.mods[k]);
            const std::vector<CarPart>& parts = catalogue.lists[k].parts;
            if (index == -1 || parts.empty()) continue;
            const CarPart& part = index >= 0 && index < static_cast<int>(parts.size()) ? parts[static_cast<size_t>(index)]
                                                                                          : parts.front();
            if (part.level == 3) ++prototypes;
        }
        if (prototypes > 3) ++n;
    }
    return n;
}

// 0x004A9920: the class of an event of the crew.
int pickClass(const Career& career, const Crew& crew, CareerContext& context) {
    CrtRandom& random = context.making.random;
    const std::vector<Car>& cars = carsOf(career, crew);
    if (cars.empty()) {
        return static_cast<int>(draw(random) % 7);
    }
    if (career.profile.introduction != 0) {
        return fraction(random) > 0.25f ? 7 : 6;
    }
    const int limit = context.tables.crewClassLimits[static_cast<size_t>(crew.id)];
    int cls = 0;
    int tries = 0;
    do {
        bool present[9] = {};
        for (const Car& car : cars) {
            const int c = carClass(car);
            if (c >= 0 && c < 9) present[c] = true;
        }
        do {
            cls = static_cast<int>(draw(random) % 9);
        } while (!present[cls]);
        if (cls > 0 && cls < 8 && draw(random) % 5 == 1) --cls;
    } while (limit < cls && ++tries < 100);
    return cls;
}

// 0x004A8A60, 0x004A8AD0: the region / make of one of the crew's cars.
int randomRegion(const Career& career, const Crew& crew, CrtRandom& random) {
    const std::vector<Car>& cars = carsOf(career, crew);
    const Car& car = cars[draw(random) % cars.size()];
    return car.def != nullptr ? car.def->region : 0;
}

int randomMake(const Career& career, const Crew& crew, CrtRandom& random) {
    const std::vector<Car>& cars = carsOf(career, crew);
    const Car& car = cars[draw(random) % cars.size()];
    return car.def != nullptr ? car.def->make : 0;
}

// 0x004A9B80, 0x004A9BF0: the class of the crew's first car of the region / make, -1 without one.
int classOfRegion(const Career& career, const Crew& crew, int region) {
    for (const Car& car : carsOf(career, crew)) {
        if (car.def != nullptr && car.def->region == region) return carClass(car);
    }
    return -1;
}

int classOfMake(const Career& career, const Crew& crew, int make) {
    for (const Car& car : carsOf(career, crew)) {
        if (car.def != nullptr && car.def->make == make) return carClass(car);
    }
    return -1;
}

// 0x004A93C0: whether a crew comes to the event.
bool attends(const Career& career, const Crew& crew, const CalendarEvent& e, CareerContext& context) {
    if (crew.id == 0) return false;
    if (crew.id == 3 && e.type == EventKind::Showoff) return true;
    const int cls = e.carClass;
    if (countByRegion(career, crew, e.region, cls) == 0 || countByMake(career, crew, e.make, cls) == 0 ||
        countByModel(career, crew, e.model, cls) == 0 || countByClass(career, crew, cls) == 0) {
        return false;
    }
    if (e.extreme != 0 && countPrototypeCars(career, crew, cls, context.making.catalogue) == 0) return false;
    const bool invited = crew.willAttend[static_cast<size_t>(e.crew)] != 0;
    if (e.asksPrototype != 0) return invited;
    return invited && fraction(context.making.random) > 0.25f;
}

// 0x004B7D80: whether the player may enter the event, and why (6 when nothing is missing).
bool eligible(const Career& career, const CalendarEvent& e, int& reason, const PartCatalogue& catalogue) {
    const Crew& leader = career.crews[static_cast<size_t>(e.crew)];
    const Crew& player = career.crews[0];
    reason = 6;
    if (e.done != 0) {
        reason = 5;
        return false;
    }
    if (career.garage.empty() && e.type == EventKind::Showoff) {
        reason = 2;
        return false;
    }
    const int stage0 = leader.stage[0];
    if (stage0 == 0 && e.challenge == 0 && (e.type != EventKind::Showoff || e.crew > 4)) {
        reason = 3;
        return false;
    }
    const int team = e.round.teamSize;
    if (career.profile.memberCount < team) {
        reason = 7;
        return true;
    }
    if (career.profile.money < e.entranceFee) {
        reason = 4;
        return true;
    }
    if (leader.stage[1] == 0 && e.challenge == 0) {
        if (stage0 != 0 && e.type != EventKind::Showoff) reason = 0;
        return true;
    }
    const int cls = e.carClass;
    if (countByRegion(career, player, e.region, cls) < team) reason = 1;
    if (countByMake(career, player, e.make, cls) < team) reason = 1;
    if (countByModel(career, player, e.model, cls) < team) reason = 1;
    if (e.extreme != 0 && countPrototypeCars(career, player, cls, catalogue) < team) reason = 1;
    if (countByClass(career, player, cls) < team) reason = 1;
    return true;
}

bool pending(const CalendarEvent& e) {
    return e.used != 0 && e.done == 0;
}

// 0x004A65B0
int pendingEvents(const Calendar& calendar) {
    int n = 0;
    for (const CalendarEvent& e : calendar.events) n += pending(e) ? 1 : 0;
    return n;
}

// 0x004A6B90
int pendingCruises(const Calendar& calendar) {
    int n = 0;
    for (const CalendarEvent& e : calendar.events) n += pending(e) && e.type == EventKind::Showoff ? 1 : 0;
    return n;
}

// 0x004A6D00
int pendingSpecials(const Calendar& calendar) {
    int n = 0;
    for (const CalendarEvent& e : calendar.events) n += pending(e) && e.asksPrototype != 0 ? 1 : 0;
    return n;
}

// 0x004A6C50: waiting events the player may not enter.
int pendingClosed(const Career& career, const PartCatalogue& catalogue) {
    int n = 0;
    for (const CalendarEvent& e : career.calendar.events) {
        int reason = 0;
        if (pending(e) && !eligible(career, e, reason, catalogue)) ++n;
    }
    return n;
}

// 0x004A6CA0: waiting events the player may enter that lack cars, members or the crew's step (reasons 0, 1, 7).
int pendingWanting(const Career& career, const PartCatalogue& catalogue) {
    int n = 0;
    for (const CalendarEvent& e : career.calendar.events) {
        int reason = 0;
        if (pending(e) && eligible(career, e, reason, catalogue) && (reason == 0 || reason == 1 || reason == 7)) ++n;
    }
    return n;
}

// 0x004A6450: the first free slot of the calendar takes the event.
void addEvent(Calendar& calendar, const CalendarEvent& e) {
    for (CalendarEvent& slot : calendar.events) {
        if (slot.used == 0) {
            slot = e;
            slot.used = 1;
            return;
        }
    }
}

// 0x004A64A0
bool eventOn(const Calendar& calendar, int month, int day) {
    for (const CalendarEvent& e : calendar.events) {
        if (e.used != 0 && e.dayOfMonth == day && e.month == month) return true;
    }
    return false;
}

// 0x004A66A0: the latest day with an event.
void latestDay(const Calendar& calendar, int& month, int& day) {
    month = 0;
    day = 0;
    for (const CalendarEvent& e : calendar.events) {
        if (e.used == 0) continue;
        if (month < e.month) {
            day = e.dayOfMonth;
            month = e.month;
        }
        if (e.month == month && day < e.dayOfMonth) {
            day = e.dayOfMonth;
            month = e.month;
        }
    }
}

// 0x004B8380: a garage car of a random class whose model has a part table value of 2.
const Car* pickSpecialCar(const Career& career, CrtRandom& random) {
    const std::vector<Car>& garage = career.garage;
    if (garage.empty()) return nullptr;
    for (int outer = 0; outer < 20; ++outer) {
        const int cls = static_cast<int>(draw(random) & 7);
        for (int inner = 0; inner < 50; ++inner) {
            const Car& car = garage[draw(random) % garage.size()];
            if (carClass(car) != cls || car.def == nullptr) continue;
            for (int8_t v : career.profile.partsA[static_cast<size_t>(car.def->id)]) {
                if (v == 2) return &car;
            }
        }
    }
    return nullptr;
}

// 0x00498F40: the suppliers (the parts' byte +0x2A) of the prototype parts of a kind, each once.
std::vector<int> prototypeSuppliers(const PartCatalogue& catalogue, size_t kind) {
    std::vector<int> suppliers;
    for (const CarPart& part : catalogue.lists[kind].parts) {
        const int supplier = static_cast<int8_t>(part.brand);
        bool seen = false;
        for (int s : suppliers) seen = seen || s == supplier;
        if (!seen && part.level == 3) suppliers.push_back(supplier);
    }
    return suppliers;
}

// 0x004A2C70: one event in eight (with cars and enough respect) asks for a car with prototype parts of a supplier.
void specialCarEvent(Career& career, CareerContext& context, CalendarEvent& e) {
    CrtRandom& random = context.making.random;
    e.asksPrototype = 0;
    if (career.garage.empty() || (draw(random) & 7) != 1) return;
    const int level = career.profile.respectLevel;
    if (level < 6) return;
    if (pendingSpecials(career.calendar) > (level < 13 ? 3 : 5)) return;
    const Car* car = pickSpecialCar(career, random);
    if (car == nullptr) return;
    const PartCatalogue& catalogue = context.making.catalogue;
    e.prototypeKind = static_cast<int32_t>(draw(random) % 10);
    int tries = 0;
    while (prototypeSuppliers(catalogue, static_cast<size_t>(e.prototypeKind)).empty() ||
           career.profile.partsA[static_cast<size_t>(car->def->id)][static_cast<size_t>(e.prototypeKind)] != 2) {
        ++tries;
        e.prototypeKind = static_cast<int32_t>(draw(random) % 10);
        if (tries > 50) return;
    }
    e.asksPrototype = 1;
    const std::vector<int> suppliers = prototypeSuppliers(catalogue, static_cast<size_t>(e.prototypeKind));
    e.prototypeSupplier = suppliers[draw(random) % suppliers.size()];
    e.carClass = carClass(*car);
}

// The prize factor of a car class (0x004A393C: the table 0x0071875C by the event's +0x6C). Stock problem: class 8 (a
// cruise) reads past the 8 factors, the word after them (0x0071877C, 0: nothing refers to it), so its prizes are 0.
float prizeFactor(const ExeTables& tables, int cls) {
    if (cls >= 0 && static_cast<size_t>(cls) < tables.prizeFactors.size()) {
        return tables.prizeFactors[static_cast<size_t>(cls)];
    }
    return 0.0f;
}

// 0x004A33E0: the event's round: route, laps, team size, the crews racing, the prizes.
void setupRound(Career& career, CareerContext& context, CalendarEvent& e) {
    CrtRandom& random = context.making.random;
    EventRound& r = e.round;
    const Crew& leader = career.crews[static_cast<size_t>(e.crew)];
    r.ended = 0;
    r.used = 1;
    r.prototypePrize = 0;
    r.type = e.type;
    float km = r.km;
    bool setKm = false;
    auto teamSize = [&](bool checkProfile) -> int8_t {
        if (e.pinkSlip != 0 || e.hostedByPlayer != 0) return 1;
        if (checkProfile && career.profile.introduction != 0) return 1;
        return static_cast<int8_t>(draw(random) % 3 + 1);
    };
    switch (e.type) {
    case EventKind::Challenge: {
        // A challenge of the crew (the event's +0x06; stock problem: -1 reads the fields before the table).
        const auto at = static_cast<size_t>(0x2DC + e.challengeIndex * 0x1C);
        r.teamSize = 1;
        r.mostCrews = 1;
        r.laps = 1;
        const auto type = static_cast<ChallengeType>(raw::get<int32_t>(leader, at));
        if (type >= ChallengeType::LapTime && type <= ChallengeType::Speed) {
            const auto slot = raw::get<int32_t>(leader, at + 0x10);
            r.route = raw::get<int32_t>(leader, static_cast<size_t>(0x1C4 + slot * 4));
            r.routeIndex = slot;
            r.km = raw::get<float>(leader, static_cast<size_t>(0x1E4 + slot * 4));
        } else if (type == ChallengeType::Showoff) {
            r.laps = 0x7F;
            r.route = leader.cruiseRoute;
            r.routeIndex = 0;
            r.km = 1.0f;
        }
        break;
    }
    case EventKind::Showoff: {
        r.teamSize = 1;
        r.mostCrews = 1;
        r.laps = 0x7F;
        const uint32_t i = pick(random, leader.cruiseCount);
        r.routeIndex = static_cast<int32_t>(i);
        r.route = raw::get<int32_t>(leader, 0x240 + i * 4);
        r.km = 1.0f;
        break;
    }
    case EventKind::PointToPoint: {
        r.teamSize = teamSize(false);
        r.mostCrews = 6;
        r.laps = 1;
        const uint32_t i = pick(random, leader.p2pCount);
        r.routeIndex = static_cast<int32_t>(i);
        r.route = raw::get<int32_t>(leader, 0x22C + i * 4);
        km = static_cast<float>(r.laps) * raw::get<float>(leader, 0x234 + i * 4);
        setKm = true;
        break;
    }
    case EventKind::Circuit: {
        r.teamSize = teamSize(true);
        r.mostCrews = 6;
        const uint32_t i = pick(random, leader.circuitCount);
        r.routeIndex = static_cast<int32_t>(i);
        r.route = raw::get<int32_t>(leader, 0x1C4 + i * 4);
        const float routeKm = raw::get<float>(leader, 0x1E4 + i * 4);
        float laps;
        if (e.extreme != 0) {
            laps = (draw(random) & 1) ? 15.0f / routeKm : 10.0f / routeKm;
        } else {
            laps = static_cast<float>(16 - e.carClass) / routeKm;
        }
        r.laps = static_cast<int8_t>(nearest(laps));
        if (r.laps < 2) r.laps = 2;
        km = static_cast<float>(r.laps) * routeKm;
        setKm = true;
        break;
    }
    case EventKind::Sprint: {
        r.teamSize = 1;
        r.mostCrews = 4;
        r.laps = 1;
        const uint32_t i = pick(random, leader.sprintCount);
        r.routeIndex = static_cast<int32_t>(i);
        r.route = raw::get<int32_t>(leader, 0x24C + i * 4);
        km = static_cast<float>(r.laps) * raw::get<float>(leader, 0x250 + i * 4);
        setKm = true;
        break;
    }
        default:
            break;
    }
    if (setKm) r.km = km;
    if (r.teamSize > 3) r.teamSize = 3;
    if (r.teamSize > career.profile.memberCount && fraction(random) < 0.5f) {
        r.teamSize = static_cast<int8_t>(career.profile.memberCount);
    }
    if (fraction(random) < 0.25f) r.teamSize = 1;
    r.opponents = 0;
    r.teams = r.teamSize > 1 ? 1 : 0;
    r.drivers = 0;
    r.crews.fill(0);
    for (int tries = 0; tries < 1000; ++tries) {
        const uint32_t k = draw(random) % 9;
        if (e.attend[k] == 0 || r.crews[k] != 0) continue;
        if (r.drivers >= 6 || r.opponents >= r.mostCrews) break;
        r.drivers = static_cast<int8_t>(r.drivers + r.teamSize);
        ++r.opponents;
        r.crews[k] = 1;
    }
    auto money = [](int32_t v) { return static_cast<int32_t>(static_cast<float>(v)); };
    if (e.pinkSlip == 0 && e.challenge == 0) {
        switch (r.opponents) {
            case 2:
            case 3:
                r.prize = {money(leader.prizeMoney[0]), 0, 0};
                break;
            case 4:
                r.prize = {money(leader.prizeMoney[0]), money(leader.prizeMoney[2]), 0};
                break;
            case 5:
            case 6:
                r.prize = {money(leader.prizeMoney[0]), money(leader.prizeMoney[1]), money(leader.prizeMoney[2])};
                break;
            default:
                break;
        }
    } else {
        r.prize = {0, 0, 0};
    }
    const float factor = prizeFactor(context.tables, e.carClass);
    for (int32_t& p : r.prize) {
        p = static_cast<int32_t>(static_cast<float>(p) * factor);
        if (e.asksPrototype != 0) p *= 2;
        if (e.extreme != 0) p *= 3;
    }
    e.prizeTotal += r.prize[0] + r.prize[1] + r.prize[2];
    ++e.roundCount;
}

// 0x004A4410
void setupConditions(CalendarEvent& e, CrtRandom& random) {
    if (e.pinkSlip != 0) {
        e.timeOfDay = (draw(random) & 1) != 0 ? 4 : 3;
        e.weather = draw(random) % 5 != 2 ? 1 : 2;
        return;
    }
    if (e.type != EventKind::Showoff && e.type != EventKind::Challenge) {
        e.weather = (draw(random) & 1) != 0 ? 1 : 2;
        static const int32_t kTimes[] = {1, 2, 3, 4};
        e.timeOfDay = kTimes[draw(random) & 3];
        return;
    }
    e.weather = 1;
    e.timeOfDay = static_cast<int32_t>(draw(random) % 3) + 1;
}

}  // namespace

void eventConditions(CalendarEvent& event, CrtRandom& random) {
    setupConditions(event, random);
}

bool hostAttends(const Career& career, const Crew& crew, const CalendarEvent& event, const PartCatalogue& catalogue) {
    const CareerProfile& p = career.profile;
    if (crew.id == 0) return false;
    if (crew.id < 3) return true;
    const auto at = static_cast<size_t>(event.location);
    if (crew.stage[1] == 0 || at >= crew.willAttend.size() || crew.willAttend[at] == 0) return false;
    if (p.lastHostedType == static_cast<int32_t>(event.type) && crew.id > 4) return false;
    if (p.lastHostedLocation == event.location && crew.id > 4) return false;
    const int cls = event.carClass;
    if (countByRegion(career, crew, event.region, cls) == 0 || countByMake(career, crew, event.make, cls) == 0 ||
        countByModel(career, crew, event.model, cls) == 0 || countByClass(career, crew, cls) == 0) {
        return false;
    }
    return event.extreme == 0 || countPrototypeCars(career, crew, cls, catalogue) != 0;
}

int hostCars(const Career& career, const Crew& crew, int region, int make, int cls, EventKind type) {
    int n = 0;
    for (const Car& car : carsOf(career, crew)) {
        if (car.def == nullptr) continue;
        if ((region != car.def->region && region != 0) || (make != car.def->make && make != 0)) continue;
        if (carClass(car) != cls && cls != 8) continue;
        if (type == EventKind::Sprint && car.def->id == 0x28) continue;
        ++n;
    }
    return n;
}

// The class of a rating (0x0048E9E0 of a car's +0x88, 0x00447490): the rating cut toward zero.
int ratingClass(float rating) {
    const int r = x87::chop(rating);
    if (r > 799) return 0;
    if (r > 699) return 1;
    if (r > 599) return 2;
    if (r > 499) return 3;
    if (r > 399) return 4;
    if (r > 299) return 5;
    return r < 200 ? 7 : 6;
}

int carClass(const Car& car) {
    return ratingClass(Car::asFloat(car.rating));
}

bool mayEnter(const Career& career, const CalendarEvent& event, int& reason, const PartCatalogue& catalogue) {
    return eligible(career, event, reason, catalogue);
}

int monthDays(int month) {
    static constexpr int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    // Stock problem: a negative month reads before the table (0x00718EDC); the first month here.
    const int m = month % 12;
    return kDays[m < 0 ? 0 : m];
}

CalendarEvent* eventAt(Calendar& calendar, int day, int month) {
    for (CalendarEvent& e : calendar.events) {
        if (e.used != 0 && e.dayOfMonth == day && e.month == month) return &e;
    }
    return nullptr;
}

void addDays(int& day, int& month, int days) {
    day += days;
    while (day >= monthDays(month)) {
        day -= monthDays(month);
        ++month;
    }
}

// 0x004AA4C0: each crewmate: one who joined counts a day (+0xBC); else, with fewer than four crew members and the
// respect of the crew it would join from (+0x8C) over its threshold (+0x88), it counts a day of wanting (+0xC0); past
// 30 + 5 x its number of them it calls (0x004B63B0: kind 1, its name and number, sample 0x24, type 0, "Let driver
// join crew?" with YES and NO) and every crewmate's +0xC0 is 0.
void crewmatesDay(Career& career) {
    for (size_t i = 0; i < career.crewmates.size(); ++i) {
        Crewmate& mate = career.crewmates[i];
        if (mate.joined != 0) {
            ++mate.daysIdle;
            continue;
        }
        if (career.profile.memberCount >= 4) continue;
        const auto crew = static_cast<size_t>(mate.joinCrew);
        if (crew >= career.crews.size() || career.crews[crew].respect <= mate.joinThreshold) continue;
        ++mate.daysWanting;
        if (mate.daysWanting <= static_cast<int32_t>(i) * 5 + 30) continue;
        queuePhoneMessage(career.phone, PhoneCaller::Crewmate,
                          std::string(mate.name.data(), strnlen(mate.name.data(), mate.name.size())), mate.number, 0x24,
                          mate.number, CallType::LetJoin, phonetext::kLetJoin, phonetext::kYes, phonetext::kNo,
                          kPhoneNone, kPhoneNone);
        for (Crewmate& m : career.crewmates) m.daysWanting = 0;
    }
}

void nextDay(Career& career, CareerContext& context) {
    Calendar& calendar = career.calendar;
    ++calendar.daysPlayed;
    int day = calendar.dayOfMonth;
    int month = calendar.month;
    if (CalendarEvent* e = eventAt(calendar, day, month); e != nullptr) {
        e->done = 1;
        e->used = 0;
    }
    const int before = month;
    addDays(day, month, 1);
    calendar.dayOfMonth = day;
    calendar.month = month;
    calendar.monthCopy = month;
    if (before != month) stockDealer(career, context);
    crewmatesDay(career);
    fillCalendar(career, context);
}

const CalendarEvent* importantBefore(const Calendar& calendar, int day, int month) {
    for (const CalendarEvent& e : calendar.events) {
        if (e.used == 0 || e.done != 0) continue;
        if (e.month > month || (e.month == month && e.dayOfMonth >= day)) continue;
        if (e.challenge != 0 || e.pinkSlip != 0 || e.hostedByPlayer != 0) return &e;
    }
    return nullptr;
}

bool anyImportant(const Calendar& calendar) {
    for (const CalendarEvent& e : calendar.events) {
        if (e.used == 0 || e.done != 0) continue;
        if (e.challenge != 0 || e.pinkSlip != 0 || e.hostedByPlayer != 0) return true;
    }
    return false;
}

void nextFreeDay(const Calendar& calendar, int& day, int& month) {
    day = calendar.dayOfMonth;
    month = calendar.month;
    bool taken = true;
    while (taken) {
        ++day;
        if (day >= monthDays(month)) {
            day = 0;
            ++month;
        }
        taken = false;
        for (const CalendarEvent& e : calendar.events) {
            if (e.used != 0 && e.month == month && e.dayOfMonth == day) {
                taken = true;
                break;
            }
        }
    }
}

int openEvents(const Career& career, const PartCatalogue& catalogue) {
    int n = 0;
    for (const CalendarEvent& e : career.calendar.events) {
        if (e.used == 0 || e.done != 0 || e.type == EventKind::Showoff) continue;
        int reason = 0;
        if (!eligible(career, e, reason, catalogue)) continue;
        if (reason != 0 && reason != 1 && reason != 7) ++n;
    }
    return n;
}

void topUpCalendar(Career& career, CareerContext& context) {
    const Calendar& calendar = career.calendar;
    const PartCatalogue& catalogue = context.making.catalogue;
    if (career.garage.empty() || openEvents(career, catalogue) >= 6) return;
    int tries = 0;
    int made = 0;
    for (;;) {
        int month = calendar.month;
        int day = calendar.dayOfMonth;
        addDays(day, month, 2);
        addDays(day, month, static_cast<int>(static_cast<uint32_t>(draw(context.making.random)) % 20u));
        const int tried = tries++;
        if (tried <= 100 && eventOn(calendar, month, day)) continue;
        createEvent(career, context, month, day, 1);
        if (made++ > 100 || openEvents(career, catalogue) >= 6) return;
    }
}

float careerCompletion(const Career& career) {
    float total = 0.0f;
    bool all = true;
    for (size_t k = 1; k < 9; ++k) {
        const Crew& crew = career.crews[k];
        const auto respect = static_cast<float>(crew.respect);
        const float v = 1000.0f < respect ? 1000.0f : respect;
        total = v + total;
        if (v < 1000.0f) all = false;
        for (const CrewChallenge& c : crew.challenges) {
            if (c.done != 0) {
                total = total + 40.0f;
            } else {
                all = false;
            }
        }
    }
    total = total * 0.00011160714f;
    if (1.0f < total) {
        total = 1.0f;
    } else if (0.0f > total) {
        total = 0.0f;
    }
    return all ? 1.0f : total;
}

int respectedMost(const Crew& crew) {
    static constexpr size_t kFactors[] = {0, 1, 2, 4, 5, 6, 7, 8};
    float most = 0.0f;
    int index = 0;
    for (const size_t k : kFactors) {
        if (most < crew.respectFactor[k]) {
            most = crew.respectFactor[k];
            index = static_cast<int>(k);
        }
    }
    return most < crew.respectFactor[9] ? 9 : index;
}


void firstEvent(Career& career, CareerContext& context) {
    career.profile.introBillPaid = 0;
    career.profile.introduction = 1;
    career.stats = CareerStats{};
    CalendarEvent e{};
    e.challengeIndex = -1;
    e.done = 0;
    e.roundCount = 0;
    e.prizeTotal = 5000;
    e.dayOfMonth = 0;
    e.month = 0;
    e.crew = 1;
    e.location = 1;
    e.attend[1] = 1;
    e.attend[0] = 1;
    e.entranceFee = 0;
    e.region = 0;
    e.make = 0;
    e.model = 0;
    float best = 0.0f;
    for (const Car& car : career.garage) {
        if (best < Car::asFloat(car.rating)) best = Car::asFloat(car.rating);
    }
    e.carClass = ratingClass(best);
    e.type = EventKind::Circuit;
    setupRound(career, context, e);
    e.round.laps = 2;
    e.round.prize = {5000, 0, 0};
    e.round.route = 0x31;
    e.round.routeIndex = 0;
    e.roundIndex = 0;
    e.timeOfDay = 4;
    e.weather = 1;
    addEvent(career.calendar, e);
}

namespace {

// 0x004A9A70: the best rating of the crew's list of cars: the garage for the player's crew (+0x78 0), else every
// opponent's car.
float bestRating(const Career& career, const Crew& crew) {
    float best = 0.0f;
    for (const Car& car : crew.id == 0 ? career.garage : career.opponentCars) {
        if (best < Car::asFloat(car.rating)) best = Car::asFloat(car.rating);
    }
    return best;
}

}  // namespace

void phoneEvent(Career& career, CareerContext& context, int crew, int month, int day, bool host) {
    CrtRandom& random = context.making.random;
    if (eventOn(career.calendar, month, day) || crew < 0 || crew > 8) return;
    Crew& c = career.crews[static_cast<size_t>(crew)];
    CalendarEvent e{};
    e.challengeIndex = -1;
    e.dayOfMonth = static_cast<int8_t>(day);
    e.month = month;
    e.crew = static_cast<int8_t>(crew);
    e.location = static_cast<int8_t>(crew);
    e.attend[static_cast<size_t>(crew)] = 1;
    e.attend[0] = 1;
    if (host) {
        e.hostedByPlayer = 1;
        c.hostAccepted = 1;
    } else {
        e.pinkSlip = 1;
    }
    e.carClass = ratingClass(bestRating(career, career.crews[0]));
    e.type = EventKind::Circuit;
    if (fraction(random) < c.probP2P) e.type = EventKind::PointToPoint;
    if (fraction(random) < c.probSprint) e.type = EventKind::Sprint;
    setupRound(career, context, e);
    e.roundIndex = 0;
    setupConditions(e, random);
    addEvent(career.calendar, e);
}

void challengeEvent(Career& career, CareerContext& context, int crew, int month, int day, int challenge) {
    if (eventOn(career.calendar, month, day) || crew < 0 || crew > 8 || challenge < 0 || challenge > 2) return;
    const CrewChallenge& ch = career.crews[static_cast<size_t>(crew)].challenges[static_cast<size_t>(challenge)];
    CalendarEvent e{};
    e.challengeIndex = static_cast<int8_t>(challenge);
    e.challenge = 1;
    e.dayOfMonth = static_cast<int8_t>(day);
    e.month = month;
    e.crew = static_cast<int8_t>(crew);
    e.location = static_cast<int8_t>(crew);
    e.attend[static_cast<size_t>(crew)] = 1;
    e.attend[0] = 1;
    e.model = ch.car;
    e.carClass = 8;
    e.type = EventKind::Challenge;
    setupRound(career, context, e);
    e.round.prize = {ch.done == 0 ? ch.prize : 0, 0, 0};
    e.roundIndex = 0;
    setupConditions(e, context.making.random);
    addEvent(career.calendar, e);
}

int playerCarsOfModel(const Career& career, int32_t model) {
    if (model == 0) return 100;
    int count = 0;
    for (const Car& car : career.garage) {
        if (car.def != nullptr && car.def->id == model) ++count;
    }
    return count;
}

void createEvent(Career& career, CareerContext& context, int month, int day, int option) {
    CrtRandom& random = context.making.random;
    const PartCatalogue& catalogue = context.making.catalogue;
    Calendar& calendar = career.calendar;
    CalendarEvent e{};
    e.challengeIndex = -1;
    if (eventOn(calendar, month, day)) return;
    e.done = 0;
    e.roundCount = 0;
    e.hostedByPlayer = 0;
    e.prizeTotal = 0;
    e.dayOfMonth = static_cast<int8_t>(day);
    e.month = month;
    const auto crewIndex = static_cast<size_t>((draw(random) & 7) + 1);
    const Crew& leader = career.crews[crewIndex];
    e.crew = static_cast<int8_t>(crewIndex);
    e.location = static_cast<int8_t>(crewIndex);
    specialCarEvent(career, context, e);
    if (e.asksPrototype == 0 && countPrototypeCars(career, leader, 8, catalogue) != 0 && fraction(random) > 0.8f) {
        e.extreme = 1;
    }
    EventKind type = EventKind::Circuit;
    e.type = EventKind::Circuit;
    if (fraction(random) < leader.probP2P) {
        type = EventKind::PointToPoint;
        e.type = EventKind::PointToPoint;
    }
    if (fraction(random) < leader.probCruising && e.asksPrototype == 0 && e.extreme == 0) {
        type = EventKind::Showoff;
        e.type = EventKind::Showoff;
    }
    bool chooseClass = true;
    if (fraction(random) < leader.probSprint) {
        e.type = EventKind::Sprint;
    } else if (type == EventKind::Showoff) {
        e.asksPrototype = 0;
        e.carClass = 8;
        chooseClass = false;
    }
    if (chooseClass && e.asksPrototype == 0) {
        const bool player = (draw(random) & 3) != 0 && crewIndex < 4;
        e.carClass = pickClass(career, player ? career.crews[0] : leader, context);
    }

    int attending = 0;
    for (size_t k = 0; k < 9; ++k) {
        e.attend[k] = attends(career, career.crews[k], e, context) ? 1 : 0;
        attending += e.attend[k];
    }
    e.attend[crewIndex] = 1;
    if (attending < 3) return;

    e.entranceFee = career.crews[crewIndex].entranceFee;
    if (e.asksPrototype != 0) e.entranceFee *= 2;
    if (e.extreme != 0) e.entranceFee *= 3;
    if (e.type == EventKind::Showoff) e.entranceFee = 0;
    e.region = 0;
    e.make = 0;
    e.model = 0;
    if (e.type != EventKind::Showoff) {
        if (e.extreme == 0 && e.asksPrototype == 0) {
            if (fraction(random) < leader.probTerritory) {
                e.region = randomRegion(career, leader, random);
                e.carClass = classOfRegion(career, leader, e.region);
            } else if (fraction(random) < leader.probMake) {
                e.make = randomMake(career, leader, random);
                e.carClass = classOfMake(career, leader, e.make);
            }
        }
        if (e.carClass == -1) {
            e.carClass = 7;
            e.region = 0;
            e.make = 0;
        }
    }
    setupRound(career, context, e);
    if (e.asksPrototype != 0) e.round.prototypePrize = 1;
    e.roundIndex = 0;
    setupConditions(e, random);

    if (career.garage.empty()) {
        addEvent(calendar, e);
        return;
    }
    if (e.type == EventKind::Showoff && pendingCruises(calendar) > 1) return;
    int reason = 0;
    if (!eligible(career, e, reason, catalogue)) {
        if (career.profile.respectLevel < 6 || option != 0 || pendingClosed(career, catalogue) >= 1) return;
    }
    if (career.profile.introduction == 0) {
        const bool fewWanting = option == 0 && career.profile.respectLevel >= 4 && pendingWanting(career, catalogue) < 2;
        if (!fewWanting && (reason == 0 || reason == 1 || reason == 7)) return;
    }
    addEvent(calendar, e);
}

void fillCalendar(Career& career, CareerContext& context) {
    CrtRandom& random = context.making.random;
    static const int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    while (pendingEvents(career.calendar) < 20) {
        int month = 0;
        int day = 0;
        latestDay(career.calendar, month, day);
        day = day + static_cast<int>(draw(random) & 3) + 1;
        while (day >= kDays[month % 12]) {
            day -= kDays[month % 12];
            ++month;
        }
        createEvent(career, context, month, day, 0);
    }
}

}  // namespace oj
