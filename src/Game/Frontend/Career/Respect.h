#pragma once
// CMSRespect: the crews' territories on the city's map.

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>

namespace oj::frontend {

// The window of the record 0x007226F8 (vtable 0x006F5F1C).
class Respect {
public:
    // 0x004DB2F0
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004DB4B0
    void leave(CareerHost& host);
    // 0x004DB740
    void update(CareerHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x004DBE60: each crew's territory added to MAP3D's list at (0, 0, 0) (0x005B3E60) and hidden (+0xBC | 4); the
    // list's entry after the element's own model for it (counted as if every one before it were added) placed at the
    // height 2000 (4000 for the seventh); its material (the model's +0xA8, its first) of Coloured Spec ABlend takes the
    // colour 0x33FF0000 less n x 0xFF00 for n = 255 x the crew's respect (+0x70, at most 1000) x 0.001 cut (red to
    // green), Additive 1, Specular Highlight and Shininess 0.5.
    void territories(CareerHost& host, ui::Screen& s);
    // 0x004DB610 with the map: a crew other than the one shown (+0x10): +0x3C 0, the speech's master 0 and a line it
    // says stopped (as 0x0040B4A0, the system stepped again after), the shown crew kept, the view out of and back into
    // the map mode on an arm of (0, 0, -400000) for the overview, else -300000, the pitch within -1.5 .. -0.4.
    void mapTo(CareerHost& host, ui::Screen& s);
    // 0x004DC050: for a crew only its territory shown, every other hidden; for the overview (0) all nine shown.
    void light(CareerHost& host, ui::Screen& s);
    // 0x004DBA80: for a crew (not the overview) data_logo its flyer's logo, data_location_val the location of its first
    // circuit's route (575 + it), data_leader_val the leader's name, data_respect_val its respect (thousands marked),
    // data_respect_meter its respect / 1500 (at most 1), data_bio_val1 .. 3 over several lines its bio (724, 732 and
    // 740 + the crew's number), data_respects_val what it respects most (297 + 0x004BC110), data_chall1 .. 3 ui_yes for
    // a challenge done, else ui_no.
    void crewTexts(CareerHost& host, ui::Screen& s);

    int32_t m_crew = -1;     // +0x0C: the value of Menu's highlighted item (0 the overview, else the crew)
    int32_t m_shown = -1;    // +0x10: the crew the map went to
    bool m_map = false;      // +0x38: the MAP3D element
    float m_timer = 0.0f;    // +0x3C: the time since the crew was chosen; at 4 s its narration
    std::array<int, 9> m_territories{};  // +0x48: the crews' territories in the element's list (-1 none)
    int32_t m_dataShown = 0;      // +0x70: the DATA panel shown
    float m_narration = 0.0f;  // +0x44: the settings' speech volume, the speech's master as the window goes
};

}  // namespace oj::frontend
