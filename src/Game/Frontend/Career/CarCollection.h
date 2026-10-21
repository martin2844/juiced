#pragma once
// CMSStatsCarCollection: the garage's cars by a filter.

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <vector>

namespace oj::frontend {

// The collection's sort (its +0x0C; the filter menu's values, the game's lines kTextCarFilters + filter name them).
enum class CarFilter : int32_t {
    Rating = 0,
    SaleWorth = 1,
    Spent = 2,
    Won = 3,
    Profit = 4,  // won less spent
    Races = 5,
    Circuits = 6,
    Sprints = 7,
    Showoffs = 8,
    PinkSlips = 9,
    None = 10,  // as the window opens
};

// The window of the record 0x00722998 (vtable 0x006F6328).
class CarCollection {
public:
    // 0x004E08D0
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004E0C70
    void update(CareerHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x004E1460: the cars' order (+0x10 ..) the garage's, then sorted by the filter (+0x0C), the greatest first (a
    // swap starts the pass again): the rating, the sale worth (0x0048DA40), the money spent (+0xA8), won (+0xAC), won
    // less spent, the car's races counted.
    void sort(CareerHost& host);
    // 0x004E0E70: the car kept highlighted (+0xEC: the first of the order at first, else CarMenu's highlighted item);
    // an empty garage empties CarMenu and DataMenu.
    void lists(CareerHost& host, ui::Screen& s);

    CarFilter m_filter = CarFilter::Rating;  // +0x0C
    std::vector<int32_t> m_order;   // +0x10: the garage indices in order
    int32_t m_kept = -1;            // +0xEC: the car kept highlighted
};

}  // namespace oj::frontend
