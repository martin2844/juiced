#pragma once
// A new career (0x004A2300, docs/spec/08-career.md).

#include "Core/Career/Career.h"
#include "Core/Cars/CarCollection.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Data/ExeTables.h"
#include "Core/Base/GameFiles.h"

#include <string>

namespace oj {

// What the career draws on. The counters live as long as the game: the automatic modification's (in making) and the
// crewmate number CMSCrewmates.txt reading keeps (0x0075EF60, 0 at start).
struct CareerContext {
    const ExeTables& tables;
    CarMaking& making;
    DynamicsStore& dynamics;
    const GameFiles& files;
    int& currentCrewmate;
    uint32_t logo = 0;  // the save system's settings' +0x20 ([G+0x64]+0x74): the player's crew logo
};

// Starts a new career in the career objects; driverName is the driver's name as the crew keeps it (8-bit).
void newCareer(Career& career, CareerContext& context, const std::string& driverName);

// 0x004BD8A0: the history's step: at its place the money and the eight rivals' respect; the place on (50 of them).
void recordHistory(Career& career);

// The unlock steps of a car model (0x004B8230): up to `changes` raised values from the car's step on. False when
// the steps ran out before that many were raised.
bool unlockSteps(CareerProfile& profile, size_t car, int changes, const ExeTables& tables);

// 0x004B4BE0: the dealer's offers made again: two or three of each car model the profile's respect level reaches
// (three times its dealer threshold), sorted; a level raised calls the player (a phone message, rand() & 3).
void stockDealer(Career& career, CareerContext& context);

// The dealer's list shown (+0x14, its count +0x27D4) and its fields (+0x00 the mode, +0x04 the entry chosen
// (Career::dealerEntry), +0x08 a car bought or sold in the garage, +0x27D8 the list it shows, +0x27DC the chosen
// entry's +0x00).

// 0x004B4440: an entry for each garage car (its garage index, model, 1 (used), condition 1 (2 and +0x24 set when its
// grid is damaged, 0x00557E80), the model's class table value (0x0048CE20), mileage 0, its rating (cut), its sale
// worth (0x0048DA40), +0x20 0, +0x28 more than three prototype parts (0x004A8F50)); then the garage's first entries
// sorted by rating and by +0x24, each the greatest first (a swap starts the pass again).
void dealerGarageList(Career& career, const ExeTables& tables, const PartCatalogue& catalogue);

// 0x004B4620: the dealer's offers (+0x27EC, count +0x4FAC) added to the list as they are.
void dealerOffersList(Dealer& dealer);

// 0x004B52F0: the first entry of the list with that +0x00 chosen: +0x27DC its +0x00, +0x04 the entry, +0x08 0.
void dealerChoose(Career& career, int32_t index);

// 0x004B43C0: the dealer's mode (+0x00), the list and +0x08, +0x27DC emptied; mode 1 the offers (0x004B4620, +0x27D8
// its +0x27E8), else the garage (0x004B4440, +0x27D8 the garage).
void dealerShow(Career& career, DealerMode mode, const ExeTables& tables, const PartCatalogue& catalogue);

// 0x004B5330: the dealer's cars (+0x4FB0) emptied, then a car for each offer: the model's CarDef (0x0048CC30), the
// offer's paint (+0x2C .. +0x34 to the car's +0x18 .. +0x20), modified automatically (0x0048F2C0, its +0x28 the
// prototype flag, level -1) when its +0x20 is set, rated (0x0048E8A0) and given its grid (0x005562A0).
void makeDealerCars(Career& career, CareerContext& context);

// 0x004B5070: the entry of the list's place taken to the garage. Mode 1: a car of that offer as above (the showroom,
// +0x27E8, emptied first) and +0x27DC 0, +0x04 the offer; then a car each for the first two other used offers
// without damage (0x004B5C20), these without a grid. Else +0x04 the list's entry and +0x27DC its +0x00. +0x08 0;
// returns 1.
int32_t dealerTake(Career& career, int32_t place, CareerContext& context);

// 0x004B5C20: the (n + 1)th offer, used (+0x08) and without damage (+0x24), other than the chosen one's +0x00; -1
// when there are fewer.
int32_t otherOffer(const Career& career, int32_t n);

// 0x004B5470: the garage car of the list's place (its +0x00). Stock problem: a list made before the garage was
// emptied (a first visit to the dealer's windows, 0x004C2AD0) names cars past it, read anyway; here none (null).
Car* dealerGarageCar(Career& career, int32_t place);

// 0x004B5700: the damage of a damaged offer (mode 1, the entry's +0x24) put on the showroom's first car's grid, 25
// times (0x007255E0) a point of rand(): kind 1 (front) x = rand() x 0.5 / 32767 x 400 + 70, negated when rand() /
// 32767 is over 0.5, y = rand() % 30 + 40, z 400, 8 each; kind 2 (a side by rand() / 32767 over 0.5: x 400, else
// -400) y = rand() % 100 + 40, z = (rand() / 32767 - 0.5) x 400, 24 each; kind 3 (rear) as kind 1 with z -400.
void damageShowroomCar(Career& career, CrtRandom& random);

// 0x004B5490: with +0x08 set (cleared): mode 1 the showroom's car at +0x27DC joins the garage (its grid made,
// 0x005562A0) and leaves the showroom (0x00489D30); else the garage car at +0x27DC is taken out (0x00489CD0). The
// showroom is emptied (0x00489C30).
void dealerFinish(Career& career);

// 0x004B6640: a phone contact of that kind, name and crew added at the end (kind 1 never; a name already there,
// without regard to case, not again); true when added. Stock problem: past 26 contacts the stock code writes after the
// list; here none is added.
bool addPhoneContact(Phone& phone, PhoneCaller kind, const std::string& name, int32_t crew);

// 0x004B8320: the profile's respect level (+0x4E) and every crew's +0x2CC up one.
void respectLevelUp(Career& career);

// 0x004A6E10: a crew's respect (+0x70) the value, none below 0, cut toward zero.
void setCrewRespect(Crew& crew, float respect);

// 0x004B80D0: a crewmate (by its number) joins the player's crew when it has fewer than four members.
void crewmateJoins(Career& career, int32_t mate);

// 0x004BC2B0: crews 1 .. 8's gains into their respect (0x004BBD80).
void updateRivalsRespect(Career& career, const ExeTables& tables);

// 0x004A6E50: a cheat. 0 the arcade's progress unlocked (0x0042B6B0 on [G+0x8C]: GameData::applyCheat); 1 the money
// 10000000; 2 for crews 1 .. 8 two respect levels up (0x004B8320), the crew's respect 2000 (0x004A6E10), its +0x2D4 and
// +0x2D8 0, its fourth step reached (+0x60), a respect note (kind 23, its number) and its leader a phone contact
// ("<short name> (<crew's name>)", kind 0), then their gains (0x004BC2B0) and both note lists emptied (0x004B5CC0);
// 3 for each of the 53 models a respect level up, its unlock steps (0x004B8230, 100), the profile's +0x50 and +0x262
// values 10 and its +0x474 value 10, then every +0x50 value 3, both note lists emptied and, until the garage has 55
// cars, a car of each model from 1 on with a paint at random (rand() % the paints), rated and with its grid; 4 every
// crewmate not in the crew joins it (0x004B80D0); 5 and 6 [career + 0x64] +0x00 and +0x04 1; 7 cheats 0 .. 4 and the
// profile's +0x628 0.
void applyCheat(Career& career, Cheat cheat, CareerContext& context);

// 0x004BB150: the player's own racer's car in the session (crew 0, member 0), else the first racer's of crew 0, else 0.
int32_t playerRaceCar(const RaceSession& session);

// 0x004BBD80: the crew's gains go into its respect now (+0x74; each gain not 0 noted in the career's +0x3C list
// by 0x004BBBF0, the sum cut toward zero), held to 50 for each challenge of its leader met (310 for the leader of id
// 1); then the respect steps reached or lost are checked.
void updateRespect(Career& career, Crew& crew, const ExeTables& tables);

// 0x004B5CC0: a message list emptied with the gains of crews 1 .. 8 (0x004BC760) and every crew's +0x330.
void clearNotes(Career& career, CareerNotes& notes);

// The default cars that fill a saved garage up to 55 (written with the profile, 0x004B7770): the CarDef of "celica"
// with its grid.
std::vector<Car> garagePadding(const Career& career, const CarDefs& defs);

}  // namespace oj
