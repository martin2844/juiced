#pragma once

#include "Core/Career/Career.h"
#include "Core/Gamble/Poker.h"
#include <optional>

namespace oj::poker {

// Career funds stay untouched while a hand is being played. Settlement is once-only;
// the caller then uses the normal career autosave. No new fields in legacy saves.
class CareerHand {
  public:
    CareerHand(Career &career, std::span<const int> crews, int buyIn, int dealer, std::optional<size_t> titleCar,
               uint32_t seed, const PartCatalogue *catalogue = nullptr);
    Hand hand;
    std::vector<int> crews; // seat zero is the player
    int buyIn;
    bool titles() const {
        return m_title.has_value();
    }
    const std::vector<Car> &stakes() const {
        return m_cars;
    }
    bool settled() const {
        return m_settled;
    }
    int titleValue(int seat) const;
    bool titleAvailable(int seat) const;
    bool canPledge(int cash = 0) const;
    int pledgeWager(int cash = 0) const;
    bool pledge(int cash = 0); // Wager the whole title plus this much cash, on the current turn.
    bool pledged(int seat) const;
    int titleOwner(int seat) const; // -2: not wagered; -1: liquidated for a split; otherwise recipient seat.
    int equityDelta(int seat) const;
    int cashDelta() const {
        return m_cashDelta;
    }
    int carsWon() const;
    bool carLost() const;
    void settle(Career &career);

  private:
    bool tryPledge(Hand &, int cash) const;
    std::vector<int> m_values, m_pledgeStarts, m_owners;
    int m_cashDelta = 0;
    int m_cash;
    size_t m_garageSize;
    std::optional<size_t> m_title;
    std::vector<Car> m_cars;
    std::vector<std::vector<uint8_t>> m_originalCars;
    bool m_settled = false;
};
} // namespace oj::poker
