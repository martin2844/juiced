#pragma once
// The race of the career's current event (docs/spec/08-career.md, "The race of an event"): the racers and their
// cars, the odds the bookmaker gives, the bets on the rival crew and the skills of the AI drivers; the way into the
// career and the start of a race.

#include "Core/Career/Career.h"
#include "Core/Career/CareerInit.h"
#include "Core/Race/RaceKinds.h"

#include <array>
#include <cstdint>
#include <string>

namespace oj {

struct RaceCar;

// A fraction the bookmaker offers (0x0C bytes at 0x00718D40, 33): against / stake, and that as a float.
struct OddsFraction {
    int32_t against = 0;
    int32_t stake = 0;
    float value = 0.0f;
};

// The table of fractions as the session's constructor leaves it (0x004B8450): the values worked out, sorted by
// value, smallest first.
const std::array<OddsFraction, 33>& oddsFractions();

// 0x0048CFE0: the value of a car: the CarDef's VALUE, (level + 3) x 500 for each body part chosen (levels 1 .. 126),
// the COST of each catalogue part, at least 199.
int carValue(const Car& car, const PartCatalogue& catalogue);

// 0x004A64E0: the calendar's event of today (day of the month and month); nullptr when none.
CalendarEvent* currentEvent(Calendar& calendar);

// The round of an event that its round index (+0x55) names.
const EventRound* roundOf(const Career& career, const CalendarEvent& event);

// 0x004B1870: the bets on a race against `crew`: the crew's money (2000 for the first event), seven amounts from
// 0.6 of it below to 0.6 above in steps of 0.2 of it, cut to hundreds, with their weights (0x007189F8).
void setupBets(Career& career, int crew);

// 0x004B1A00: after a bet too low the three highest amounts are left.
void keepHighBets(CareerBets& bets);

// 0x004B8570: the racers of the current event's round when the session needs a setup, their odds and the AI drivers;
// then the bet's racer.
void setupRace(Career& career, CareerContext& context);

// 0x004B9150 (an arcade or custom race): the session of the race's players (+0x58) and opponents (+0x5C): racer i
// placed i, numbered i, of crew 0 (a player's, the driver) or of crew i, member 0, no car, no name; the members and
// cars of the six racers -1 first; the odds cleared (stake 1), the crews' counts 0, the order -1, the first
// finisher none, the bet none; then the drivers (0x004BDCC0).
void setupCustomSession(Career& career, int32_t players, int32_t opponents, CrtRandom& random);

// 0x004B9670, the garage's test track: one racer, no event nor round; every racer's member and car -1, the player's
// crew's race cars (+0x33C ..) none; the racer the player's own (crew 0, number 0, member 0, the garage, the dealer's
// chosen garage car +0x27DC, the leader's short name); the odds cleared (stake 1), the crews' counts and the order
// (+0x340 .., 0 here) cleared, +0x368 and +0x36C -1; the profile's +0x5F0 1, +0x514 and the first race car (+0x600)
// the car, its member 0, the race cars chosen; the drivers (0x004BDCC0); the bet none.
void setupTestTrackSession(Career& career, CrtRandom& random);

// A player of a network race as the session reads it (one in the race and not gone): this machine's or another's,
// its index on its machine, its grid place and its name.
struct NetEntry {
    bool ours = false;         // the player's peer -1
    int32_t localIndex = 0;    // +0x04
    int32_t gridPlace = 0;     // +0xC4
    std::wstring name;         // +0x14
};
// 0x004B8C90: the session of a network race's players: the six racers' members and cars -1 first; racer n placed n,
// numbered by its grid place, the driver for our first player, of crew 0 (ours) or its index on its machine + 1
// (Stock problem: the first players of two machines share crew 1), member and car 0, no car list; then the rest as
// setupCustomSession. The stock names every racer "Online Test !!" and its HUD reads the cars' names (the players');
// the port's HUD reads the racers', which take the players' names here.
void setupNetSession(Career& career, const std::vector<NetEntry>& entries, CrtRandom& random);

// An arcade race's entry as the session reads it: the player's car, the opponent's number.
struct ArcadeEntry {
    bool player = false;  // +0x04
    int32_t number = 0;   // +0x08
};
// 0x004B93C0: the session of an arcade race's entries: racer i placed and numbered i, of crew 0 (the player's, the
// driver) or of the entry's number, member 1 for an opponent numbered 0 (else 0), no car, no name; the rest as
// setupCustomSession.
void setupArcadeSession(Career& career, const std::vector<ArcadeEntry>& entries, CrtRandom& random);

// 0x004B9920: the odds of the crews of the race.
void setupOdds(Career& career);

// 0x004B1780: the racer of the crew bet with (its leader); 0 when none.
uint32_t betRacer(const Career& career);

// 0x004B1730 (crew = the bets' crew) and 0x004B17C0 (crew 0): the car of the crew leader's racer; 0 when none.
int32_t leaderCar(const Career& career, int32_t crew);

// 0x00434D90: a copy of a car record without its grid (+0xB4), which is made anew (0x005562A0): the kept damage
// is not copied.
Car copyCar(const Car& car);

// 0x004B1A80 (the post race betting window's leave with a pink slip bet): won (the bets' +0x04), while the garage
// has fewer than 55 cars, a copy of the car of the leader of the crew bet with joins the garage, the crew's +0x2D0
// grows, and the profile knows the car's parts: for each kind its catalogue part's level (the part of the car's
// +0x0E + kind, the list's first past its end) in the model's +0x50 levels, and its body parts (+0x04 + category) in
// the model's +0x262 levels, each kept when higher (category 3 then 100). Lost: the garage's last car (the stake,
// put back there before the race) goes, the crew's +0x2D0 and the profile's +0x4E0 cleared.
void pinkSlipCar(Career& career, const PartCatalogue& catalogue);

// 0x004A49A0: an online pink slip's stakes: the career's +0x88 a new list holding a copy of our car (0x00434D90),
// +0x8C one holding a copy of the rival's (the lists there before not freed); with the statistics ([0x0075F030])
// each of our session players of local index 0 (its user referenced, 0x004EA100) takes the race (0x004EC910) and
// they are sent (vt+0x20) when the last took; +0x90 1, +0xAC, +0x94 and +0x9C 0.
void onlineStakes(Career& career, const Car& own, const Car& rival);

class DynamicsStore;

// 0x004A4C60: an online pink slip's end, with its stakes set (+0x90): unless `tie`, won: a copy of the rival's car
// (+0x8C's first, 0x00434D90; its grid made when none, 0x005562A0), rated (0x0048E8A0), at the garage's end (no
// check of the 55 cars), its value (0x0048CFE0, for the statistics) and the profile knowing its catalogue parts (for
// each kind the part's level, the list's first past its end, in the model's +0x50 levels, kept when higher; not its
// body parts, unlike 0x004B1A80); lost: the garage's last car taken out (0x00489CD0). With the statistics
// ([0x0075F030], not a tie) our first session player of local index 0 (its user referenced, 0x004EA100) takes the
// result (0x004ED090 with won and the value, 0 lost) and they are sent (vt+0x20). Then the lists freed and +0x88,
// +0x8C, +0x90 and +0xAC 0.
void onlineSettle(Career& career, bool won, bool tie, const PartCatalogue& catalogue, DynamicsStore* dynamics);

// 0x0048CE20: the drive of a car's model: 0 front, 1 rear, 2 all wheels (the jump table; 0 past it).
int driveOf(const Car& car, const ExeTables& tables);

// The cars a crew races with: the garage for crew 0, else the opponents' cars.
const std::vector<Car>& carsOf(const Career& career, int32_t crew);

// The player pays `amount` (a negative amount is money received): the career's money counters, and the profile's
// money, not below 0 (0x004B1A40 and the same lines in the betting window).
void pay(Career& career, int32_t amount);

// 0x004B1A40: a bet of `amount` with the rival crew is made and paid.
void placeBet(Career& career, int32_t amount);

// A crew's respect after a change (the betting window's messages): cut to a whole number, not below 0.
void changeRespect(Crew& crew, float change);

// The way into the career from its first sight of the city (0x00435434): the career head value cleared, a new career
// over the career objects, the bets on the first event's crew (1), the player's first racer (member 0, garage car 0)
// and the race session set up.
void enterCareer(Career& career, CareerContext& context, const std::string& driverName);

// What a race is started with (the game object's +0x6C, the career's race; 0x004A3CC0 fills it).
struct RaceSetup {
    RaceMode mode = RaceMode::Arcade;    // +0x00
    RaceType type = RaceType::TimedLap;  // +0x04: by the round's type or the challenge's (0x004A3CC0)
    int32_t heat = 0;            // +0x08: a sprint's heat (from 0)
    int32_t heats = 0;           // +0x0C: a sprint's heats (3; 1 online)
    std::string track;           // +0x10 (31 characters at most)
    std::string route;           // +0x30 (31 characters at most)
    int32_t laps = 0;            // +0x50
    int32_t gearbox = 0;         // +0x54: the settings' +0x84 (1 automatic), 0 for type 6
    int32_t players = 0;         // +0x58: the players racing (1; more in a split screen race)
    int32_t opponents = 0;       // +0x5C: the AI cars of a custom race
    float difficulty = 0.0f;     // +0x60: a custom race's (1 .. 3; the cruise's)
    int32_t timeOfDay = 0;       // +0x6C: the event's +0x7C; 1 morning, 2 noon, 3 evening, 4 night (0x00424270)
    int32_t weather = 0;         // +0x70: the event's +0x80; 2 wet
    float lapTime = 0.0f;        // +0x74: a challenge's lap time
    float speed = 0.0f;          // +0x78: a challenge's speed
    ChallengeOutcome challenge = ChallengeOutcome::None;  // +0x7C
};

// 0x004A3CC0: the race of the current event: the crews' +0x330 and the career's +0x34 cleared, the race's type,
// track, route and laps; the event's time of day (+0x7C) drawn again (rand() % 5) while it is 0 or 4 (night) and the
// round's +0x18 is above 4. automaticGearbox: the save system's +0x84 (the settings' +0x30); networkGame: the network
// game [0x0075EF8C] is there (a sprint of one heat).
void startRace(Career& career, CareerContext& context, RaceSetup& race, int32_t automaticGearbox,
               int32_t networkGame);

// 0x004BC2E0: the profile's bet share: the money bet in the race (the crews' odds +0x0C and the player's bet), as a
// percentage of the player's money.
void updateBetShare(Career& career);

// 0x004A4F90: the place of a crew in the race (0 first): the crews in the order their last driver (the round's +0x17
// drivers each) is reached along the places; -1 when a racer has no car or a car has not crossed the start line yet;
// the last place found when a place is missing ([0x007255B4], kept between calls). racerCars: the session's racers'
// cars (the racer's +0x2C).
int32_t crewPlace(Career& career, const std::vector<const RaceCar*>& racerCars, int32_t crew);

// 0x004BB2F0: none with the profile's +0x00; the garage's worth (0x0048D430 summed) over the crew's mark (+0x64, a
// float) x the factor 0 x 0.001. Applying adds it to the gain 0, keeps the worth in the profile's +0x4F4 (cut) and
// as the mark.
int32_t respectByCollection(Career& career, Crew& crew, const PartCatalogue& catalogue, bool apply);
// 0x004BB3D0: none with the profile's +0x00; the garage's best car by (8 - its class) x its worth (0x0048D430), that
// product over the crew's mark (+0x68, a float). Applying keeps the best car's worth in the profile's +0x4FC (cut) and
// the product as the mark. Then x the factor 1 and x 1/450 when it rose (else x 1/2250); applying adds it to the
// gain 1.
int32_t respectByBestCar(Career& career, Crew& crew, const PartCatalogue& catalogue, bool apply);

// 0x004BCD90 in a career race not of type 9 (the callers' raceRespectOf): the respect the player's crew has won in the
// race so far, by the crew's place (the last when lastPlace or not known; a round of type 5 by the points 0x004A5670)
// against each rival crew (place 0x004BB520, type 5 0x004BB740, type 2 the money 0x004BBB00, the standing 0x004BB9A0
// outside the profile's +0x618, the rams), with the bet's; with lastPlace or collection also the garage's best car
// (0x004BB3D0, crew 5) and its worth (0x004BB2F0, crew 6), not applied; over 10 cut to multiples of 5. finishers: the
// race's finishers so far. Kept in the career's +0x34 object's +0x30.
float raceRespect(Career& career, const std::vector<const RaceCar*>& racerCars, size_t finishers, bool lastPlace,
                  bool collection, const PartCatalogue& catalogue);

// 0x004B9CF0 (a career race not of type 9): a racer of the session finished: its crew's count (+0x31C) grows, the
// racer's +0x28 is set; in an event with +0x05 the event crew's challenge (+0x2DC, the event's +0x06) takes -1, or the
// racer's best lap unless `other`; the first such racer and its crew kept (+0x368, +0x36C); when its crew's count
// reaches the round's drivers (+0x17, with the round's +0x08 at 1; else 1) the crew goes to the crews' order (+0x340,
// +0x364 of them) with the time in its odds' +0x00.
void racerFinished(Career& career, int32_t racer, float raceTime, float bestLap, bool other);

// 0x004BA8A0 (the session's end, before 0x004B9CF0's rest in 0x004B9E20): the profile's +0x630 set when the crews'
// order's sixth place is taken (not -1 .. 0 as by "!= 0": any value but 0) while none of the first five is the
// player's crew (0); with no crew finished the odds of the nine crews cleared (+0x04, +0x0C, +0x10 0, +0x08 1).
// Outside a cruise the racers not finished (+0x28 0) are taken by their numbers (+0x24) and, with more than one,
// shuffled: rand() r1, rand() r2, (r2 % 6) + 2 x (r1 % the round's laps) times two neighbours swapped at
// rand() % (n - 1); the time t is the route's record cut toward zero (180 below 10, else plus rand() % 15) or a
// finished crew's odds time (+0x00) when greater; then each such racer of another crew, and after them each of the
// player's crew, finishes (0x004B9CF0, `other` set) at (rand() % 20) x 0.1 + its place in the list x 3.2 + t.
// random: the generator of the thread that runs it (the unload job's).
void finishSession(Career& career, RaceType raceType, float routeRecord, CrtRandom& random);

// 0x0048D430: a car's worth for the career's respect: the model's VALUE, each body part's (level + 3) x 500 (none for
// -1, 0 and over 0x7E) and each catalogue part's COST (the first part's past the list), each held to 15000; at least
// 199, cut toward zero.
int cappedCarValue(const Car& car, const PartCatalogue& catalogue);

// 0x004A8F50: more than three of the car's catalogue parts are prototype parts (level 3).
bool heavilyTuned(const Car& car, const PartCatalogue& catalogue);

// 0x004A9080: the crew's scheme on every car of its list (the garage for the crew of id 0, else every opponents'
// car): the paint (+0x1A4 .. +0x1B4), the wheels (+0x1BC to the car's +0x12), the neon on (+0x0D 1) of the colour
// (+0x1C0, the CarDefs' neon colours, the first past the list), +0x40 1, the wheels' colour (+0x1B8 of the wheel
// colours, to +0x3C), and the decal (+0x0C) none for a prototype car, else the CarDef's decal list's size less 8, 7, 1,
// 4, 3, 5, 6 for the crews of id 2 .. 8 (2 for the others), its colour (+0x38) 0. Stock problem: a wheel colour past
// the list is read after it; here the car's stays.
void applyCrewScheme(Career& career, const Crew& crew, const CarDefs& defs, const PartCatalogue& catalogue);

// The money the player's crew won in the race (the career's +0x60 object's +0x30).
float raceMoney(const Career& career);

// 0x004BACB0: the event crew's challenge (+0x2DC, the event's +0x06) met: of types 0 .. 2 the race's +0x7C at 1, of
// type 3 the money won in the race (cut) over its value; else not.
bool challengeMet(const Career& career, const RaceSetup& race);

// 0x004B8060: the player may race the event: the round's team size (+0x17) within the cars listed for the meeting (the
// profile's +0x5F0) and the members (+0x510); the event's crew past its second step (+0x58), or a cruise, or the
// event's +0x05; and the player may enter it (0x004B7D80) for a reason other than 0, 1 and 7.
bool mayRace(const Career& career, const CalendarEvent& event, const PartCatalogue& catalogue);

// 0x004B7F00: the event attended by the player's crew (+0x56); another crew's event the player may enter for a reason
// other than 0, 1 and 7 takes its fee (0x004B8200).
void attendEvent(Career& career, CalendarEvent& event, const PartCatalogue& catalogue);

// 0x004B1620: the bets cleared: +0x00, +0x04, +0x0C, +0x18 and +0x5C .. +0x80.
void clearBets(CareerBets& bets);

// 0x004B9E20 (the session's end after 0x004BA8A0; nothing for type 9): the crews' order into the stats (+0x78, by
// the event's round); a money round's respect of the money (0x004BBB00) for each crew; the best car's (0x004BB3D0,
// crew 5) and the garage's (0x004BB2F0, crew 6) respect; for each place the counts and, from the player's crew's
// place, each crew's respect of it (gains 8, 2, 4 or 5); a bet's outcome on the rival crew's money; the first crew's
// odds' winnings; the win counts; the player's racers' crewmates learn (0x004AA440); crews 1 .. 8's gain 3 from the
// rams; the event's prototype part to the player's cars; the cars' counters; the models' unlocks (0x004B8230); the
// bet's respect (0x004BB8E0); with the profile's +0x630 the cars' respect again into the career's +0x34 object's +0x30.
// The cars and parts noted in the career's +0x38 list. race: the game's +0x6C (its type and +0x7C).
void sessionResults(Career& career, CareerContext& context, const RaceSetup& race);

// 0x004A6870: the event's end: its +0x01 set; with more won on bets than lost (the career's money counters +0x0C
// over +0x08) each crew gains the bet's respect of a win (0x004BC390, as 0x004BB8E0); with the profile's +0x00 both
// message lists emptied (0x004B5CC0), else crews 1 .. 8's gains go into their respect (0x004BBD80) and the crewmates'
// learning is noted (0x004B5DE0).
void endEvent(Career& career, CalendarEvent& event, const ExeTables& tables);

// 0x004A5670: the player's crew's place by the points (a round of type 5).
int32_t raceRespectPlace(Career& career, const std::vector<const RaceCar*>& racerCars);
// 0x004A5110: the lead of the player's crew (see raceRespect).
int32_t raceRespectLead(Career& career, const std::vector<const RaceCar*>& racerCars);

}  // namespace oj
