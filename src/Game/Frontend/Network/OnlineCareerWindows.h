#pragma once
// The online career's grid walk (CMSOnlineGridWalk).

#include "Core/Race/Racer.h"
#include "Game/Cars/CarSound.h"
#include "Game/Cars/CarView.h"
#include "Game/Frontend/Network/NetworkHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>
#include <memory>

namespace oj::frontend {

// The window of the record of CMSOnlineGridWalk (0x00769A68, vtable 0x006F5D10): each slot's car
// (+0x10, 0xBC bytes each), car view (+0x478) and config seen (+0x490, 0x74 bytes each); the list the
// cars are shown from (+0x0C, 0x00489B80: the slot's car put in, shown and taken out again), its
// current car.
class OnlineGridWalk {
public:
    // 0x004D3310
    void enter(NetworkHost& host, ui::Screen& s);
    // 0x004D40B0
    void update(NetworkHost& host, ui::Screen& s, uint32_t event);
    // 0x004D3850
    void leave(WindowHost& host);
    // 0x004D4090
    void timeout();

private:
    // 0x004D38F0, each frame. ready<n> and notready<n> hidden past the players, else shown; Car3d<n> shown for a
    // player, else it and bph<n> hidden.
    void slots(NetworkHost& host, ui::Screen& s);

    struct Slot {
        Car car;
        std::unique_ptr<scene::CarView> view;
        std::array<uint8_t, 0x74> seen{};
    };
    std::array<Slot, 6> m_slots;
    uint32_t m_current = 0;    // the list's +0x10
    int32_t m_ready = 0;       // +0x748: our readiness
    int32_t m_own = -1;        // +0x74C: the slot of our car (a pink slip; the stock's pointer, none at first)
    int32_t m_rival = -1;      // +0x750: the slot of the rival's car
    int32_t m_staked = 0;      // +0x754: the stakes set (0x004A49A0)
    int32_t m_unused = -1;     // +0x758: -1 by the enter, read by nothing
    int32_t m_answer = -1;     // +0x75C: the pink slip's question
    int32_t m_left = 0;        // +0x760: the session left (0x004D4090)
};

}  // namespace oj::frontend
