#include "Core/Gamble/CareerPoker.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Cars/CarRepair.h"
#include "Core/Race/RaceSession.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace oj::poker {
namespace {
std::vector<int> stacks(size_t count, int buyIn) {
    if (count < 1 || count > 4 || buyIn < 100 || buyIn > 1000000)
        throw std::invalid_argument("Choose one to four rivals and a buy-in from $100 to $1,000,000.");
    return std::vector<int>(count + 1, buyIn);
}
} // namespace
CareerHand::CareerHand(Career &career, std::span<const int> rivals, int cash, int dealer,
                       std::optional<size_t> titleCar, uint32_t seed, const PartCatalogue *catalogue)
    : hand(stacks(rivals.size(), cash), dealer, std::max(2, cash / 50), seed), buyIn(cash),
      m_cash(career.profile.money), m_garageSize(career.garage.size()), m_title(titleCar) {
    if (career.profile.racing || career.profile.introduction)
        throw std::runtime_error("Finish your current career event before joining the table.");
    if (m_cash < buyIn)
        throw std::runtime_error("Not enough cash for this buy-in.");
    if (static_cast<int64_t>(m_cash) + buyIn * rivals.size() > std::numeric_limits<int32_t>::max())
        throw std::runtime_error("Your balance is too high for this table.");
    crews.push_back(0);
    for (int crew : rivals) {
        if (crew < 1 || crew > 8 || std::find(crews.begin(), crews.end(), crew) != crews.end())
            throw std::invalid_argument("Invalid or repeated opponent.");
        crews.push_back(crew);
    }
    if (!m_title)
        return;
    if (*m_title >= career.garage.size())
        throw std::runtime_error("Choose a car from your garage.");
    if (career.garage.size() + rivals.size() > 55)
        throw std::runtime_error("Make room in your garage for every car you could win.");
    m_cars.push_back(career.garage[*m_title]);
    for (int crew : rivals) {
        // The retail opponent garage is a shared pool, not a per-crew inventory.
        // leaderCar() addresses the last race session and is invalid at a poker table.
        const auto &owner = career.crews[crew];
        const Car *selected = nullptr;
        int64_t best = std::numeric_limits<int64_t>::max();
        for (const auto &car : career.opponentCars) {
            if (!car.def)
                continue;
            int64_t score =
                std::abs(static_cast<int64_t>(car.def->value) - (m_cars[0].def ? m_cars[0].def->value : 10000));
            if (owner.preferredMake && car.def->make != owner.preferredMake)
                score += 10000000;
            if (owner.preferredRegion && car.def->region != owner.preferredRegion)
                score += 1000000;
            for (size_t n = 1; n < m_cars.size(); ++n)
                if (m_cars[n].def == car.def)
                    score += 100000;
            if (score < best) {
                best = score;
                selected = &car;
            }
        }
        if (!selected)
            throw std::runtime_error("One of the rivals has no car to stake.");
        m_cars.push_back(*selected);
        auto &car = m_cars.back();
        car.base = owner.baseColour;
        car.metallic = owner.metallicColour;
        car.pearl = owner.pearlColour;
    }
    for (const auto &car : m_cars) {
        if (!car.def)
            throw std::runtime_error("A staked car is missing its definition.");
        const int value = catalogue ? saleWorth(car, *catalogue) : car.def->value;
        if (value <= 0 || value > 10000000)
            throw std::runtime_error("A title has an invalid cash value.");
        m_values.push_back(value);
        m_pledgeStarts.push_back(-1);
        m_owners.push_back(-2);
        m_originalCars.emplace_back();
        car.write(m_originalCars.back());
    }
    int64_t maximum = m_cash + static_cast<int64_t>(buyIn) * rivals.size();
    for (int value : m_values)
        maximum += value;
    if (maximum > std::numeric_limits<int32_t>::max())
        throw std::runtime_error("Your balance is too high for these title stakes.");
}

int CareerHand::titleValue(int seat) const {
    return seat >= 0 && seat < static_cast<int>(m_values.size()) ? m_values[seat] : 0;
}
bool CareerHand::pledged(int seat) const {
    return seat >= 0 && seat < static_cast<int>(m_pledgeStarts.size()) && m_pledgeStarts[seat] >= 0;
}
bool CareerHand::titleAvailable(int seat) const {
    return titleValue(seat) > 0 && !pledged(seat);
}
bool CareerHand::tryPledge(Hand &candidate, int cash) const {
    const int actor = candidate.actor();
    if (!titleAvailable(actor) || m_settled || candidate.finished() || cash < 0)
        return false;
    const auto &seat = candidate.seats()[actor];
    if (cash > seat.chips)
        return false;
    const int wager = titleValue(actor) + cash;
    if (wager < candidate.toCall() && cash != seat.chips)
        return false;
    const int call = candidate.toCall();
    if (candidate.fundedAction(titleValue(actor), wager > call ? Action::Raise : Action::CheckCall, seat.round + wager))
        return true;
    // A title slightly above the call must not force an illegal under-raise.
    // Redeem it to call, leaving its unused value as change in the table stack.
    return cash == 0 && call > 0 && titleValue(actor) >= call &&
           candidate.fundedAction(titleValue(actor), Action::CheckCall);
}
bool CareerHand::canPledge(int cash) const {
    Hand candidate = hand;
    return tryPledge(candidate, cash);
}
int CareerHand::pledgeWager(int cash) const {
    Hand candidate = hand;
    const int actor = hand.actor();
    if (actor < 0 || !tryPledge(candidate, cash))
        return -1;
    return candidate.seats()[actor].committed - hand.seats()[actor].committed;
}
bool CareerHand::pledge(int cash) {
    const int actor = hand.actor();
    if (actor < 0)
        return false;
    const int start = hand.seats()[actor].committed;
    if (!tryPledge(hand, cash))
        return false;
    m_pledgeStarts[actor] = start;
    return true;
}
int CareerHand::titleOwner(int seat) const {
    return seat >= 0 && seat < static_cast<int>(m_owners.size()) ? m_owners[seat] : -2;
}
int CareerHand::equityDelta(int seat) const {
    return hand.seats()[seat].chips - buyIn - (pledged(seat) ? titleValue(seat) : 0);
}
int CareerHand::carsWon() const {
    return static_cast<int>(std::count(m_owners.begin() + (m_owners.empty() ? 0 : 1), m_owners.end(), 0));
}
bool CareerHand::carLost() const {
    return m_settled && pledged(0) && titleOwner(0) != 0;
}

void CareerHand::settle(Career &career) {
    if (m_settled)
        throw std::logic_error("This hand has already been settled.");
    if (!hand.finished())
        throw std::logic_error("Finish the hand before settling.");
    if (career.profile.money != m_cash || career.garage.size() != m_garageSize)
        throw std::runtime_error("Career changed during this hand; no poker payment was applied.");
    if (pledged(0)) {
        std::vector<uint8_t> current;
        career.garage[*m_title].write(current);
        if (current != m_originalCars[0])
            throw std::runtime_error("The staked car changed; payment cancelled.");
    }
    // Cars replace monetary entitlements; they never add free value or create debt.
    std::vector<int> balances, owners(m_cars.size(), -2), order;
    std::vector<std::vector<int>> shares(m_cars.size());
    for (const auto &seat : hand.seats())
        balances.push_back(seat.chips);
    for (int i = 0; i < static_cast<int>(m_cars.size()); ++i) {
        if (!pledged(i))
            continue;
        const int contributed = std::min(m_values[i], hand.seats()[i].committed - m_pledgeStarts[i]);
        shares[i] = hand.contributionAwards(i, m_pledgeStarts[i], contributed);
        shares[i][i] += m_values[i] - contributed;
        if (shares[i][i] == m_values[i] && balances[i] >= m_values[i]) {
            owners[i] = i;
            balances[i] -= m_values[i];
        } else
            order.push_back(i);
    }
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return m_values[a] > m_values[b]; });
    for (int i : order) {
        int recipient = -1;
        for (int n = 0; n < static_cast<int>(balances.size()); ++n) {
            const int seat = (hand.dealer() + 1 + n) % static_cast<int>(balances.size());
            if (shares[i][seat] > 0 && balances[seat] >= m_values[i] &&
                (recipient < 0 || shares[i][seat] > shares[i][recipient] ||
                 (shares[i][seat] == shares[i][recipient] && seat == i)))
                recipient = seat;
        }
        owners[i] = recipient;
        if (recipient >= 0)
            balances[recipient] -= m_values[i];
        // Otherwise liquidate this indivisible title; every entitlement stays cash.
    }
    std::vector<Car> garage = career.garage;
    const bool lost = pledged(0) && owners[0] != 0;
    if (lost)
        garage.erase(garage.begin() + static_cast<ptrdiff_t>(*m_title));
    for (size_t i = 1; i < owners.size(); ++i)
        if (owners[i] == 0)
            garage.push_back(m_cars[i]);
    if (garage.size() > 55)
        throw std::runtime_error("No room for poker prizes; settlement cancelled.");
    const int delta = balances[0] - buyIn;
    const int64_t newMoney = static_cast<int64_t>(m_cash) + delta;
    if (newMoney < 0 || newMoney > std::numeric_limits<int32_t>::max())
        throw std::runtime_error("Invalid poker balance; settlement cancelled.");
    if (lost || garage.size() != career.garage.size())
        career.garage.swap(garage);
    pay(career, -delta);
    m_cashDelta = delta;
    m_owners = std::move(owners);
    if (lost) {
        // Dealer/race selection indexes must be rebuilt after removing a garage car.
        career.profile.raceCarKept = 0;
        career.profile.raceCarsChosen = 0;
        career.profile.raceCarCount = 0;
        career.profile.raceChoiceCount = 0;
        career.profile.raceCars.fill(0);
        career.profile.raceChoices.fill(0);
    }
    m_settled = true;
}
} // namespace oj::poker
