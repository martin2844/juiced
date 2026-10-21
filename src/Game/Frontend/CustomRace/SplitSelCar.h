#pragma once
// The custom race's window split_selcar (docs/spec/02-frontend.md, "The custom race").

#include "Game/Cars/CarView.h"
#include "Game/Frontend/CustomRace/CustomRaceHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace oj::frontend {

// The window of the record of split_selcar (0x00721698, vtable 0x006F2BBC), each
// player's: the car view (+0x0C, +0x10), the place in its groups (+0x34), the arcade car
// (+0x3C), the paint (+0x44), ready (+0x54) and MODDING shown (+0x64).
class SplitSelCar {
public:
    // 0x0046CE10
    void enter(CustomRaceHost& host, ui::Screen& s);
    // 0x0046DA60
    void update(CustomRaceHost& host, ui::Screen& s);
    // 0x0046D3F0
    void leave();

private:
    // 0x0046D4A0, each player with its own event (the window's +0x88 / +0x8C).
    void updatePlayer(CustomRaceHost& host, ui::Screen& s, size_t i);

    std::array<std::unique_ptr<scene::CarView>, 2> m_views;
    std::array<int32_t, 2> m_place{};
    std::array<uint32_t, 2> m_car{};
    std::array<int32_t, 2> m_paint{};
    std::array<int32_t, 2> m_ready{};
    std::array<int32_t, 2> m_modding{};
};

}  // namespace oj::frontend
