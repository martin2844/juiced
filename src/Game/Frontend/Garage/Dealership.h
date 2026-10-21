#pragma once
// The dealer's cars and the player's garage in the career (CMSCarDealership, CMSCarCollection2).

#include "Game/Cars/CarView.h"
#include "Game/Frontend/Garage/GarageHost.h"
#include "Game/UI/UiScreen.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace oj::frontend {

// The window of the records of CMSCarCollection2 (0x00721D38) and CMSCarDealership (0x00721D6C), vtable 0x006F5154.
class Dealership {
public:
    // 0x004C2AD0
    void enter(GarageHost& host, ui::Screen& s);
    // 0x004C2F10
    void update(GarageHost& host, ui::Screen& s, uint32_t event);
    // 0x004C2E70
    void leave(GarageHost& host);

private:
    // 0x004B4680: carmenu an item for each entry of the dealer's list.
    void menu(GarageHost& host, ui::Screen& s);
    // 0x004B48D0: the details of the list's entry highlighted.
    void details(GarageHost& host, ui::Screen& s);

    int32_t m_shown = -1;          // +0x0C: the list's place the view shows
    bool m_repairText = false;     // +0x10: Repair_Text
    bool m_repairButton = false;   // +0x14: Repair_But
    int32_t m_repairable = 0;      // +0x18: the car shown is damaged
    int32_t m_repairAnswer = -1;   // +0x1C: "ARE YOU SURE?"
    std::unique_ptr<scene::CarView> m_view;  // +0x20
    int32_t m_unused = -1;         // +0x2C: set by the enter (0x004C2CC9), not read by the window
    int32_t m_leaving = 0;         // +0x30
    // The globals the window keeps between its visits.
    uint32_t m_carsCurrent = 0;    // the dealer's cars' list (+0x4FB0) +0x10
    size_t m_offersRow = 0;        // [0x0075EF68]: carmenu's item highlighted when the dealership was left
    size_t m_garageRow = 0;        // [0x0075EF6C]: the same for the garage
    // [0x0079E854]: carmenu's highlighted item of the frame before. The stock keeps the item's address; here its
    // place and the fill of the menu it was in (a fill makes new items).
    int m_carMenuFill = 0;
    int m_lastCarFill = -1;
    size_t m_lastCarItem = 0;
};

}  // namespace oj::frontend
