#pragma once
// The windows the PC game never opens (Game/Frontend/Unused/CutWindows.cpp): opts_playlist (object 0x007688B0,
// vtable 0x006F3624), opts_ctl_port (record 0x00721BAC, vtable 0x006F36C8) and opts_ctl_pc (record 0x00721BB8,
// vtable 0x006F3688); and three more classes never registered: a plain window (record 0x0072212C, vtable
// 0x006F5598), one going back to CMSMainMenu (records 0x00722138 and 0x007225D8, vtable 0x006F5D00) and one doing
// nothing (record 0x0072173C, vtable 0x006F2BDC), opened here by "unregistered:0x0072212C",
// "unregistered:0x00722138", "unregistered:0x007225D8" and "unregistered:0x0072173C" (no .ui).

#include "Game/Frontend/Options/OptionsHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <string>
#include <vector>

namespace oj::frontend {

// opts_playlist (the object 0x007688B0).
class Playlist {
public:
    // 0x007688B0
    void enter(WindowHost& host, ui::Screen& s);
    // 0x00483600
    void update(WindowHost& host, ui::Screen& s, uint32_t event);
    // 0x004835D0
    void leave(ui::Screen& s);
    // The choice back to the playlist the window started with (+0x48, +0x4C from +0x24, +0x28).
    void restoreChoice() {
        m_chosen = m_current;
        m_chosenName = m_currentName;
    }

private:
    // 0x00483720: with the game's music object ([G+0x48]) the name of the sound its bank plays read (+0xEC's +0x20 and
    // +0x28; kept by nothing), the object freed with its song and audio\music opened into a new one (0x00408B60:
    // [G+0x48], [0x0075EF20] and [0x0075EF1C]); with the sound on the new one's playlist is made from the driver's
    // settings (0x00408D20: the songs of their bits, shuffled with rand()).
    void remakeMusic(WindowHost& host);

    int32_t m_current = 0;          // +0x24: PLMENU's item of the driver's playlist
    std::wstring m_currentName;     // +0x28: its name
    std::vector<std::wstring> m_playlists;  // +0x38: the playlists' names
    int32_t m_chosen = 0;           // +0x48: the playlist chosen (the item's value, cut toward zero)
    std::wstring m_chosenName;      // +0x4C: its name
};

// opts_ctl_port (the record 0x00721BAC).
class ControllerPorts {
public:
    // 0x00721BAC
    void enter(ui::Screen& s);
    // 0x004838F0
    void update(OptionsHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_port = 0;  // [0x0075EF2C]: the port opts_ctl_port chose (written only)
};

// opts_ctl_pc (the record 0x00721BB8); it keeps no state of its own.
class PcControls {
public:
    // 0x00721BB8
    void update(OptionsHost& host, uint32_t event);
};

// The class of the records 0x00722138 and 0x007225D8; it keeps no state of its own.
class BackToCareerMenu {
public:
    // 0x004D31D0
    void update(WindowHost& host, uint32_t event);
};

}  // namespace oj::frontend
