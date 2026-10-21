#pragma once
// schememenu: the car's look kept as the settings' scheme or put on the car.

#include "Core/Cars/Car.h"
#include "Game/Frontend/Garage/Garage.h"
#include "Game/Frontend/Garage/GarageHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>

namespace oj::frontend {

// The window of the record 0x0071FDE0.
class SchemeMenu {
public:
    // 0x0043C600
    void enter(GarageHost& host, ui::Screen& s);
    // 0x0043CAB0
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    // A look put on the car (applying: charged, else as the window found it).
    void onCar(GarageHost& host, Car& car, const std::array<uint32_t, 17>& look, bool applying);

    int32_t m_answer = -1;               // +0x0C
    int32_t m_saving = 0;                // +0x18: saving asked (else applying)
    std::array<uint32_t, 17> m_kept{};   // +0x1C .. +0x5C: the car's look, as the settings' scheme keeps one
    int32_t m_applied = 0;               // +0x60
    int32_t m_price = 0;                 // +0x64: the price of applying
};

}  // namespace oj::frontend
