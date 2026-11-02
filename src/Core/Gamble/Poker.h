#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace oj::poker {

// Cards are suit * 13 + rank, with rank 0 = deuce and 12 = ace.
using Card = uint8_t;
uint32_t evaluate(std::span<const Card> cards);
std::string category(uint32_t score);
std::string label(Card card);

enum class Action { Fold, CheckCall, Raise };
enum class Street { Preflop, Flop, Turn, River, Finished };

struct Seat {
    std::array<Card, 2> hole{};
    int chips = 0;
    int committed = 0;
    int round = 0;
    int payout = 0;
    bool folded = false;
    bool acted = false;
    bool mayRaise = true;
    int lastFacedBet = -1; // -1 has not acted; a check records zero.
};

// One no-limit hold'em hand. No career mutations, rendering, or opponent decisions.
// Raises specify the total contribution for this street, not an increment.
class Hand {
  public:
    Hand(std::span<const int> stacks, int dealer, int bigBlind, uint32_t seed);
    // Explicit deck for replay/tests; must be a permutation of all 52 cards.
    Hand(std::span<const int> stacks, int dealer, int bigBlind, std::array<Card, 52> deck);
    bool act(Action action, int raiseTo = 0);
    // Introduce a valued asset and wager it atomically. Invalid actions add no chips.
    bool fundedAction(int value, Action action, int raiseTo = 0);
    // Attribution for a valued asset's interval of a contributor's wager, after settlement.
    std::vector<int> contributionAwards(int contributor, int start, int value) const;
    int toCall() const;
    int minimumRaiseTo() const {
        return m_bet + m_raise;
    }
    int pot() const;
    int actor() const {
        return m_actor;
    }
    int dealer() const {
        return m_dealer;
    }
    Street street() const {
        return m_street;
    }
    const std::vector<Seat> &seats() const {
        return m_seats;
    }
    const std::vector<Card> &board() const {
        return m_board;
    }
    bool finished() const {
        return m_street == Street::Finished;
    }

  private:
    void start(std::span<const int> stacks, int dealer, int blind);
    void pay(int seat, int amount);
    void advance();
    void reveal();
    void settle();
    int next(int seat) const {
        return (seat + 1) % static_cast<int>(m_seats.size());
    }
    std::array<Card, 52> m_deck{};
    size_t m_dealt = 0;
    std::vector<Seat> m_seats;
    std::vector<Card> m_board;
    int m_actor = 0, m_dealer = 0, m_blind = 0, m_bet = 0, m_raise = 0;
    Street m_street = Street::Preflop;
    struct PotAward {
        int start, end;
        std::vector<int> winners;
    };
    std::vector<PotAward> m_awards;
};

} // namespace oj::poker
