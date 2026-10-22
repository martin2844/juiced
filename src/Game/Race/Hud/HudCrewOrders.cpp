// The crew's orders on the race's HUD (docs/spec/22-race-hud.md): the members' lines and their keys.

#include "Game/Race/Hud/HudCrewOrders.h"
#include "Game/Race/Hud/HudInternal.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace oj {

using namespace hud;

void HudCrewOrders::enter(const HudRace& race) {
    ui::Screen& s = m_screen;
    // The orders shown in the career with the player's crew racing with members (the race drivers' +0x0C); the
    // members' cars taken once (0x004BDE90: the racers of crew 0 other than its leader, in the session's order, each
    // with the car made for it); each line with its pictures shown while its member races; the lines emptied
    // (0x004BDC50). The window is kept only while the drivers' +0x10 is set.
    m_ordersShown = false;
    m_dpad = {s.find("Dpad1"), s.find("Dpad2"), s.find("Dpad3")};
    const bool crewRace =
        race.mode == RaceMode::Career && race.career != nullptr && race.career->drivers.withMembers != 0;
    if (!m_crewSlotsTaken && race.mode == RaceMode::Career && race.career != nullptr) {
        m_crewSlotsTaken = true;
        const RaceSession& session = race.career->session;
        for (uint32_t i = 0; i < session.count && i < session.racers.size(); ++i) {
            const Racer& r = session.racers[i];
            if (r.crew != 0 || r.member == 0) continue;
            const RaceCar* car = nullptr;
            for (const RaceCar* c : race.cars) {
                if (c != nullptr && c->record == static_cast<int32_t>(i)) car = c;
            }
            m_crewSlots.push_back({car, 0.0f});
        }
    }
    // The state: 1 in a crew race with members, else 0 (all the pictures hidden until a tactics key opens it).
    m_ordersState = crewRace ? OrdersState::Orders : OrdersState::Closed;
    m_ordersShown = race.mode == RaceMode::Career && race.career != nullptr && race.career->drivers.ordersShown != 0;
    showOrders(s, !crewRace);
    if (m_ordersShown) {
        for (const int e : m_dpad) {
            if (e >= 0) s.clearText(e);
        }
    }
}

void HudCrewOrders::leave() {
    m_ordersShown = false;
    m_dpad = {-1, -1, -1};
}

// 0x004BDBC0: each line's pictures (DpadBg, Dpad, DpadControl) hidden as asked, or when the line has no member.
void HudCrewOrders::showOrders(ui::Screen& s, bool hidden) {
    static const char* const kBg[3] = {"DpadBg1", "DpadBg2", "DpadBg3"};
    static const char* const kControl[3] = {"DpadControl01", "DpadControl02", "DpadControl03"};
    for (size_t k = 0; k < 3; ++k) {
        const bool hide = hidden || k >= m_crewSlots.size() || m_crewSlots[k].car == nullptr;
        for (const int e : {s.find(kBg[k]), m_dpad[k], s.find(kControl[k])}) {
            if (e >= 0) s.setHidden(e, hide);
        }
    }
}

// The state of the crew's orders (0x004BDF00's end, with the drivers' +0x10 set): 0 (0x004BE290) any tactics key
// opens it; 1 the lines (0x004BE720) and the keys (0x004BE340); 2 (set by nothing) the levels (0x004BE960) and their
// keys (0x004BE5C0).
void HudCrewOrders::frame(const HudRace& race, const std::array<bool, 6>& tactics, float dt) {
    m_ordersFlip = !m_ordersFlip;
    if (!m_ordersShown || race.career == nullptr || race.career->drivers.ordersShown == 0) return;
    if (m_ordersState == OrdersState::Closed) {
        bool any = false;
        for (const bool t : tactics) any = any || t;
        if (any) open(race, dt);
        return;
    }
    if (m_ordersState == OrdersState::Levels) {
        levelLines();
        levelKeys(race, tactics, dt);
        return;
    }
    orderLines(race, dt);
    orderKeys(race, tactics);
}

void HudCrewOrders::flash(const HudRace& race, int32_t racer, float dt) {
    if (!m_ordersShown || race.career == nullptr) return;
    for (CrewSlot& slot : m_crewSlots) {
        if (slot.car != nullptr && slot.car->record == racer) slot.flash = 1.0f;
    }
    if (m_ordersState == OrdersState::Closed) open(race, dt);
}

void HudCrewOrders::open(const HudRace& race, float dt) {
    m_ordersState = OrdersState::Orders;
    // 0x004BE9F0: swapped until none is ahead of the one before.
    const auto place = [](const CrewSlot& c) {
        return c.car != nullptr && c.car->competitor != nullptr ? c.car->competitor->place : 0;
    };
    for (size_t i = 0; i + 1 < m_crewSlots.size(); ++i) {
        if (place(m_crewSlots[i + 1]) < place(m_crewSlots[i])) {
            std::swap(m_crewSlots[i], m_crewSlots[i + 1]);
            i = static_cast<size_t>(-1);
        }
    }
    showOrders(m_screen, false);
    orderLines(race, dt);
    m_ordersTime = 5.0f;
}

// 0x004BE960: the lines emptied (0x004BDC50), then DPAD1 .. 3 "High", "Medium", "Low" (their effects started again)
// in 0xFFECEC01.
void HudCrewOrders::levelLines() {
    ui::Screen& s = m_screen;
    for (const int e : m_dpad) {
        if (e >= 0) s.clearText(e);
    }
    for (size_t k = 0; k < m_dpad.size(); ++k) {
        const int e = m_dpad[k];
        if (e < 0) continue;
        s.setText(e, m_texts.at(kTextQualities + 2 - k));
        s.setFontColour(e, 0xFFECEC01u);
    }
}

// 0x004BE5C0: for the member of +0x34 DRIVER_TACTICS_UP, _RIGHT, _DOWN give the level 2, 1, 0 (its order and the
// order's time 0; a changed level's time -1); one of them or _LEFT ends the time (-1); the time runs down by the
// window's step and below 0 the state is 1 again.
void HudCrewOrders::levelKeys(const HudRace& race, const std::array<bool, 6>& tactics, float dt) {
    RaceSession& session = const_cast<Career*>(race.career)->session;
    if (m_ordersPick >= m_crewSlots.size()) return;
    const RaceCar* car = m_crewSlots[m_ordersPick].car;
    if (car == nullptr || car->record < 0 || static_cast<size_t>(car->record) >= session.racers.size()) return;
    Racer& r = session.racers[static_cast<size_t>(car->record)];
    bool any = false;
    static const int32_t kLevels[3] = {2, 1, 0};
    for (size_t k = 0; k < 3; ++k) {
        if (!tactics[k]) continue;
        r.order = 0;
        r.orderTime = 0.0f;
        if (r.level != kLevels[k]) {
            r.level = kLevels[k];
            r.levelTimer = -1.0f;
        }
        any = true;
    }
    if (tactics[3] || any) m_ordersTime = -1.0f;
    m_ordersTime = m_ordersTime - dt;
    if (m_ordersTime < 0.0f) m_ordersState = OrdersState::Orders;
}

void HudCrewOrders::orderLines(const HudRace& race, float dt) {
    ui::Screen& s = m_screen;
    // The orders change the racers of the career's session (the HUD reads the career as const elsewhere).
    RaceSession& session = const_cast<Career*>(race.career)->session;
    const auto racerOf = [&](const RaceCar* car) -> Racer* {
        if (car == nullptr || car->record < 0 || static_cast<size_t>(car->record) >= session.racers.size()) {
            return nullptr;
        }
        return &session.racers[static_cast<size_t>(car->record)];
    };
    // 0x004BE720: each member's line, "<short name> <place><st/nd/rd/th> <Low/Medium/High>", the place's ending
    // by its last digit; while the line's flash time is above 0 it is empty whenever the blink (0.25 s round, by
    // the game's frame time) is below 0.1, the flash running down only then (by the window's step); its colour by the
    // member's nerves: red 236, green (2 - nerves) / 2 x 236 rounded (held within 0 .. 236; 236 every other frame
    // past 1.9), blue 1.
    m_ordersBlink = m_ordersBlink - dt;
    if (m_ordersBlink < 0.0f) m_ordersBlink = 0.25f;
    for (const int e : m_dpad) {
        if (e >= 0) s.clearText(e);
    }
    static const size_t kEndings[10] = {40, 37, 38, 39, 40, 40, 40, 40, 40, 40};
    for (size_t k = 0; k < m_crewSlots.size() && k < m_dpad.size(); ++k) {
        CrewSlot& slot = m_crewSlots[k];
        Racer* r = racerOf(slot.car);
        if (r == nullptr || slot.car->competitor == nullptr) continue;
        std::wstring line;
        if (r->member >= 0 && r->member < 4) {
            const auto number = static_cast<size_t>(race.career->members[static_cast<size_t>(r->member)]);
            if (number < race.career->crewmates.size()) {
                const auto& name = race.career->crewmates[number].shortName;
                line = widen(std::string(name.data(), strnlen(name.data(), name.size())));
            }
        }
        const int32_t place = slot.car->competitor->place;
        line += L" " + widen(std::to_string(place)) + m_texts.at(kEndings[static_cast<size_t>(place % 10)]) + L" " +
                m_texts.at(kTextQualities + static_cast<size_t>(std::clamp(r->level, 0, 2)));
        const int e = m_dpad[k];
        if (e < 0) continue;
        if (slot.flash > 0.0f && m_ordersBlink < 0.1f) {
            slot.flash = slot.flash - dt;
            s.clearText(e);
        } else {
            s.setText(e, line);
        }
        // 0x004BEF20
        float green = std::clamp((2.0f - r->nerves) * 0.5f * 236.0f, 0.0f, 236.0f);
        if (r->nerves > 1.9f && m_ordersFlip) green = 236.0f;
        const auto g = static_cast<uint32_t>(static_cast<int32_t>(std::nearbyint(green))) & 0xFF;
        s.setFontColour(e, 0xFFEC0001u | (g << 8));
    }
}

void HudCrewOrders::orderKeys(const HudRace& race, const std::array<bool, 6>& tactics) {
    RaceSession& session = const_cast<Career*>(race.career)->session;
    const auto racerOf = [&](const RaceCar* car) -> Racer* {
        if (car == nullptr || car->record < 0 || static_cast<size_t>(car->record) >= session.racers.size()) {
            return nullptr;
        }
        return &session.racers[static_cast<size_t>(car->record)];
    };
    // 0x004BE340: a tactics key of a member's line: its order and the order's time 0, its level one up or down
    // (held within low .. high), and a changed level's time -1.
    static const struct {
        size_t action, slot;
        int step;
    } kKeys[6] = {{0, 0, 1}, {2, 0, -1}, {1, 1, 1}, {3, 1, -1}, {4, 2, 1}, {5, 2, -1}};
    for (const auto& key : kKeys) {
        if (!tactics[key.action] || key.slot >= m_crewSlots.size()) continue;
        Racer* r = racerOf(m_crewSlots[key.slot].car);
        if (r == nullptr) continue;
        r->order = 0;
        r->orderTime = 0.0f;
        const int32_t level = std::clamp(r->level + key.step, 0, 2);
        if (level != r->level) {
            r->level = level;
            r->levelTimer = -1.0f;
        }
    }
}

}  // namespace oj
