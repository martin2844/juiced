#pragma once
// The custom race's window instant_selcar (docs/spec/02-frontend.md, "The custom race").

#include "Core/Race/Racer.h"
#include "Game/Cars/CarView.h"
#include "Game/Frontend/CustomRace/CustomRaceHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <memory>
#include <optional>

namespace oj::frontend {

// The window of the record of instant_selcar (0x00720C10, vtable 0x006F19F4).
class InstantSelCar {
public:
    // 0x0045EC40
    void enter(CustomRaceHost& host, ui::Screen& s);
    // 0x0045F340
    void update(CustomRaceHost& host, ui::Screen& s, uint32_t event);
    // 0x0045F100
    void leave();

private:
    // 0x0045F150: OptionsBox, CarSel_Colour_Text and CarSel_ColourArrows shown; the car of the group's place locked off
    // line: Locked and Locked_Text shown, else hidden; with the parts' buttons (+0x48) CarSel_AutoMod(_Text) and
    // CarSel_ClearMod(_Text) shown, else hidden.
    void showOptions(CustomRaceHost& host, ui::Screen& s);
    // 0x0045A8A0 on the car shown, while the view loads nothing: clear (0x004600C0) the parts, else the automatic
    // modification (0x0048F2C0) as a prototype (any level) or of level 2; the model's parts again.
    void setParts(CustomRaceHost& host, int clear, int prototype);

    std::unique_ptr<scene::CarView> m_view;  // +0x10
    int32_t m_place = 0;       // +0x1C: the place in the group
    uint32_t m_car = 0;        // +0x20: the arcade car shown
    int32_t m_group = 0;       // +0x24
    int32_t m_paint = 0;       // +0x28: the paint of the CarDefs' list
    bool m_fade = false;       // +0x18: CARFADE
    bool m_modding = false;    // +0x44: MODDING shown
    bool m_options = false;    // +0x48: the auto modification's buttons
    bool m_prototypes = false;  // +0x4C: PROT_ shown
    int m_answer = -1;         // +0x54: "AUTO-MOD CAR?"
    // The start kept for the network's BACK: the place (+0x2C), the car (+0x30), its paint (+0x34), the group
    // (+0x38) and a copy of the car (+0x3C, 0x00434D90); its question's answer (+0x50).
    int32_t m_savedPlace = 0;
    uint32_t m_savedCar = 0;
    int32_t m_savedPaint = 0;
    int32_t m_savedGroup = 0;
    std::optional<Car> m_saved;
    int m_backAnswer = -1;
};

}  // namespace oj::frontend
