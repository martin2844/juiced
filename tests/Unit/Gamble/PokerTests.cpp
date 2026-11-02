#include "Core/Gamble/Poker.h"
#include "Framework/Test.h"

#include <algorithm>
#include <numeric>
#include <random>

using oj::test::check;
using namespace oj::poker;

OJ_UNIT_TEST(Poker, Rankings) {
    const auto rank = [](std::initializer_list<Card> cards) {
        return evaluate(std::span(cards.begin(), cards.size()));
    };
    check((rank({12, 0, 1, 2, 3}) >> 20) == 8, "Ace-low straight flush");
    check(rank({12, 0, 1, 2, 3}) < rank({0, 1, 2, 3, 4}), "Six high beats wheel");
    check((rank({12, 25, 38, 11, 24, 37, 0}) >> 20) == 6, "Two trips make a full house");
    check(rank({12, 25, 38, 11, 24, 0, 1}) > rank({11, 24, 37, 12, 25, 0, 1}), "Full house trips outrank pair");
    check(rank({12, 25, 38, 51, 11}) > rank({12, 25, 38, 51, 10}), "Quads kicker decides");
    check(rank({12, 25, 11, 24, 10}) > rank({12, 25, 11, 24, 9}), "Two pair kicker decides");
    bool rejected = false;
    try {
        rank({0, 0, 1, 2, 3});
    } catch (...) {
        rejected = true;
    }
    check(rejected, "Duplicate cards rejected");
}

OJ_UNIT_TEST(Poker, HeadsUpAndInvalidActions) {
    Hand h(std::array{1000, 1000}, 0, 20, 10u);
    check(h.actor() == 0 && h.toCall() == 10, "Heads-up dealer posts small blind and acts first");
    check(!h.act(Action::Raise, 25) && h.pot() == 30, "Under-minimum raise rejected without mutation");
    check(!h.act(Action::Raise, 2000), "Over-stack raise rejected");
    h.act(Action::CheckCall);
    check(h.actor() == 1 && h.toCall() == 0, "Big blind retains option");
    h.act(Action::CheckCall);
    check(h.street() == Street::Flop && h.actor() == 1, "Heads-up nondealer acts first postflop");
    while (!h.finished())
        h.act(Action::CheckCall);
    check(h.seats()[0].chips + h.seats()[1].chips == 2000, "Showdown conserves chips");
    check(!h.act(Action::CheckCall), "Settled hand cannot pay twice");
}

OJ_UNIT_TEST(Poker, ShortAllIn) {
    Hand h(std::array{1000, 150, 1000}, 0, 20, 20u);
    check(h.actor() == 0, "Three-handed starts left of big blind");
    h.act(Action::Raise, 100);
    h.act(Action::Raise, 150);
    h.act(Action::CheckCall);
    check(h.actor() == 0 && h.toCall() == 50, "Short all-in still requires earlier raiser to call");
    check(!h.act(Action::Raise, 300), "Short all-in does not reopen raising for earlier actor");
    h.act(Action::CheckCall);
    check(h.street() == Street::Flop, "Round completes after short all-in called");
}

OJ_UNIT_TEST(Poker, CumulativeShortRaisesReopen) {
    Hand h(std::array{1000, 150, 200, 1000}, 0, 20, 12u);
    check(h.actor() == 3, "Four-handed first action");
    h.act(Action::Raise, 100); // full raise by 80
    h.act(Action::CheckCall);  // dealer calls 100
    h.act(Action::Raise, 150); // short by 50
    h.act(Action::Raise, 200); // short by another 50
    check(h.actor() == 3 && h.seats()[3].mayRaise, "Two short raises totaling a full raise reopen action");
    check(h.act(Action::Raise, 280), "Reopened raiser may raise by the last full increment");
}

OJ_UNIT_TEST(Poker, BoardTieAndOddChip) {
    std::array<Card, 52> deck{};
    std::vector<Card> prefix = {0, 1, 2, 3, 4, 5, 6, 47, 48, 49, 7, 50, 8, 51};
    size_t at = 0;
    for (Card c : prefix)
        deck[at++] = c;
    for (int c = 0; c < 52; ++c)
        if (std::find(prefix.begin(), prefix.end(), c) == prefix.end())
            deck[at++] = c;
    Hand h(std::array{100, 100, 100}, 0, 11, deck);
    h.act(Action::CheckCall);
    h.act(Action::Fold);
    while (!h.finished())
        h.act(Action::CheckCall);
    check(h.pot() == 27, "Odd pot contains folded small blind");
    check(h.seats()[0].payout == 13 && h.seats()[2].payout == 14,
          "Board royal flush splits pot; odd chip goes left of dealer");
}

OJ_UNIT_TEST(Poker, SidePotsAndFolds) {
    Hand h(std::array{50, 100, 200}, 0, 20, 90u);
    h.act(Action::Raise, 50);
    h.act(Action::Raise, 100);
    h.act(Action::Raise, 200);
    check(h.finished() && h.board().size() == 5, "All-ins run out board");
    check(h.seats()[0].payout <= 150 && h.seats()[1].payout <= 250, "Short stacks cannot win unmatched layers");
    check(h.seats()[2].payout >= 100, "Uncalled overbet returned");
    int sum = 0;
    for (const auto &s : h.seats())
        sum += s.chips;
    check(sum == 350, "Side pots conserve every chip");
    Hand f(std::array{1000, 1000, 1000}, 0, 20, 1u);
    f.act(Action::Fold);
    f.act(Action::Fold);
    check(f.finished() && f.board().empty(), "Last player wins without showing board");
    check(f.seats()[2].chips == 1010, "Fold winner receives pot and own blind");
}

OJ_UNIT_TEST(Poker, RandomHandsConserveMoney) {
    std::mt19937 rng(987654);
    bool valid = true, attributed = true;
    for (int run = 0; run < 2000 && valid; ++run) {
        std::vector<int> stacks(2 + rng() % 4);
        for (int &stack : stacks)
            stack = 1 + rng() % 5000;
        const int total = std::accumulate(stacks.begin(), stacks.end(), 0);
        Hand h(stacks, rng() % stacks.size(), 20, rng());
        int actions = 0;
        while (!h.finished() && ++actions < 500) {
            if (rng() % 9 == 0)
                h.act(Action::Fold);
            else if (rng() % 3 == 0) {
                const auto &seat = h.seats()[h.actor()];
                if (!h.act(Action::Raise, seat.round + seat.chips))
                    h.act(Action::CheckCall);
            } else
                h.act(Action::CheckCall);
        }
        int after = 0;
        for (const auto &seat : h.seats()) {
            after += seat.chips;
            valid = valid && seat.chips >= 0;
        }
        std::vector<int> awards(h.seats().size());
        for (size_t contributor = 0; contributor < h.seats().size(); ++contributor) {
            const int amount = h.seats()[contributor].committed;
            const auto full = h.contributionAwards(static_cast<int>(contributor), 0, amount);
            const auto first = h.contributionAwards(static_cast<int>(contributor), 0, amount / 2);
            const auto second = h.contributionAwards(static_cast<int>(contributor), amount / 2, amount - amount / 2);
            for (size_t winner = 0; winner < awards.size(); ++winner) {
                awards[winner] += full[winner];
                attributed &= first[winner] + second[winner] == full[winner];
            }
        }
        for (size_t winner = 0; winner < awards.size(); ++winner)
            attributed &= awards[winner] == h.seats()[winner].payout;
        valid = valid && h.finished() && after == total;
    }
    check(valid, "2000 seeded multiway hands terminate and conserve money under folds and unequal all-ins");
    check(attributed, "2000 multiway hands preserve per-winner contribution attribution and interval additivity");
}

OJ_UNIT_TEST(Poker, AtomicAssetFunding) {
    Hand h(std::array{1000, 1000}, 0, 20, 10u);
    check(!h.fundedAction(5000, Action::Raise, 25) && h.seats()[0].chips == 990 && h.pot() == 30,
          "Illegal funded raise restores stack and pot");
    check(!h.fundedAction(5000, Action::Fold) && !h.fundedAction(-1, Action::CheckCall),
          "Cannot fund folds or negative title values");
    check(h.fundedAction(5000, Action::Raise, 5510), "Whole title plus cash enters a legal raise");
    h.act(Action::CheckCall);
    check(h.finished(), "Short cash call resolves side pots against funded title");
    const auto share = h.contributionAwards(0, 10, 5000);
    check(std::accumulate(share.begin(), share.end(), 0) == 5000,
          "Title contribution attribution includes its uncalled refund");
    check(share[0] >= 4010, "Short caller has no claim to the unmatched title layer");
    check(!h.fundedAction(5000, Action::CheckCall), "Finished hand cannot introduce another asset");
}

OJ_UNIT_TEST(Poker, ShortOpeningAllInAfterCheck) {
    Hand shortBet(std::array{1000, 1000, 150}, 0, 100, 1u);
    for (int i = 0; i < 3; ++i)
        shortBet.act(Action::CheckCall);
    check(shortBet.street() == Street::Flop && shortBet.actor() == 1, "Short-opening fixture reaches flop");
    shortBet.act(Action::CheckCall); // Seat 1 checks at zero.
    shortBet.act(Action::Raise, 50); // Seat 2 has only $50 left.
    check(shortBet.actor() == 0 && shortBet.seats()[0].mayRaise,
          "An unacted player can raise a short opening all-in");
    Hand fullRaise = shortBet;
    check(fullRaise.act(Action::Raise, 150) && fullRaise.actor() == 1 && fullRaise.seats()[1].mayRaise &&
              fullRaise.act(Action::Raise, 250),
          "A subsequent full raise reopens the prior checker");
    shortBet.act(Action::CheckCall);
    const int pot = shortBet.pot(), stack = shortBet.seats()[1].chips;
    check(shortBet.actor() == 1 && !shortBet.seats()[1].mayRaise,
          "Calling a short opening wager does not reopen the prior checker");
    check(!shortBet.fundedAction(5000, Action::Raise, 150) && shortBet.pot() == pot &&
              shortBet.seats()[1].chips == stack,
          "Title funding cannot bypass closed raising rights");
    check(!shortBet.act(Action::Raise, 150) && shortBet.pot() == pot && shortBet.actor() == 1,
          "Prior checker cannot raise a subminimum opening all-in");
    check(shortBet.act(Action::CheckCall) && shortBet.street() == Street::Turn,
          "Prior checker can call the short opening bet");

    Hand fullBet(std::array{1000, 1000, 1000}, 0, 100, 1u);
    for (int i = 0; i < 3; ++i)
        fullBet.act(Action::CheckCall);
    fullBet.act(Action::CheckCall);
    fullBet.act(Action::Raise, 100);
    fullBet.act(Action::CheckCall);
    check(fullBet.actor() == 1 && fullBet.seats()[1].mayRaise && fullBet.act(Action::Raise, 200),
          "A full opening bet preserves normal check-raising");

    Hand cumulative(std::array{1000, 1000, 150, 200}, 0, 100, 1u);
    for (int i = 0; i < 4; ++i)
        cumulative.act(Action::CheckCall);
    cumulative.act(Action::CheckCall);
    cumulative.act(Action::Raise, 50);
    cumulative.act(Action::Raise, 100);
    cumulative.act(Action::CheckCall);
    check(cumulative.actor() == 1 && cumulative.seats()[1].mayRaise && cumulative.act(Action::Raise, 200),
          "Cumulative short opening all-ins reaching a full bet reopen the checker");
}

OJ_UNIT_TEST(Poker, ContributionAttributionConservesOddChips) {
    std::array<Card, 52> deck{};
    const std::vector<Card> prefix{0, 1, 2, 3, 4, 47, 48, 49, 5, 50, 6, 51};
    size_t at = 0;
    for (Card c : prefix)
        deck[at++] = c;
    for (int c = 0; c < 52; ++c)
        if (std::find(prefix.begin(), prefix.end(), c) == prefix.end())
            deck[at++] = c;
    for (int amount : {10001, 10002}) {
        Hand h(std::array{amount, amount}, 0, 20, deck);
        check(h.act(Action::Raise, amount) && h.act(Action::CheckCall) && h.finished(),
              "Odd-contribution fixture reaches a tied all-in showdown");
        std::vector<int> attributed(2);
        for (int contributor = 0; contributor < 2; ++contributor) {
            const auto full = h.contributionAwards(contributor, 0, amount);
            const int first = amount / 2;
            const auto a = h.contributionAwards(contributor, 0, first);
            const auto b = h.contributionAwards(contributor, first, amount - first);
            bool additive = true;
            for (int winner = 0; winner < 2; ++winner) {
                attributed[winner] += full[winner];
                additive &= a[winner] + b[winner] == full[winner];
            }
            check(additive, "Splitting a contribution interval cannot create a second odd-chip entitlement");
        }
        check(attributed[0] == h.seats()[0].payout && attributed[1] == h.seats()[1].payout,
              "All contribution entitlements add up to each winner's actual payout");
    }
}
