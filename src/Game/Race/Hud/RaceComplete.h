#pragma once
// The window hud_race_complete: the race's outcome, then the results' window.

#include "Core/Data/Language.h"
#include "Game/Race/Hud/HudRace.h"
#include "Game/Race/Hud/RaceHud.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/WindowManager.h"

#include <cstdint>
#include <memory>
#include <string>

namespace oj {

// The window hud_race_complete (vtable 0x006F14D8: enter 0x0044FD20, frame 0x004504A0): the race's outcome for 5 s,
// then the results' window. The respect counted in the career (Counter, RespMsg, RespCounter, Respect_Icon) is not
// followed: those elements stay hidden as outside the career.
class RaceComplete {
public:
    RaceComplete(ui::WindowManager& windows, const TextTable& texts);
    // hud: the HUD state of the race front end, whose players' messages are emptied. The window is loaded here, its
    // transitions starting as it opens; false without it.
    bool enter(const HudRace& race, RaceHud* hud);
    // The window it asks the race front end for once its time is out, else empty.
    std::string update(const HudRace& race, float dt);
    void draw(float seconds, float dt);

private:
    ui::WindowManager& m_windows;
    std::shared_ptr<ui::Screen> m_screen;
    const TextTable& m_texts;
    float m_time = 0.0f;      // +0x0C
    float m_master = 0.0f;    // +0x14: the 3D world's player's master as the window came
    int m_fading = -1;        // +0x10: MSG, or POS when the place or nothing is shown there
    int m_counter = -1;       // +0x18
    int m_respectMessage = -1;  // +0x1C
    int m_respectCounter = -1;  // +0x20
    float m_respect = 0.0f;   // +0x24: the respect shown in Counter
    float m_hold = 0.0f;      // +0x28
    int32_t m_countingBest = 0;        // +0x2C
    float m_bestCar = 0.0f;            // +0x30: the best car's respect still to count into Counter
    float m_bestCarWait = 0.0f;        // +0x34
    int32_t m_countingCollection = 0;  // +0x38
    float m_collection = 0.0f;         // +0x3C: the garage's worth's respect still to count
    float m_collectionWait = 0.0f;     // +0x40
    static inline float m_blink = 0.0f;     // [0x0079E7D4]
};

}  // namespace oj
