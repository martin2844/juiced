#pragma once
// The crew's orders on the race's HUD (docs/spec/22-race-hud.md): the members' lines, their levels and their keys.

#include "Core/Data/GameTexts.h"
#include "Game/Race/Hud/HudRace.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace oj {

// The crew's orders on the HUD: closed until a tactics key opens them, the orders' lines and keys, the levels' (set
// by nothing).
enum class OrdersState : int32_t { Closed = 0, Orders = 1, Levels = 2 };

// The crew's orders (the race drivers, career +0x74): the window kept (+0x38), the state (+0x04: 0 closed, 1 the
// members' lines, 2 a member's levels), its time (+0x08), the member of state 2 (+0x34), the members' cars (+0x1C,
// 8 bytes each with their flash time +0x20), the lines DPAD1 .. 3 (+0x48 ..).
class HudCrewOrders {
public:
    HudCrewOrders(ui::Screen& screen, const TextTable& texts) : m_screen(screen), m_texts(texts) {}

    // 0x004BDA30, in the HUD's enter. The orders shown in the career with the player's crew racing with members (the
    // race drivers' +0x0C); the members' cars taken once (0x004BDE90); each line with its pictures shown while its
    // member races; the lines emptied (0x004BDC50). The window is kept only while the drivers' +0x10 is set.
    void enter(const HudRace& race);
    // The crew's orders in the drivers' levels' frame (0x004BDF00): RaceHud::crewOrders.
    void frame(const HudRace& race, const std::array<bool, 6>& tactics, float dt);
    // The HUD's leave (0x0044A450): the orders' window and lines (the race drivers' +0x38 .. +0x5C) cleared.
    void leave();

private:
    // 0x004BDBC0
    void showOrders(ui::Screen& s, bool hidden);
    // 0x004BE720
    void orderLines(const HudRace& race, float dt);
    // 0x004BE340
    void orderKeys(const HudRace& race, const std::array<bool, 6>& tactics);
    // 0x004BE960
    void levelLines();
    // 0x004BE5C0
    void levelKeys(const HudRace& race, const std::array<bool, 6>& tactics, float dt);

    ui::Screen& m_screen;
    const TextTable& m_texts;
    struct CrewSlot {
        const RaceCar* car = nullptr;
        float flash = 0.0f;
    };
    bool m_ordersShown = false;
    OrdersState m_ordersState = OrdersState::Closed;
    float m_ordersTime = 0.0f;
    size_t m_ordersPick = 0;
    std::vector<CrewSlot> m_crewSlots;
    bool m_crewSlotsTaken = false;  // +0x14
    std::array<int, 3> m_dpad{-1, -1, -1};
    static inline float m_ordersBlink = 0.0f;    // [0x0079E848]
    static inline bool m_ordersFlip = false;     // [0x0075EF64]
};

}  // namespace oj
