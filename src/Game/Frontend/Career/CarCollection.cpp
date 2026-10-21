// CMSStatsCarCollection: the garage's cars by a filter.

#include "Game/Frontend/Career/CarCollection.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

namespace {

// A car's races counted, as the window shows them.
int16_t carCounter(const Car& car, CarRaces k) {
    return static_cast<int16_t>(car.races(k));
}

}  // namespace

// 0x004E1460: the cars' order (+0x10 ..) the garage's, then sorted by the filter (+0x0C), the greatest first (a swap
// starts the pass again).
void CarCollection::sort(CareerHost& host) {
    GameData* data = host.gameData();
    const Career& career = *data->career;
    const PartCatalogue& catalogue = data->catalogue;
    const size_t n = career.garage.size();
    if (n == 0) return;
    m_order.resize(n);
    for (size_t i = 0; i < n; ++i) m_order[i] = static_cast<int32_t>(i);
    const auto f = [](uint32_t bits) { return std::bit_cast<float>(bits); };
    for (size_t i = 0; i + 1 < n; ++i) {
        const Car& a = career.garage[static_cast<size_t>(m_order[i])];
        const Car& b = career.garage[static_cast<size_t>(m_order[i + 1])];
        bool swap = false;
        switch (m_filter) {
        case CarFilter::Rating: swap = f(a.rating) < f(b.rating); break;
        case CarFilter::SaleWorth: swap = saleWorth(a, catalogue) < saleWorth(b, catalogue); break;
        case CarFilter::Spent: swap = f(a.spent) < f(b.spent); break;
        case CarFilter::Won: swap = f(a.winnings) < f(b.winnings); break;
        case CarFilter::Profit: swap = f(b.winnings) - f(b.spent) > f(a.winnings) - f(a.spent); break;
        case CarFilter::Races: swap = carCounter(a, CarRaces::Races) < carCounter(b, CarRaces::Races); break;
        case CarFilter::Circuits: swap = carCounter(a, CarRaces::Circuits) < carCounter(b, CarRaces::Circuits); break;
        case CarFilter::Sprints: swap = carCounter(a, CarRaces::Sprints) < carCounter(b, CarRaces::Sprints); break;
        case CarFilter::Showoffs: swap = carCounter(a, CarRaces::Showoffs) < carCounter(b, CarRaces::Showoffs); break;
        case CarFilter::PinkSlips:
            swap = carCounter(a, CarRaces::PinkSlips) < carCounter(b, CarRaces::PinkSlips);
            break;
        default: break;
        }
        if (swap) {
            std::swap(m_order[i], m_order[i + 1]);
            i = static_cast<size_t>(-1);
        }
    }
}

// 0x004E0E70: the car kept highlighted (+0xEC: the first of the order at first, else CarMenu's highlighted item); an
// empty garage empties CarMenu and DataMenu. DataTitle the filter's line; CarMenu the cars in order (the display name
// and " (<rating>)", the value the garage index) with the kept car highlighted; DataMenu the filter's figure of each:
// 0 "<class + 1> (<rating> bhp)", 1 the sale worth, 2 spent, 3 won, 4 won less spent (money), 5 races (wins), 6
// circuit races (wins), 7 sprints (wins), 8 showoffs, 9 pink slips (thousands marked), the kept car's place highlighted
// (its help into the menu's help ticker +0x184: none in this window).
void CarCollection::lists(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Career& career = *data->career;
    const int language = host.language().index;
    if (s.menuSize("CarMenu") == 0) return;
    if (m_kept == -1) {
        m_kept = m_order.empty() ? 0 : m_order[0];
    } else {
        m_kept = x87::chop(s.menuItemValue("CarMenu", s.selectedMenuItem("CarMenu")));
    }
    if (career.garage.empty()) {
        s.clearMenu("CarMenu");
        s.clearMenu("DataMenu");
        return;
    }
    s.setText("DataTitle",
              texts.at(kTextCarFilters + static_cast<size_t>(std::clamp(static_cast<int32_t>(m_filter), 0, 9))));
    s.clearMenu("CarMenu");
    for (size_t i = 0; i < m_order.size(); ++i) {
        const Car& car = career.garage[static_cast<size_t>(m_order[i])];
        std::wstring text = car.def != nullptr ? widenSigned(car.def->displayName) : std::wstring();
        text += toWide(std::format(" ({})", x87::chop(std::bit_cast<float>(car.rating))));
        s.addMenuItem("CarMenu", text, static_cast<float>(m_order[i]));
        if (m_order[i] == m_kept) s.selectMenuItem("CarMenu", i);
    }
    s.clearMenu("DataMenu");
    const auto number = [&](int value) { return toWide(groupThousands(value, language)); };
    const auto money = [&](int value) { return toWide(moneyText(value, language)); };
    const auto f = [](uint32_t bits) { return std::bit_cast<float>(bits); };
    for (size_t i = 0; i < m_order.size(); ++i) {
        const Car& car = career.garage[static_cast<size_t>(m_order[i])];
        std::wstring text;
        switch (m_filter) {
        case CarFilter::Rating:
            text = toWide(std::format("{} ({} ", carClass(car) + 1, x87::chop(f(car.rating)))) +
                   texts.at(kTextBhp) + L")";
            break;
        case CarFilter::SaleWorth: text = money(saleWorth(car, data->catalogue)); break;
        case CarFilter::Spent: text = money(x87::chop(f(car.spent))); break;
        case CarFilter::Won: text = money(x87::chop(f(car.winnings))); break;
        case CarFilter::Profit: text = money(x87::chop(f(car.winnings)) - x87::chop(f(car.spent))); break;
        case CarFilter::Races:
            text = number(carCounter(car, CarRaces::Races)) + L" (" + number(carCounter(car, CarRaces::Wins)) + L")";
            break;
        case CarFilter::Circuits:
            text = number(carCounter(car, CarRaces::Circuits)) + L" (" +
                   number(carCounter(car, CarRaces::CircuitWins)) + L")";
            break;
        case CarFilter::Sprints:
            text = number(carCounter(car, CarRaces::Sprints)) + L" (" + number(carCounter(car, CarRaces::SprintWins)) +
                   L")";
            break;
        case CarFilter::Showoffs: text = number(carCounter(car, CarRaces::Showoffs)); break;
        case CarFilter::PinkSlips: text = number(carCounter(car, CarRaces::PinkSlips)); break;
        default: break;
        }
        s.addMenuItem("DataMenu", text, static_cast<float>(m_order[i]));
        if (m_order[i] == m_kept) s.selectMenuItem("DataMenu", i);
    }
}

// 0x004E08D0: filtermenu the ten filters (1834 .., values 0 .. 9); SpentValue the money spent on cars (the profile's
// +0x4E8), WinningValue the money they won (+0x4EC), "$%s" ("%s $" in language 1); the garage's worth (the sale
// worths) into the profile's +0x4F0 and the greatest so far (+0x4F4); ValueValue the worth; CarsValue the cars; +0xEC
// -1, the filter 0, the order (0x004E1460) and the lists (0x004E0E70).
void CarCollection::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    m_filter = CarFilter::None;
    s.clearMenu("filtermenu");
    for (int i = 0; i < 10; ++i) {
        s.addMenuItem("filtermenu", texts.at(kTextCarFilters + static_cast<size_t>(i)), static_cast<float>(i));
    }
    const auto dollars = [&](int32_t value) {
        const std::string n = groupThousands(value, host.language().index);
        return toWide(host.language().index == 1 ? n + " $" : "$" + n);
    };
    s.setText("SpentValue", dollars(static_cast<int32_t>(p.moneySpent)));
    s.setText("WinningValue", dollars(static_cast<int32_t>(p.prizesWon)));
    int32_t worth = 0;
    for (const Car& car : career.garage) worth += saleWorth(car, data->catalogue);
    p.garageWorth = static_cast<uint32_t>(worth);
    if (worth > static_cast<int32_t>(p.garageWorthBest)) p.garageWorthBest = static_cast<uint32_t>(worth);
    s.setText("ValueValue", dollars(worth));
    s.setText("CarsValue", toWide(std::format("{}", career.garage.size())));
    m_kept = -1;
    m_filter = CarFilter::Rating;
    sort(host);
    lists(host, s);
}

// 0x004E0C70: BACK (CLICK) CMSStatsMenu; SELECT (CLICK) sorts by the filter and builds the lists again; PGDN on a car
// (CLICK): the dealer's +0x00, +0x27D4, +0x08, +0x27DC 0, the garage listed (0x004B4440, +0x27D8 the garage) and
// the car chosen (0x004B52F0), its +0x0C 1, garageload and the career's +0xA4 0; a new filter highlighted (+0x0C)
// builds the lists again unsorted.
void CarCollection::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("CMSStatsMenu", SwitchMode::Replace);
        return;
    }
    if (event == input::kSelect) {
        host.sound("CLICK");
        sort(host);
        lists(host, s);
        return;
    }
    if (event == input::kSelect2) {
        if (s.menuSize("DataMenu") == 0) return;
        host.sound("CLICK");
        Dealer& dealer = data->career->dealer;
        dealer.mode = DealerMode::Garage;
        dealer.listCount = 0;
        dealer.deal = 0;
        dealer.chosen = 0;
        dealerGarageList(*data->career, data->tables, data->catalogue);
        dealerChoose(*data->career, x87::chop(s.menuItemValue("DataMenu", s.selectedMenuItem("DataMenu"))));
        dealer.fromStats = 1;
        host.request("garageload", SwitchMode::Replace);
        data->careerScreen = CareerScreen::CarSel;
        return;
    }
    if (s.menuSize("filtermenu") == 0) return;
    const float value = s.menuItemValue("filtermenu", s.selectedMenuItem("filtermenu"));
    if (static_cast<float>(static_cast<int32_t>(m_filter)) != value) {
        m_filter = static_cast<CarFilter>(x87::chop(value));
        lists(host, s);
    }
}

}  // namespace oj::frontend
