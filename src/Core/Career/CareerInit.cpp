#include "Core/Career/CareerInit.h"
#include "Core/Race/RaceSounds.h"

#include "Core/Career/Phone.h"

#include "Core/Career/CareerEvents.h"
#include "Core/Career/CareerFiles.h"
#include "Core/Cars/CarDamage.h"
#include "Core/Cars/CarRepair.h"
#include "Core/Base/Log.h"
#include "Core/Race/RaceSession.h"
#include "Core/Base/X87.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string_view>
#include <utility>

namespace oj {

namespace {

std::optional<std::string> readText(const GameFiles& files, const char* name) {
    const auto data = files.read(std::filesystem::path("scripts") / name);
    if (!data) return std::nullopt;
    return std::string(reinterpret_cast<const char*>(data->data()), data->size());
}

void giveGrid(Car& car) {
    if (!car.grid) car.grid = GridDamage{};
}

template <size_t N>
void copyText(std::array<char, N>& to, const char* from) {
    const size_t n = std::min(std::strlen(from), N - 1);
    std::memcpy(to.data(), from, n);
    to[n] = 0;
}

// 0x004B5FD0
void startPhone(Phone& phone, CrtRandom& random) {
    phone.state = PhoneState::Idle;
    phone.timer = static_cast<float>(random.next()) * 0.00030518509f + 5.0f;
    if (void (*stop)() = speechStop()) stop();  // with the speech object, 0x0040B600
    for (PhoneMessage& message : phone.messages) message.used = 0;
    phone.contactCount = 0;
    phone.respectPending = 0;
    phone.challengesCrew = 0;
    phone.shownCrewmate = 0;
    phone.hostNoteShown = 0;
}

// 0x004A6340
void startCalendar(Calendar& calendar) {
    calendar.dayOfMonth = 0;
    calendar.month = 0;
    calendar.daysPlayed = 0;
    calendar.monthCopy = 0;
    for (CalendarEvent& event : calendar.events) {
        event.used = 0;
    }
}

// 0x004A4240: five groups of the CarDefs 1 .. 52.
void makeOpponentCars(Career& career, CareerContext& context) {
    CarMaking& making = context.making;
    career.opponentCars.clear();
    for (int group = 0; group < 5; ++group) {
        for (size_t i = 1; i < 53 && i < making.defs.cars.size(); ++i) {
            Car car = makeCar(&making.defs.cars[i]);
            switch (group) {
                case 0: car.mods[kNitro] = 1; break;
                case 1: autoModify(car, false, 0, making); break;
                case 2: autoModify(car, false, 1, making); break;
                case 3: autoModify(car, false, 2, making); break;
                default: autoModify(car, true, -1, making); break;
            }
            rateCar(car, context.dynamics.of(making.defs.cars[i]), making.catalogue);
            career.opponentCars.push_back(std::move(car));
        }
    }
}

// 0x004A7240 before CMSCrews.txt.
void clearCrew(Crew& crew) {
    for (size_t i = 0; i < 4; ++i) {
        crew.memberNames[i][0] = 0;
        crew.memberShortNames[i][0] = 0;
        crew.members[i] = CrewMemberDef{};
    }
    crew.number = crew.id;
    crew.respect = 0;
    crew.respectFactor.fill(0.0f);
    crew.respectGain.fill(0.0f);
    crew.collectionMark = 0;
    crew.bestCarMark = 0;
    crew.moneyMark = 1000.0f;
    crew.stage.fill(0);
    crew.circuitCount = 0;
    crew.p2pCount = 0;
    crew.cruiseCount = 0;
    crew.sprintCount = 0;
    crew.illegalCount = 0;
    crew.preferredRegion = 0;
    crew.preferredMake = 0;
    for (int32_t* v : {&crew.hostAccepted, &crew.hostWon, &crew.hostLost, &crew.racesSinceCall,
                       &crew.pinkSlipsWon}) *v = 0;
    crew.lastBetWon = -1;
    static const float kMoney[] = {0.0f, 2500.0f, 5000.0f, 7500.0f, 15000.0f, 40000.0f, 100000.0f, 250000.0f,
                                   100000.0f};
    crew.money = crew.id >= 0 && crew.id < 9 ? kMoney[crew.id] : 0.0f;
    for (CrewChallenge& c : crew.challenges) {
        c.done = 0;
        c.value = 0;
        c.car = 0;
        c.type = ChallengeType::LapTime;
    }
    crew.racesWonHosted = 0;
    crew.willAttend.fill(0);
    crew.circuitRoute.fill(0);
    crew.illegalRoute = 0;
    crew.p2pRoute.fill(0);
    crew.sprintRoute = 0;
}

// 0x004BBFC0 / 0x004BC070: a respect step reached or lost: the leader's +0x2D4 and +0x2D8 0, the step's flag, and a
// note in the career's +0x3C list (0x004B5C70) of kind 20 + the step (reached) or 24 + the step (lost), with the
// crew's number, 0 and 0.
void stepUp(Career& career, Crew& crew, Crew& leader, size_t step) {
    leader.unused = 0;
    leader.meterBlinks = 0;
    crew.stage[step] = 1;
    career.respectChanges.add(20 + static_cast<int32_t>(step), static_cast<float>(crew.number), 0.0f, 0.0f);
}

void stepDown(Career& career, Crew& crew, Crew& leader, size_t step) {
    leader.unused = 0;
    leader.meterBlinks = 0;
    crew.stage[step] = 0;
    career.respectChanges.add(24 + static_cast<int32_t>(step), static_cast<float>(crew.number), 0.0f, 0.0f);
}

}  // namespace

namespace {

// 0x004BBBF0: a gain of at least 1 either way noted in the career's +0x3C list: of kind k for a rise, 10 + k for a
// fall, with the gain, the crew's number and its respect.
void noteRespectChange(Career& career, const Crew& crew, size_t k, float change) {
    if (std::fabs(change) < 1.0f || k > 9) return;
    const auto kind = static_cast<int32_t>(change > 0.0f ? k : 10 + k);
    career.respectChanges.add(kind, change, static_cast<float>(crew.number), static_cast<float>(crew.respect));
}

}  // namespace

void updateRespect(Career& career, Crew& crew, const ExeTables& tables) {
    Crew& leader = career.crews[static_cast<size_t>(crew.number)];
    crew.respectNow = crew.respect;
    for (size_t k = 0; k < crew.respectGain.size(); ++k) {
        float& gain = crew.respectGain[k];
        if (gain != 0.0f) noteRespectChange(career, crew, k, gain);
        crew.respectNow = x87::chop(static_cast<float>(crew.respectNow) + gain);
        gain = 0.0f;
    }
    float least = 0.0f;
    if (leader.challenges[0].done != 0) least = 50.0f;
    if (leader.challenges[1].done != 0) least += 50.0f;
    if (leader.challenges[2].done != 0) least += 50.0f;
    if (leader.id == 1) least = 310.0f;
    if (least <= static_cast<float>(crew.respectNow)) least = static_cast<float>(crew.respectNow);
    crew.respectNow = x87::chop(least);
    const std::vector<int>& steps = tables.respectSteps;
    for (size_t i = 0; i < 4; ++i) {
        if (i == 3) {
            if (crew.stage[3] == 0) {
                // The last step's respect with no host race called yet: the leader's call "Host challenge?" (type 2,
                // sample 1).
                if (crew.respectNow >= steps[3] && leader.hostAccepted == 0) {
                    const auto id = static_cast<size_t>(std::clamp<int>(leader.id, 0, 8));
                    const std::string name =
                        std::string(leader.memberShortNames[0].data(),
                                    strnlen(leader.memberShortNames[0].data(), leader.memberShortNames[0].size())) +
                        " (" + kPhoneCrewNames[id] + ")";
                    queuePhoneMessage(career.phone, PhoneCaller::CrewLeader, name, leader.id, 1, leader.id,
                                      CallType::HostChallenge, phonetext::kHostChallenge, phonetext::kYes,
                                      phonetext::kNo, kPhoneNone, kPhoneNone);
                }
                if (leader.hostWon != 0) stepUp(career, crew, leader, 3);
            }
            if (leader.hostLost != 0) {
                stepDown(career, crew, leader, 3);
                crew.respectNow = std::max(0, std::min(crew.respectNow, steps[3] - 50));
                crew.respect = crew.respectNow;
            }
            leader.hostWon = 0;
            leader.hostLost = 0;
            leader.hostAccepted = 0;
        } else {
            if (crew.stage[i] == 0) {
                if (steps[i] <= crew.respectNow) stepUp(career, crew, leader, i);
                if (crew.stage[i] == 0) continue;
            }
            if (crew.respectNow < steps[i]) stepDown(career, crew, leader, i);
        }
    }
}

namespace {

// 0x004B7F40: the player's crew reaches its first steps, the respect of crews 0 and 1, the other crews' steps.
void startCrews(Career& career, const ExeTables& tables) {
    Crew& player = career.crews[0];
    for (size_t step : {size_t{0}, size_t{1}, size_t{3}}) {
        Crew& leader = career.crews[static_cast<size_t>(player.number)];
        leader.unused = 0;
        leader.meterBlinks = 0;
        player.stage[step] = 1;
    }
    career.crews[0].respect = 5000;
    career.crews[1].respect = 400;
    for (size_t i = 1; i < 9; ++i) {
        updateRespect(career, career.crews[i], tables);
    }
    clearNotes(career, career.meeting);
    clearNotes(career, career.respectChanges);
}

// 0x004B7660
void startProfile(Career& career, const ExeTables& tables) {
    CareerProfile& p = career.profile;
    p.money = 40000;
    p.backedPrizes = 0;
    p.crewPrizes = 0;
    p.raceCarKept = 1;
    p.memberPointers.fill(0);
    p.memberCount = 0;
    p.lastHostedType = 7;
    p.lastHostedLocation = -1;
    p.tenthCrewBeaten = 0;
    p.racesNotWon = 0;
    p.winsByType.fill(0);
    p.winsByLocation.fill(0);
    p.crewsBeaten.fill(0);
    startCrews(career, tables);
    for (size_t car = 0; car < 53; ++car) {
        p.unlockStep[car] = 0;
        p.partsA[car].fill(-1);
        p.partsB[car].fill(-1);
        p.carLevel[car] = -1;
        unlockSteps(p, car, 9, tables);
    }
    p.moneySpent = p.prizesWon = p.garageWorth = p.garageWorthBest = p.bestCarWorthTold = p.bestCarWorth = 0;
    p.completeShown = 0;
    p.respectLevel = 0;
    p.rivalsSkill = -0.4f;
}

// 0x004B4DD0
void offerCar(DealerOffer& offer, int model, CareerContext& context) {
    CarMaking& making = context.making;
    CrtRandom& random = making.random;
    const CarDef* def = findCarDef(making.defs, context.tables.carModels[static_cast<size_t>(model)]);
    offer.model = model;
    offer.used = def->state;
    offer.price = def->value + static_cast<int32_t>(static_cast<uint32_t>(random.next()) % 5) * 100;
    if (offer.used == 0 && static_cast<uint32_t>(random.next()) % 100 > 50) {
        offer.used = 1;
    }
    offer.mileage = offer.used == 0 ? static_cast<int32_t>(static_cast<uint32_t>(random.next()) % 15)
                                    : static_cast<int32_t>(static_cast<uint32_t>(random.next()) % 50000 + 5000);
    if (offer.used == 1 && def->state == 0) {
        const auto lowered = static_cast<int32_t>(static_cast<float>(offer.price) * 0.8f);
        offer.price = static_cast<int32_t>(static_cast<float>(lowered) - static_cast<float>(offer.mileage) * 0.1f);
    }
    offer.condition = offer.mileage > 20000 ? 1 : 0;
    offer.modelClass = def->id >= 0 && def->id <= 52 ? context.tables.modelClasses[static_cast<size_t>(def->id)] : 0;
    offer.prototypeParts = 0;
    offer.modified = 0;
    offer.damage = 0;
    if (offer.used == 1 && static_cast<uint32_t>(random.next()) % 100 > 60) {
        const auto r = static_cast<uint32_t>(random.next());
        offer.condition = 2;
        offer.damage = static_cast<int32_t>(r % 3 + 1);
        offer.price = static_cast<int32_t>(static_cast<float>(offer.price) * 0.9f);
    }
    const std::vector<CarPaint>& paints = making.defs.paints;
    const CarPaint& paint = paints[static_cast<uint32_t>(random.next()) % paints.size()];
    offer.base = paint.base;
    offer.metallic = offer.base;
    offer.pearl = offer.base;
    offer.metallicAmount = 0.0f;
    offer.pearlAmount = 0.0f;
    Car car = makeCar(findCarDef(making.defs, context.tables.carModels[static_cast<size_t>(model)]));
    if (offer.modified != 0) {
        autoModify(car, offer.prototypeParts != 0, -1, making);
    }
    offer.rating = static_cast<int32_t>(rateCar(car, car.def != nullptr ? context.dynamics.of(*car.def) : nullptr,
                                                making.catalogue));
}

// 0x004B4D30: largest rating first (an exchange sort that starts again after each swap), then numbered.
void sortOffers(Dealer& dealer) {
    const int count = dealer.offerCount;
    if (count != 1 && count - 1 >= 0) {
        for (int i = 0; i < count - 1;) {
            if (dealer.offers[static_cast<size_t>(i)].rating < dealer.offers[static_cast<size_t>(i) + 1].rating) {
                std::swap(dealer.offers[static_cast<size_t>(i)], dealer.offers[static_cast<size_t>(i) + 1]);
                i = 0;
            } else {
                ++i;
            }
        }
    }
    for (int i = 0; i < count; ++i) {
        dealer.offers[static_cast<size_t>(i)].index = i;
    }
}

}  // namespace

void dealerGarageList(Career& career, const ExeTables& tables, const PartCatalogue& catalogue) {
    Dealer& dealer = career.dealer;
    int32_t count = dealer.listCount;
    for (size_t k = 0; k < career.garage.size(); ++k) {
        const Car& car = career.garage[k];
        // Stock problem: past 159 entries the list runs into the fields after it; not kept.
        if (count >= static_cast<int32_t>(dealer.list.size())) break;
        ++count;
        DealerOffer& e = dealer.list[static_cast<size_t>(count - 1)];
        e.index = static_cast<int32_t>(k);
        e.used = 1;
        const int id = car.def != nullptr ? car.def->id : 0;
        e.model = id;
        e.condition = 1;
        e.modelClass = id >= 0 && static_cast<size_t>(id) < tables.modelClasses.size()
                           ? tables.modelClasses[static_cast<size_t>(id)] : 0;
        if (car.grid && damageLevel(*car.grid) > 0.0f) {
            e.condition = 2;
            e.damage = 1;
        } else {
            e.damage = 0;
        }
        e.mileage = 0;
        e.rating = x87::chop(std::bit_cast<float>(car.rating));
        e.price = saleWorth(car, catalogue);
        e.modified = 0;
        e.prototypeParts = heavilyTuned(car, catalogue) ? 1 : 0;
    }
    dealer.listCount = count;
    const size_t n = std::min(career.garage.size(), dealer.list.size());
    for (size_t i = 0; i + 1 < n; ++i) {
        if (dealer.list[i].rating < dealer.list[i + 1].rating) {
            std::swap(dealer.list[i], dealer.list[i + 1]);
            i = static_cast<size_t>(-1);
        }
    }
    for (size_t i = 0; i + 1 < n; ++i) {
        if (dealer.list[i].damage < dealer.list[i + 1].damage) {
            std::swap(dealer.list[i], dealer.list[i + 1]);
            i = static_cast<size_t>(-1);
        }
    }
}

void dealerOffersList(Dealer& dealer) {
    int32_t count = dealer.listCount;
    for (int32_t i = 0; i < dealer.offerCount && i < static_cast<int32_t>(dealer.offers.size()); ++i) {
        // Stock problem: past 159 entries the list runs into the fields after it; not kept.
        if (count >= static_cast<int32_t>(dealer.list.size())) break;
        ++count;
        dealer.list[static_cast<size_t>(count - 1)] = dealer.offers[static_cast<size_t>(i)];
    }
    dealer.listCount = count;
}

void dealerChoose(Career& career, int32_t index) {
    Dealer& dealer = career.dealer;
    const int32_t count = dealer.listCount;
    for (int32_t i = 0; i < count && i < static_cast<int32_t>(dealer.list.size()); ++i) {
        if (dealer.list[static_cast<size_t>(i)].index != index) continue;
        dealer.chosen = dealer.list[static_cast<size_t>(i)].index;
        career.dealerEntry = &dealer.list[static_cast<size_t>(i)];
        dealer.deal = 0;
        return;
    }
}

namespace {

Car dealerCar(const DealerOffer& offer, CareerContext& context, bool grid) {
    CarMaking& making = context.making;
    Car car = makeCar(findCarDef(making.defs, context.tables.carModels[static_cast<size_t>(offer.model)]));
    car.base = offer.base;
    car.metallic = offer.metallic;
    car.pearl = offer.pearl;
    if (offer.modified != 0) autoModify(car, offer.prototypeParts != 0, -1, making);
    rateCar(car, car.def != nullptr ? context.dynamics.of(*car.def) : nullptr, making.catalogue);
    if (grid) giveGrid(car);
    return car;
}

}  // namespace

void makeDealerCars(Career& career, CareerContext& context) {
    const Dealer& dealer = career.dealer;
    career.dealerCars.clear();
    for (int32_t i = 0; i < dealer.offerCount && i < static_cast<int32_t>(dealer.offers.size()); ++i) {
        career.dealerCars.push_back(dealerCar(dealer.offers[static_cast<size_t>(i)], context, true));
    }
}

int32_t otherOffer(const Career& career, int32_t n) {
    const Dealer& dealer = career.dealer;
    const int32_t chosen = career.dealerEntry != nullptr ? career.dealerEntry->index : 0;
    int32_t left = n + 1;
    for (int32_t i = 0; i < dealer.offerCount && i < static_cast<int32_t>(dealer.offers.size()); ++i) {
        const DealerOffer& offer = dealer.offers[static_cast<size_t>(i)];
        if (offer.damage != 0 || offer.used == 0 || i == chosen) continue;
        if (--left == 0) return i;
    }
    return -1;
}

int32_t dealerTake(Career& career, int32_t place, CareerContext& context) {
    Dealer& dealer = career.dealer;
    if (dealer.mode != DealerMode::Offers) {
        const DealerOffer& entry = dealer.list[static_cast<size_t>(place)];
        career.dealerEntry = &entry;
        dealer.chosen = entry.index;
        dealer.deal = 0;
        return 1;
    }
    Car car = dealerCar(dealer.offers[static_cast<size_t>(place)], context, true);
    career.dealerShowroom.clear();
    career.dealerShowroom.push_back(std::move(car));
    dealer.chosen = 0;
    career.dealerEntry = &dealer.offers[static_cast<size_t>(place)];
    for (int32_t n = 0; n < 2; ++n) {
        const int32_t other = otherOffer(career, n);
        if (other == -1) continue;
        career.dealerShowroom.push_back(dealerCar(dealer.offers[static_cast<size_t>(other)], context, false));
    }
    dealer.deal = 0;
    return 1;
}

Car* dealerGarageCar(Career& career, int32_t place) {
    if (place < 0 || static_cast<size_t>(place) >= career.dealer.list.size()) return nullptr;
    const auto index = static_cast<size_t>(career.dealer.list[static_cast<size_t>(place)].index);
    return index < career.garage.size() ? &career.garage[index] : nullptr;
}

void damageShowroomCar(Career& career, CrtRandom& random) {
    const Dealer& dealer = career.dealer;
    if (dealer.mode != DealerMode::Offers || career.dealerEntry == nullptr) return;
    const auto kind = static_cast<ShowroomDamage>(career.dealerEntry->damage);
    if (kind == ShowroomDamage::None || career.dealerShowroom.empty()) return;
    Car& car = career.dealerShowroom.front();
    if (!car.grid) car.grid.emplace();
    GridDamage& cells = *car.grid;
    constexpr int kHits = 25;  // 0x007255E0
    const auto front = [&](float z) {
        for (int n = 0; n < kHits; ++n) {
            float x = static_cast<float>(random.next()) * 1.5259255e-05f * 400.0f + 70.0f;
            if (static_cast<float>(random.next()) * 3.051851e-05f > 0.5f) x = x * -1.0f;
            const auto y = static_cast<float>(static_cast<uint32_t>(random.next()) % 30 + 40);
            DamageGrid::keep(cells, {x, y, z, 0.0f}, 8.0f);
        }
    };
    if (kind == ShowroomDamage::Front) front(400.0f);
    if (kind == ShowroomDamage::Side) {
        const float x = static_cast<float>(random.next()) * 3.051851e-05f <= 0.5f ? -400.0f : 400.0f;
        for (int n = 0; n < kHits; ++n) {
            const auto y = static_cast<float>(static_cast<uint32_t>(random.next()) % 100 + 40);
            const float z = (static_cast<float>(random.next()) * 3.051851e-05f - 0.5f) * 400.0f;
            DamageGrid::keep(cells, {x, y, z, 0.0f}, 8.0f * 3.0f);
        }
    }
    if (kind == ShowroomDamage::Back) front(-400.0f);
}

void dealerFinish(Career& career) {
    Dealer& dealer = career.dealer;
    if (dealer.deal != 0) {
        dealer.deal = 0;
        const auto at = static_cast<size_t>(dealer.chosen);
        if (dealer.mode == DealerMode::Offers) {
            // The list shown (+0x27D8) is the showroom in this mode.
            if (at < career.dealerShowroom.size()) {
                career.garage.push_back(career.dealerShowroom[at]);
                giveGrid(career.garage.back());
                career.dealerShowroom.erase(career.dealerShowroom.begin() + static_cast<ptrdiff_t>(at));
            }
        } else if (at < career.garage.size()) {
            career.garage.erase(career.garage.begin() + static_cast<ptrdiff_t>(at));
        }
    }
    career.dealerShowroom.clear();
}

void dealerShow(Career& career, DealerMode mode, const ExeTables& tables, const PartCatalogue& catalogue) {
    Dealer& dealer = career.dealer;
    dealer.mode = mode;
    dealer.listCount = 0;
    dealer.deal = 0;
    dealer.chosen = 0;
    if (mode == DealerMode::Offers) {
        dealerOffersList(dealer);
        dealer.listCars = dealer.showroom;
    } else {
        // +0x27D8 the garage list's address: not kept.
        dealerGarageList(career, tables, catalogue);
    }
}

int32_t playerRaceCar(const RaceSession& session) {
    for (uint32_t i = 0; i < session.count && i < session.racers.size(); ++i) {
        if (session.racers[i].crew == 0 && session.racers[i].member == 0) return session.racers[i].car;
    }
    for (uint32_t i = 0; i < session.count && i < session.racers.size(); ++i) {
        if (session.racers[i].crew == 0) return session.racers[i].car;
    }
    return 0;
}

void stockDealer(Career& career, CareerContext& context) {
    Dealer& dealer = career.dealer;
    CrtRandom& random = context.making.random;
    bool raised = false;
    dealer.offerCount = 0;
    auto offer = [&](int model) {
        offerCar(dealer.offers[static_cast<size_t>(dealer.offerCount)], model, context);
        ++dealer.offerCount;
    };
    for (int model = 1; model < 53; ++model) {
        const int threshold = context.tables.dealerThresholds[static_cast<size_t>(model)];
        if (career.profile.respectLevel < threshold * 3) continue;
        if (threshold > dealer.level) {
            raised = true;
            dealer.level = threshold;
        }
        offer(model);
        if (random.next() & 1) offer(model);
        if (random.next() & 1) offer(model);
    }
    sortOffers(dealer);
    if (raised) queueDealerMessage(career, random);
}

namespace {

// 0x004B80D0 for crewmate 0, the driver.
void joinCrew(Career& career, size_t mateIndex, const std::string& driverName) {
    CareerProfile& p = career.profile;
    if (p.memberCount >= 4) return;
    const auto slot = static_cast<size_t>(p.memberCount);
    Crewmate& mate = career.crewmates[mateIndex];
    // The save keeps the member's address (and, for free slots, a value of the writer's stack: a stock problem); 0
    // here.
    p.memberPointers[slot] = 0;
    career.members[slot] = mate.number;
    mate.joined = 1;
    mate.races = 0;
    mate.wins = 0;
    mate.daysIdle = 0;
    mate.memberSlot = p.memberCount;
    mate.joinDay = career.calendar.month;
    Crew& crew = career.crews[0];
    if (mateIndex == 0) {
        copyText(crew.memberNames[slot], driverName.c_str());
        copyText(crew.memberShortNames[slot], driverName.c_str());
        copyText(mate.shortName, driverName.c_str());
        copyText(mate.name, driverName.c_str());
    } else {
        copyText(crew.memberNames[slot], mate.shortName.data());
        copyText(crew.memberShortNames[slot], mate.shortName.data());
    }
    ++p.memberCount;
}

}  // namespace

bool addPhoneContact(Phone& phone, PhoneCaller kind, const std::string& name, int32_t crew) {
    if (kind == PhoneCaller::Crewmate) return false;
    for (int32_t i = 0; i < phone.contactCount && i < static_cast<int32_t>(phone.contacts.size()); ++i) {
        if (_stricmp(name.c_str(), phone.contacts[static_cast<size_t>(i)].name.data()) == 0) return false;
    }
    if (phone.contactCount < 0 || phone.contactCount >= static_cast<int32_t>(phone.contacts.size())) return false;
    PhoneContact& contact = phone.contacts[static_cast<size_t>(phone.contactCount)];
    contact.name.fill('\0');
    copyText(contact.name, name.c_str());
    contact.crew = crew;
    contact.kind = kind;
    ++phone.contactCount;
    return true;
}

void respectLevelUp(Career& career) {
    career.profile.respectLevel = static_cast<int16_t>(career.profile.respectLevel + 1);
    for (Crew& crew : career.crews) ++crew.racesSinceCall;
}

void crewmateJoins(Career& career, int32_t mate) {
    if (mate < 0 || mate >= static_cast<int32_t>(career.crewmates.size())) return;
    joinCrew(career, static_cast<size_t>(mate), std::string());
}

void setCrewRespect(Crew& crew, float respect) {
    crew.respect = x87::chop(respect < 0.0f ? 0.0f : respect);
}

void updateRivalsRespect(Career& career, const ExeTables& tables) {
    for (size_t k = 1; k < 9; ++k) updateRespect(career, career.crews[k], tables);
}

void applyCheat(Career& career, Cheat cheat, CareerContext& context) {
    CareerProfile& p = career.profile;
    CarMaking& making = context.making;
    switch (cheat) {
    case Cheat::Arcade:
        // The arcade's object, not the career's: GameData::applyCheat (0x0042B6B0).
        return;
    case Cheat::Money: p.money = 10000000; return;
    case Cheat::Respect:
        for (size_t k = 1; k < 9; ++k) {
            respectLevelUp(career);
            respectLevelUp(career);
            Crew& crew = career.crews[k];
            setCrewRespect(crew, 2000.0f);
            Crew& numbered = career.crews[static_cast<size_t>(crew.number)];
            numbered.unused = 0;
            numbered.meterBlinks = 0;
            crew.stage[3] = 1;
            career.respectChanges.add(23, static_cast<float>(crew.number), 0.0f, 0.0f);
            const auto& shortName = crew.memberShortNames[0];
            const auto id = static_cast<size_t>(crew.id);
            const std::string crewName = id < context.tables.crewNames.size() ? context.tables.crewNames[id] : "";
            const std::string name = std::string(shortName.data(), strnlen(shortName.data(), shortName.size())) +
                                     " (" + crewName + ")";
            addPhoneContact(career.phone, PhoneCaller::CrewLeader, name, static_cast<int32_t>(k));
        }
        updateRivalsRespect(career, context.tables);
        clearNotes(career, career.meeting);
        clearNotes(career, career.respectChanges);
        return;
    case Cheat::CarsAndParts: {
        for (size_t m = 0; m < 53; ++m) {
            respectLevelUp(career);
            unlockSteps(p, m, 100, context.tables);
            p.partsA[m].fill(10);
            p.partsB[m].fill(10);
            p.carLevel[m] = 10;
        }
        for (auto& parts : p.partsA) parts.fill(3);
        clearNotes(career, career.meeting);
        clearNotes(career, career.respectChanges);
        const std::vector<CarPaint>& paints = making.defs.paints;
        for (size_t m = 1; m < 53; ++m) {
            if (career.garage.size() == 55) return;
            Car car = makeCar(findCarDef(making.defs, context.tables.carModels[m]));
            const CarPaint& paint = paints[static_cast<uint32_t>(making.random.next()) % paints.size()];
            car.base = paint.base;
            car.metallic = paint.metallic;
            car.pearl = paint.pearl;
            rateCar(car, car.def != nullptr ? context.dynamics.of(*car.def) : nullptr, making.catalogue);
            giveGrid(car);
            career.garage.push_back(std::move(car));
        }
        return;
    }
    case Cheat::Crewmates:
        for (size_t i = 0; i < career.crewmates.size(); ++i) {
            if (career.crewmates[i].joined == 0) joinCrew(career, i, std::string());
        }
        return;
    case Cheat::CharacterTest: career.cheatFlags[kCheatCharacterTest] = 1; return;
    case Cheat::WinAllRaces: career.cheatFlags[kCheatWinAllRaces] = 1; return;
    case Cheat::Everything:
        for (int32_t c = 0; c < 5; ++c) applyCheat(career, static_cast<Cheat>(c), context);  // Arcade .. Crewmates
        p.rivalsSkill = 0.0f;
        return;
    default:
        return;
    }
}

void recordHistory(Career& career) {
    History& h = career.history;
    const auto at = static_cast<size_t>(h.position);
    h.series[0][at] = static_cast<float>(career.profile.money);
    for (size_t k = 1; k < 9; ++k) {
        h.series[k][at] = static_cast<float>(career.crews[k].respect);
    }
    if (++h.position == 50) h.position = 0;
}

bool unlockSteps(CareerProfile& p, size_t car, int changes, const ExeTables& tables) {
    if (changes == 0) return true;
    const std::vector<ExeTables::UnlockStep>& steps = tables.unlockSteps;
    auto raise = [&](int8_t& field, int value) {
        if (field < value) {
            field = static_cast<int8_t>(value);
            return true;
        }
        return false;
    };
    size_t at = p.unlockStep[car];
    while (at < steps.size()) {
        const ExeTables::UnlockStep& s = steps[at];
        ++p.unlockStep[car];
        ++at;
        const auto index = static_cast<size_t>(s.index);
        if (s.kind == ExeTables::UnlockKind::Part) {
            if (raise(p.partsA[car][index], s.value)) --changes;
        } else if (s.kind == ExeTables::UnlockKind::BodyPart) {
            if (index != 0) {
                if (raise(p.partsB[car][index], s.value)) --changes;
            } else {
                if (raise(p.partsB[car][0], s.value)) --changes;
                raise(p.partsB[car][1], s.value);
                raise(p.partsB[car][4], s.value);
            }
        } else if (s.kind == ExeTables::UnlockKind::CarLevel) {
            if (raise(p.carLevel[car], s.value)) --changes;
        }
        if (changes == 0) return true;
    }
    return false;
}

void clearNotes(Career& career, CareerNotes& notes) {
    notes.count = 0;
    for (size_t i = 1; i < 9; ++i) career.crews[i].respectGain.fill(0.0f);
    for (Crew& crew : career.crews) crew.rams = 0.0f;
}

std::vector<Car> garagePadding(const Career& career, const CarDefs& defs) {
    std::vector<Car> padding;
    for (size_t i = career.garage.size(); i < 55; ++i) {
        Car car = makeCar(findCarDef(defs, "celica"));
        giveGrid(car);
        padding.push_back(std::move(car));
    }
    return padding;
}

void newCareer(Career& career, CareerContext& context, const std::string& driverName) {
    CarMaking& making = context.making;
    startPhone(career.phone, making.random);
    startCalendar(career.calendar);
    for (size_t k = 0; k < 9; ++k) {
        career.crews[k].id = static_cast<int16_t>(k);
    }
    makeOpponentCars(career, context);

    career.garage.clear();
    if (const auto text = readText(context.files, "CMSPlayersCrewCollection.txt")) {
        career.garage = loadCollection(*text, making);
    }
    if (!career.garage.empty()) {
        autoModify(career.garage.front(), false, -1, making);
    }
    for (Car& car : career.garage) {
        rateCar(car, car.def != nullptr ? context.dynamics.of(*car.def) : nullptr, making.catalogue);
    }
    for (Car& car : career.garage) giveGrid(car);

    for (Crew& crew : career.crews) {
        crew.baseColour = 0x80FFFFFF;
        crew.metallicColour = 0x80FFFFFF;
        crew.pearlColour = 0x80FFFFFF;
        uint32_t bits = 0x80FFFFFF;
        std::memcpy(&crew.metallicAmount, &bits, 4);
    }
    const std::optional<std::string> crews = readText(context.files, "CMSCrews.txt");
    for (Crew& crew : career.crews) {
        clearCrew(crew);
        if (crews) readCrewFile(*crews, crew, context.tables);
        crew.unused = 0;
        crew.meterBlinks = 0;
    }
    const std::optional<std::string> mates = readText(context.files, "CMSCrewmates.txt");
    for (size_t k = 0; k < career.crewmates.size(); ++k) {
        Crewmate& mate = career.crewmates[k];
        mate.number = static_cast<int32_t>(k);
        mate.joined = 0;
        mate.daysIdle = 0;
        mate.races = 0;
        mate.wins = 0;
        mate.daysWanting = 100;
        if (mates) readCrewmateFile(*mates, mate, context.currentCrewmate);
    }
    if (const auto challenges = readText(context.files, "CMSCrewChallenges.txt")) {
        readChallengeFile(*challenges, career.crews, context.tables);
    }
    startProfile(career, context.tables);
    // 0x004B2C20: the flyers, the player's crew logo the one chosen (the table 0x00718A28; stock problem: a choice
    // past its 19 reads past it; the file's logo kept here).
    if (const auto flyers = readText(context.files, "CMSFlyers.txt")) career.flyers = readFlyersFile(*flyers);
    static constexpr std::array<const char*, 19> kChosenLogos = {
        "ui_chooselogo_01", "ui_chooselogo_02", "ui_chooselogo_03", "ui_chooselogo_04", "ui_chooselogo_05",
        "ui_chooselogo_06", "ui_chooselogo_07", "ui_chooselogo_08", "ui_chooselogo_09", "ui_chooselogo_10",
        "ui_chooselogo_11", "ui_chooselogo_12", "ui_chooselogo_13", "ui_chooselogo_14", "ui_chooselogo_15",
        "ui_chooselogo_16", "ui_chooselogo_17", "ui_chooselogo_18", "ui_chooselogo_20",
    };
    if (context.logo < kChosenLogos.size()) career.flyers.crews[0].logo = kChosenLogos[context.logo];

    CareerBets& bets = career.bets;
    bets.placed = 0;
    bets.won = 0;
    bets.amount = 0.0f;
    bets.pinkSlip = 0;
    bets.talkedTo.fill(0);
    bets.rivalAsked = 0;
    stockDealer(career, context);
    joinCrew(career, 0, driverName);
    firstEvent(career, context);
    fillCalendar(career, context);

    Advisor& advisor = career.advisor;
    advisor.toldRatings.fill(0);
    advisor.chosen = 0;
    advisor.openPhone = 0;
    advisor.openHosting = 0;
    advisor.openCalendar = 0;
    advisor.openDealer = 0;
    advisor.hosted = 0;
    advisor.event = 0;
    advisor.lastAdvice = 13;
    advisor.advice = 13;

    career.history = History{};
    recordHistory(career);
    career.calendar.newCareer = 1;
    career.padding = garagePadding(career, making.defs);
}

}  // namespace oj
