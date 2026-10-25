// Unit tests: Dealer.

#include "Framework/Test.h"

#include "Core/Career/Career.h"
#include "Core/Career/CareerInit.h"
#include "Core/Cars/Car.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Cars/CarRepair.h"
#include "Core/Data/ExeTables.h"

#include <bit>
#include <cstring>
#include <memory>

namespace {

using oj::test::check;

}  // namespace

// The dealer's garage list (0x004B4440): three undamaged cars rated 300, 500 and 400 listed by rating, the
// greatest first, each with its garage index, condition 1, the model's class and its sale worth.
OJ_UNIT_TEST(Dealer, GarageList) {
    auto career = std::make_unique<oj::Career>();
    oj::CarDef def;
    def.id = 1;
    def.value = 10000;
    oj::ExeTables tables;
    tables.modelClasses.assign(53, 2);
    oj::PartCatalogue catalogue;
    for (const float rating : {300.0f, 500.0f, 400.0f}) {
        oj::Car car;
        car.def = &def;
        car.body.fill(0xFF);
        car.mods.fill(0xFF);
        car.nitro = std::bit_cast<uint32_t>(15.0f);
        car.rating = std::bit_cast<uint32_t>(rating);
        career->garage.push_back(car);
    }
    oj::dealerGarageList(*career, tables, catalogue);
    const auto& list = career->dealer.list;
    int32_t count = 0;
    std::memcpy(&count, reinterpret_cast<const uint8_t*>(&career->dealer) + 0x27D4, 4);
    const int worth = oj::saleWorth(career->garage[0], catalogue);
    const bool ordered = count == 3 && list[0].index == 1 && list[1].index == 2 && list[2].index == 0 &&
                         list[0].rating == 500 && list[2].rating == 300;
    const bool fields = list[0].condition == 1 && list[0].used == 1 && list[0].modelClass == 2 &&
                        list[0].price == worth && worth == 7000 && list[0].damage == 0;
    check(ordered && fields, "dealer: the garage's list by rating with the sale worths");
}
// The dealer's other offers (0x004B5C20) pass over the chosen one, the new and the damaged; a car bought in
// the garage (+0x08, mode 1) joins the garage from the showroom, one sold (mode 0) leaves it (0x004B5490).
OJ_UNIT_TEST(Dealer, OtherOffers) {
    auto career = std::make_unique<oj::Career>();
    oj::Dealer& dealer = career->dealer;
    dealer.offerCount = 5;
    for (int i = 0; i < 5; ++i) {
        dealer.offers[static_cast<size_t>(i)].index = i;
        dealer.offers[static_cast<size_t>(i)].used = i == 1 ? 0 : 1;
    }
    dealer.offers[3].damage = 2;
    career->dealerEntry = &dealer.offers[0];
    const bool others = oj::otherOffer(*career, 0) == 2 && oj::otherOffer(*career, 1) == 4 &&
                        oj::otherOffer(*career, 2) == -1;
    const auto set = [&](size_t at, int32_t v) {
        std::memcpy(reinterpret_cast<uint8_t*>(&dealer) + at, &v, 4);
    };
    oj::CarDef a;
    a.id = 7;
    oj::CarDef b;
    b.id = 9;
    oj::Car car;
    car.def = &a;
    career->dealerShowroom.push_back(car);
    car.def = &b;
    career->dealerShowroom.push_back(car);
    set(0x00, 1);
    set(0x08, 1);
    set(0x27DC, 1);
    oj::dealerFinish(*career);
    const bool bought = career->garage.size() == 1 && career->garage[0].def == &b && career->garage[0].grid &&
                        career->dealerShowroom.empty();
    car.def = &a;
    career->garage.insert(career->garage.begin(), car);
    set(0x00, 0);
    set(0x08, 1);
    set(0x27DC, 0);
    oj::dealerFinish(*career);
    const bool sold = career->garage.size() == 1 && career->garage[0].def == &b;
    check(others && bought && sold, "dealer: other offers, a car bought and a car sold");
}

// The fixed build's wing mirrors open by their own steps of the model's unlocks (none of which raises them): the
// standard part alone before step 14, the first from it, the second from 26, every part from the last, 37.
OJ_UNIT_TEST(Unlocks, MirrorLevels) {
    auto career = std::make_unique<oj::Career>();
    oj::CareerProfile& p = career->profile;
    const auto at = [&](uint8_t step) {
        p.unlockStep[5] = step;
        return oj::mirrorLevel(p, 5);
    };
    check(at(0) == -1 && at(13) == -1 && at(14) == 0 && at(25) == 0 && at(26) == 1 && at(36) == 1 && at(37) == 100 &&
              at(38) == 100 && oj::mirrorLevel(p, 60) == -1,
          "mirror levels (fixed) by the model's unlock step");
}
