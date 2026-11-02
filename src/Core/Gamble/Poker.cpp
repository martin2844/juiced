#include "Core/Gamble/Poker.h"

#include <algorithm>
#include <numeric>
#include <random>
#include <stdexcept>

namespace oj::poker {
namespace {
uint32_t five(const std::array<Card, 5> &cards) {
    std::array<int, 15> counts{};
    bool flush = true;
    for (Card c : cards) {
        ++counts[c % 13 + 2];
        flush = flush && c / 13 == cards[0] / 13;
    }
    int straight = 0;
    for (int high = 14; high >= 5; --high) {
        bool found = true;
        for (int n = 0; n < 5; ++n)
            found = found && counts[high - n == 1 ? 14 : high - n] != 0;
        if (found) {
            straight = high;
            break;
        }
    }
    std::vector<std::pair<int, int>> groups;
    for (int r = 14; r >= 2; --r)
        if (counts[r])
            groups.emplace_back(counts[r], r);
    std::sort(groups.rbegin(), groups.rend());
    const int type = flush && straight                              ? 8
                     : groups[0].first == 4                         ? 7
                     : groups[0].first == 3 && groups[1].first == 2 ? 6
                     : flush                                        ? 5
                     : straight                                     ? 4
                     : groups[0].first == 3                         ? 3
                     : groups[0].first == 2 && groups[1].first == 2 ? 2
                     : groups[0].first == 2                         ? 1
                                                                    : 0;
    uint32_t score = static_cast<uint32_t>(type) << 20;
    if (straight && (type == 4 || type == 8))
        return score | (straight << 16);
    int shift = 16;
    for (const auto &[count, rank] : groups) {
        score |= static_cast<uint32_t>(rank) << shift;
        shift -= 4;
    }
    return score;
}
} // namespace

uint32_t evaluate(std::span<const Card> cards) {
    if (cards.size() < 5 || cards.size() > 7)
        throw std::invalid_argument("Poker needs five to seven cards");
    uint64_t seen = 0;
    for (Card c : cards) {
        if (c >= 52 || (seen & (uint64_t{1} << c)))
            throw std::invalid_argument("Invalid or duplicate card");
        seen |= uint64_t{1} << c;
    }
    uint32_t best = 0;
    for (size_t a = 0; a + 4 < cards.size(); ++a)
        for (size_t b = a + 1; b + 3 < cards.size(); ++b)
            for (size_t c = b + 1; c + 2 < cards.size(); ++c)
                for (size_t d = c + 1; d + 1 < cards.size(); ++d)
                    for (size_t e = d + 1; e < cards.size(); ++e)
                        best = std::max(best, five({cards[a], cards[b], cards[c], cards[d], cards[e]}));
    return best;
}

std::string category(uint32_t score) {
    static const char *names[] = {"High card", "Pair",       "Two pair",       "Three of a kind", "Straight",
                                  "Flush",     "Full house", "Four of a kind", "Straight flush"};
    return names[std::min(score >> 20, 8u)];
}

std::string label(Card c) {
    if (c >= 52)
        return "?";
    static const char *ranks[] = {"2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"};
    return std::string(ranks[c % 13]) + "CDHS"[c / 13];
}

Hand::Hand(std::span<const int> stacks, int dealer, int blind, uint32_t seed) {
    std::iota(m_deck.begin(), m_deck.end(), Card{0});
    std::mt19937 rng(seed);
    std::shuffle(m_deck.begin(), m_deck.end(), rng);
    start(stacks, dealer, blind);
}
Hand::Hand(std::span<const int> stacks, int dealer, int blind, std::array<Card, 52> deck) : m_deck(deck) {
    auto sorted = deck;
    std::sort(sorted.begin(), sorted.end());
    for (size_t i = 0; i < sorted.size(); ++i)
        if (sorted[i] != i)
            throw std::invalid_argument("Deck is not a permutation");
    start(stacks, dealer, blind);
}
void Hand::start(std::span<const int> stacks, int dealer, int blind) {
    if (stacks.size() < 2 || stacks.size() > 5 || dealer < 0 || dealer >= static_cast<int>(stacks.size()) ||
        blind < 2 || blind > 1000000)
        throw std::invalid_argument("Invalid table");
    for (int stack : stacks) {
        if (stack <= 0 || stack > 100000000)
            throw std::invalid_argument("Invalid stack");
        Seat s;
        s.chips = stack;
        m_seats.push_back(s);
    }
    m_dealer = dealer;
    m_blind = m_raise = blind;
    for (int round = 0; round < 2; ++round) {
        int seat = next(dealer);
        for (size_t n = 0; n < stacks.size(); ++n, seat = next(seat))
            m_seats[seat].hole[round] = m_deck[m_dealt++];
    }
    const int small = stacks.size() == 2 ? dealer : next(dealer);
    const int big = next(small);
    pay(small, blind / 2);
    pay(big, blind);
    m_bet = blind;
    m_actor = big;
    advance();
}
void Hand::pay(int seat, int amount) {
    Seat &s = m_seats[seat];
    amount = std::min(amount, s.chips);
    s.chips -= amount;
    s.round += amount;
    s.committed += amount;
}
int Hand::toCall() const {
    return finished() ? 0 : std::max(0, m_bet - m_seats[m_actor].round);
}
int Hand::pot() const {
    int value = 0;
    for (const auto &s : m_seats)
        value += s.committed;
    return value;
}
bool Hand::act(Action action, int raiseTo) {
    if (finished())
        return false;
    Seat &s = m_seats[m_actor];
    if (action == Action::Raise) {
        const int max = s.round + s.chips;
        if (!s.mayRaise || raiseTo <= m_bet || raiseTo > max || (raiseTo < minimumRaiseTo() && raiseTo != max))
            return false;
        const int increase = raiseTo - m_bet;
        if (increase >= m_raise) {
            m_raise = increase;
            for (auto &other : m_seats) {
                other.acted = false;
                other.mayRaise = true;
            }
        }
        pay(m_actor, raiseTo - s.round);
        m_bet = raiseTo;
        for (auto &other : m_seats)
            if (other.lastFacedBet < 0 || m_bet - other.lastFacedBet >= m_raise)
                other.mayRaise = true;
    } else if (action == Action::CheckCall) {
        pay(m_actor, toCall());
    } else if (action == Action::Fold) {
        s.folded = true;
    } else
        return false;
    s.acted = true;
    // Checking is an action at zero. A later short opening all-in only
    // reopens this seat when a full bet (possibly cumulative) is faced.
    s.lastFacedBet = m_bet;
    s.mayRaise = false;
    advance();
    return true;
}
bool Hand::fundedAction(int value, Action action, int raiseTo) {
    if (finished() || value <= 0 || value > 10000000 || action == Action::Fold)
        return false;
    const auto &seat = m_seats[m_actor];
    if (seat.chips + seat.committed > 100000000 - value)
        return false;
    Hand next = *this;
    next.m_seats[m_actor].chips += value;
    if (!next.act(action, raiseTo))
        return false;
    *this = std::move(next);
    return true;
}
void Hand::advance() {
    for (;;) {
        int alive = 0, withChips = 0;
        for (const auto &s : m_seats)
            if (!s.folded) {
                ++alive;
                if (s.chips)
                    ++withChips;
            }
        if (alive == 1) {
            settle();
            return;
        }
        for (size_t n = 0; n < m_seats.size(); ++n) {
            m_actor = next(m_actor);
            const Seat &s = m_seats[m_actor];
            if (!s.folded && s.chips && (s.round < m_bet || (!s.acted && withChips > 1)))
                return;
        }
        if (m_street == Street::River) {
            settle();
            return;
        }
        reveal();
        for (auto &s : m_seats) {
            s.round = 0;
            s.acted = false;
            s.mayRaise = true;
            s.lastFacedBet = -1;
        }
        m_bet = 0;
        m_raise = m_blind;
        m_actor = m_dealer;
    }
}
void Hand::reveal() {
    ++m_dealt; // Burn one card before each street.
    const int count = m_street == Street::Preflop ? 3 : 1;
    for (int i = 0; i < count; ++i)
        m_board.push_back(m_deck[m_dealt++]);
    m_street = static_cast<Street>(static_cast<int>(m_street) + 1);
}
void Hand::settle() {
    std::vector<int> levels;
    for (const auto &s : m_seats)
        if (s.committed)
            levels.push_back(s.committed);
    std::sort(levels.begin(), levels.end());
    levels.erase(std::unique(levels.begin(), levels.end()), levels.end());
    int previous = 0;
    for (int level : levels) {
        int value = 0;
        std::vector<int> winners;
        uint32_t best = 0;
        for (size_t i = 0; i < m_seats.size(); ++i) {
            const Seat &s = m_seats[i];
            if (s.committed < level)
                continue;
            value += level - previous;
            if (s.folded)
                continue;
            uint32_t rank = 0;
            if (m_board.size() == 5) {
                auto cards = m_board;
                cards.insert(cards.end(), s.hole.begin(), s.hole.end());
                rank = evaluate(cards);
            }
            if (winners.empty() || rank > best) {
                best = rank;
                winners.clear();
            }
            if (rank == best)
                winners.push_back(static_cast<int>(i));
        }
        // A layer with no live claimant is an unmatched folded overbet. Refund its contributors.
        m_awards.push_back({previous, level, winners});
        if (winners.empty()) {
            for (auto &s : m_seats)
                if (s.committed >= level)
                    s.payout += level - previous;
        } else {
            for (int seat : winners)
                m_seats[seat].payout += value / static_cast<int>(winners.size());
            int odd = value % static_cast<int>(winners.size());
            int seat = next(m_dealer);
            while (odd) {
                if (std::find(winners.begin(), winners.end(), seat) != winners.end()) {
                    ++m_seats[seat].payout;
                    --odd;
                }
                seat = next(seat);
            }
        }
        previous = level;
    }
    for (auto &s : m_seats)
        s.chips += s.payout;
    m_street = Street::Finished;
    m_actor = -1;
}
std::vector<int> Hand::contributionAwards(int contributor, int start, int value) const {
    std::vector<int> result(m_seats.size());
    if (!finished() || contributor < 0 || contributor >= static_cast<int>(m_seats.size()) || start < 0 || value <= 0 ||
        static_cast<int64_t>(start) + value > m_seats[contributor].committed)
        return result;
    for (const auto &pot : m_awards) {
        const int overlap = std::min(start + value, pot.end) - std::max(start, pot.start);
        if (overlap <= 0)
            continue;
        if (pot.winners.empty()) {
            result[contributor] += overlap;
            continue;
        }
        for (int winner : pot.winners)
            result[winner] += overlap / static_cast<int>(pot.winners.size());
        // Attribute one continuous chip stream per pot, in contributor-seat
        // order. Restarting the odd-chip order for each title/interval would
        // give its first winner more entitlement than the pot actually paid.
        int offset = std::max(start, pot.start) - pot.start;
        for (int seat = 0; seat < contributor; ++seat)
            if (m_seats[seat].committed >= pot.end)
                offset += pot.end - pot.start;
        int skip = offset % static_cast<int>(pot.winners.size());
        int odd = overlap % static_cast<int>(pot.winners.size());
        for (int seat = next(m_dealer); odd; seat = next(seat))
            if (std::find(pot.winners.begin(), pot.winners.end(), seat) != pot.winners.end()) {
                if (skip)
                    --skip;
                else {
                    ++result[seat];
                    --odd;
                }
            }
    }
    return result;
}
} // namespace oj::poker
