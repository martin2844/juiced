#include "Core/Cars/CarDefs.h"
#include "Core/Gamble/CareerPoker.h"
#include "Framework/Test.h"
#include <algorithm>
#include <random>

using namespace oj;
using oj::test::check;

OJ_UNIT_TEST(PokerCareer, SettlementAndSaveFormat) {
    CarDef def;
    def.id = 1;
    def.value = 10000;
    def.model = "TEST_STOCK";
    Car car;
    car.def = &def;
    car.setModel(def.model);
    car.base = 0xff332211;
    for (bool win : {false, true}) {
        Career career;
        career.profile.money = 5000;
        career.garage.push_back(car);
        career.opponentCars.push_back(car);
        career.crews[1].baseColour = 0xff123456;
        const auto before = career.write();
        poker::CareerHand game(career, std::array{1}, 1000, win ? 1 : 0, 0, 123u);
        check(career.write() == before, "Dealing changes no legacy save bytes");
        // Dealer is seat zero for this deterministic mixed-title sequence.
        game.hand = poker::Hand(std::array{1000, 1000}, 0, 20, 123u);
        check(game.pledge(500), "Player raises with title plus $500");
        check(game.hand.pot() == 10530, "Title value enters the same cash pot");
        check(game.pledge(490), "Rival calls with title plus exact cash difference");
        const int fundedPot = game.hand.pot();
        check(!game.pledge() && game.hand.pot() == fundedPot, "The same title cannot fund another wager");
        if (!win)
            game.hand.act(poker::Action::Raise, 20);
        game.hand.act(poker::Action::Fold);
        const int expectedValue = game.equityDelta(0);
        game.settle(career);
        check(career.profile.money == 5000 + game.cashDelta(), "Career pays net cash after physical prizes");
        check(career.garage.size() == (win ? 2u : 0u), "Only wagered titles change ownership");
        const int wealth = career.profile.money + static_cast<int>(career.garage.size()) * 10000;
        check(wealth - 15000 == expectedValue, "Receiving a car cannot duplicate its value as cash");
        if (win) {
            check(career.garage[0].base == car.base, "Winning preserves player's existing car");
            check(career.garage[1].base == 0xff123456, "Prize retains rival's paint");
        }
        check(career.crews[1].pinkSlipsWon == 0, "Poker does not consume career pink-slip challenge progression");
        const auto settled = career.write();
        bool rejected = false;
        try {
            game.settle(career);
        } catch (...) {
            rejected = true;
        }
        check(rejected && career.write() == settled, "Repeated settlement cannot pay twice");
        Career restored;
        std::string error;
        check(restored.read(settled, &error) && restored.write() == settled,
              "Poker result round-trips in unchanged career format");
    }
}

OJ_UNIT_TEST(PokerCareer, RejectWithoutMutation) {
    Career career;
    career.profile.money = 500;
    const auto before = career.write();
    bool rejected = false;
    try {
        poker::CareerHand game(career, std::array{1}, 1000, 0, {}, 1u);
    } catch (...) {
        rejected = true;
    }
    check(rejected && before == career.write(), "Insufficient funds do not mutate career");
    rejected = false;
    try {
        poker::CareerHand game(career, std::array{1}, 100, 0, 0, 1u);
    } catch (...) {
        rejected = true;
    }
    check(rejected && before == career.write(), "Nonexistent title rejected");
    poker::CareerHand game(career, std::array{1}, 100, 0, {}, 1u);
    rejected = false;
    try {
        game.settle(career);
    } catch (...) {
        rejected = true;
    }
    check(rejected && before == career.write(), "Unfinished hand cannot settle");
    game.hand.act(poker::Action::Fold);
    ++career.profile.money;
    const auto changed = career.write();
    rejected = false;
    try {
        game.settle(career);
    } catch (...) {
        rejected = true;
    }
    check(rejected && changed == career.write(), "Concurrent career change cancels settlement without overwrite");
}

OJ_UNIT_TEST(PokerCareer, TiedTitlesAndGarageCapacity) {
    CarDef def;
    def.id = 1;
    def.value = 10000;
    def.model = "TEST_STOCK";
    Car car;
    car.def = &def;
    car.setModel(def.model);
    Career career;
    career.profile.money = 5000;
    career.garage = {car};
    career.opponentCars = {car};
    poker::CareerHand game(career, std::array{1}, 1000, 0, 0, 10u);
    const auto before = career.write();
    std::array<poker::Card, 52> deck{};
    std::vector<poker::Card> prefix = {0, 1, 2, 3, 4, 47, 48, 49, 5, 50, 6, 51};
    size_t at = 0;
    for (auto c : prefix)
        deck[at++] = c;
    for (int c = 0; c < 52; ++c)
        if (std::find(prefix.begin(), prefix.end(), c) == prefix.end())
            deck[at++] = c;
    game.hand = poker::Hand(std::array{1000, 1000}, 0, 20, deck);
    check(game.pledge(500) && game.pledge(490), "Both titles enter the tied board pot");
    while (!game.hand.finished())
        game.hand.act(poker::Action::CheckCall);
    game.settle(career);
    check(career.write() == before, "Tied cash and titles return exact original career bytes");
    career.garage.resize(55, car);
    const auto full = career.write();
    bool rejected = false;
    try {
        poker::CareerHand impossible(career, std::array{1}, 1000, 0, 0, 10u);
    } catch (...) {
        rejected = true;
    }
    check(rejected && career.write() == full, "Full garage cannot risk a hand whose prizes will not fit");
}

OJ_UNIT_TEST(PokerCareer, OptionalTitlesAndRejectedWagers) {
    CarDef def;
    def.id = 1;
    def.value = 10000;
    def.model = "TEST_STOCK";
    Car car;
    car.def = &def;
    car.setModel(def.model);
    Career career;
    career.profile.money = 5000;
    career.garage = {car};
    career.opponentCars = {car};
    poker::CareerHand game(career, std::array{1}, 1000, 0, 0, 12u);
    check(!game.pledge(-1) && !game.pledge(1000) && game.hand.pot() == 30 && !game.pledged(0),
          "Rejected cash amount adds no title or chips");
    game.hand.act(poker::Action::Fold);
    game.settle(career);
    check(career.garage.size() == 1 && !game.carLost() && game.titleOwner(0) == -2,
          "Folding without wagering selected title preserves the car");
    check(career.profile.money == 4990, "Cash-only fold loses only the blind");
    poker::CareerHand funded(career, std::array{1}, 1000, 0, 0, 12u);
    check(funded.pledge(), "A title can be wagered without adding cash");
    funded.hand.act(poker::Action::CheckCall); // Rival has only $980: uncalled overbet is refunded.
    funded.settle(career);
    check(funded.titleOwner(0) == 0, "Large unmatched title returns physically without forcing debt");
    check(career.garage.size() == 1 && career.profile.money >= 3990,
          "Short cash opponent cannot claim an entire uncalled title for free");
}

OJ_UNIT_TEST(PokerCareer, RandomizedAssetConservation) {
    CarDef playerDef, rivalDef;
    playerDef.id = 1;
    playerDef.value = 2400;
    playerDef.model = "PLAYER";
    rivalDef.id = 2;
    rivalDef.value = 1700;
    rivalDef.model = "RIVAL";
    Car player, rival;
    player.def = &playerDef;
    player.setModel(playerDef.model);
    rival.def = &rivalDef;
    rival.setModel(rivalDef.model);
    std::mt19937 rng(453);
    bool conserved = true, transfer = false;
    for (int n = 0; n < 400; ++n) {
        Career career;
        career.profile.money = 5000;
        career.garage = {player};
        career.opponentCars = {rival};
        poker::CareerHand game(career, std::array{1, 2, 3, 4}, 1000, n % 5, 0, rng());
        int turns = 0;
        while (!game.hand.finished() && ++turns < 200) {
            const int who = game.hand.actor();
            const auto &seat = game.hand.seats()[who];
            const int cash = std::min(seat.chips, std::max(0, game.hand.toCall() - game.titleValue(who)));
            if (rng() % 3 == 0 && game.canPledge(cash))
                game.pledge(cash);
            else if (rng() % 7 == 0)
                game.hand.act(poker::Action::Fold);
            else
                game.hand.act(poker::Action::CheckCall);
        }
        if (!game.hand.finished()) {
            conserved = false;
            break;
        }
        int expectedTotal = 5000, actualTotal = 0;
        for (int i = 0; i < 5; ++i) {
            if (game.pledged(i))
                expectedTotal += game.titleValue(i);
            actualTotal += game.hand.seats()[i].chips;
        }
        conserved &= expectedTotal == actualTotal;
        game.settle(career);
        int wealth = career.profile.money;
        for (const auto &car : career.garage)
            wealth += car.def->value;
        conserved &= wealth - 7400 == game.equityDelta(0) && career.profile.money >= 0;
        for (int i = 0; i < 5; ++i) {
            transfer |= game.titleOwner(i) >= 0 && game.titleOwner(i) != i;
            if (game.titleOwner(i) >= 0) {
                const auto &recipient = game.hand.seats()[game.titleOwner(i)];
                conserved &= !recipient.folded || game.titleOwner(i) == i;
            }
        }
    }
    check(conserved, "400 mixed-asset hands conserve cash plus car value and never incur debt");
    check(transfer, "Randomized hands exercise physical title transfers");
}

OJ_UNIT_TEST(PokerCareer, TitleCallChange) {
    CarDef def;
    def.id = 1;
    def.value = 10000;
    def.model = "TEST";
    Car car;
    car.def = &def;
    car.setModel(def.model);
    Career career;
    career.profile.money = 5000;
    career.garage = {car};
    career.opponentCars = {car};
    poker::CareerHand game(career, std::array{1, 2}, 1000, 0, 0, 32u);
    check(game.pledge(), "First player bets title at face value");
    check(game.pledgeWager() == 9990 && game.pledge(),
          "Equal rival title calls with $10 change rather than illegal under-raise");
    check(game.hand.seats()[1].chips == 1000, "Unused title value is retained as table chips");
    while (!game.hand.finished())
        game.hand.act(poker::Action::CheckCall);
    game.settle(career);
    int wealth = career.profile.money;
    for (const auto &c : career.garage)
        wealth += c.def->value;
    check(wealth - 15000 == game.equityDelta(0), "Title change does not duplicate career wealth");
}

OJ_UNIT_TEST(PokerCareer, IndivisibleTitleSplitPaysCash) {
    CarDef expensive, cheap;
    expensive.id = 1;
    expensive.value = 10000;
    expensive.model = "EXPENSIVE";
    cheap.id = 2;
    cheap.value = 5000;
    cheap.model = "CHEAP";
    Car player, rival;
    player.def = &expensive;
    player.setModel(expensive.model);
    rival.def = &cheap;
    rival.setModel(cheap.model);
    Career career;
    career.profile.money = 5000;
    career.garage = {player};
    career.opponentCars = {rival};
    poker::CareerHand game(career, std::array{1, 2}, 1000, 0, 0, 32u);
    // Seats 1 and 2 tie with ace high; seat 0 has KQ on a 2-4-6-8-10 board.
    std::vector<poker::Card> prefix{12, 25, 11, 14, 27, 10, 3, 0, 15, 30, 5, 45, 7, 8};
    std::array<poker::Card, 52> deck{};
    size_t at = 0;
    for (auto c : prefix)
        deck[at++] = c;
    for (int c = 0; c < 52; ++c)
        if (std::find(prefix.begin(), prefix.end(), c) == prefix.end())
            deck[at++] = c;
    game.hand = poker::Hand(std::array{1000, 1000, 1000}, 0, 20, deck);
    check(game.pledge() && game.pledge(990) && game.pledge(980), "Mixed-value titles support short all-in calls");
    check(game.hand.finished(), "All-in titles run out board with correct side pots");
    game.settle(career);
    check(game.titleOwner(0) == -1, "A car larger than every eligible award liquidates instead of creating debt");
    check(game.titleOwner(1) == 1 && game.titleOwner(2) == 2, "Split winners retain affordable physical titles");
    check(career.garage.empty() && career.profile.money == 9000 && game.equityDelta(0) == -6000,
          "Uncalled title remainder returns as cash while tied opponents share its matched value");
}
