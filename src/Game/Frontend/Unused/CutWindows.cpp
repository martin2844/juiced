// Windows of the stock code the PC game never opens (docs/spec/02-frontend.md, "Options windows nothing asks for" and
// "Windows the game never opens"): opts_playlist, opts_ctl_port and opts_ctl_pc are registered but no code asks for
// them; the classes of the records 0x0072212C, 0x00722138 / 0x007225D8 and 0x0072173C are never registered. They run
// here as the stock has them, opened by --screen.

#include "Game/Frontend/Unused/CutWindows.h"

#include "Audio/SoundSystem.h"
#include "Core/Base/Log.h"
#include "Core/Base/Random.h"
#include "Core/Base/X87.h"
#include "Game/Frontend/WindowTools.h"
#include "Game/Media/Music.h"

#include <cstring>

namespace oj::frontend {

namespace {

// The input manager's devices ([G+0x18]'s list +0x18 .. +0x1C; the keyboard alone here) and its method +0x20 for
// each (0x0042E470: 1).
constexpr size_t kInputDevices = 1;

// The playlists' source [0x0075F85C] (0x005B83F0, made with the sound system 0x005B8500; vtable 0x006FE9C0): its
// method 0 fills a list of names and answers 1 when it has them; the PC's is 0x0051F960, which answers 0 and fills
// nothing (Xbox soundtracks have no PC kind).
bool playlists(std::vector<std::wstring>&) {
    return false;
}

// The settings' playlist name (the save system's +0x21A, settings +0x1C6: wide characters up to a 0; not saved).
std::wstring settingsPlaylist(const save::Settings& set) {
    std::wstring name;
    for (size_t at = 0x1C6; at + 1 < set.object.size(); at += 2) {
        const auto c = static_cast<wchar_t>(set.object[at] | (set.object[at + 1] << 8));
        if (c == 0) break;
        name.push_back(c);
    }
    return name;
}

}  // namespace

// opts_playlist (object 0x007688B0, made by the static initialiser 0x006BEEF0 through 0x0047FE20; registered by
// 0x00484CF0 without the preload the other options have; no code asks for it). Enter 0x004833C0 (not the windows'
// enter): PLMENU (+0x20) emptied (0x005AEEF0); [0x0075EF24] the playlists' source; the game's music made again
// (0x00483720); [0x0075EF1C] the new music object; the source's list (+0x38) asked for. With one (never on the PC: the
// source answers 0) each name from the list's last to its first is added to PLMENU (0x005AF230 at the end) with the
// value 0 for "Juiced" (the game's own) and 1 for any other; with the music object's player (+0x08) the one of the
// settings' playlist name (the save system's +0x21A) is highlighted (0x005AF8A0) and kept as +0x24 / +0x28, the place
// counting the items added.
// Stock problem: without the sound system ([0x0071E5D0] & 1) there is no source and the enter reads through a null
// pointer; here it answers as the PC's does.
void Playlist::enter(WindowHost& host, ui::Screen& s) {
    const bool menu = s.find("PLMENU") >= 0;
    if (menu) s.clearMenu("PLMENU");
    remakeMusic(host);
    std::vector<std::wstring> names;
    if (!playlists(names)) return;
    m_playlists = names;
    const std::wstring wanted = settingsPlaylist(host.optionSettings());
    int32_t place = 0;
    for (auto it = m_playlists.rbegin(); it != m_playlists.rend(); ++it, ++place) {
        const std::wstring& name = *it;
        if (menu) s.addMenuItem("PLMENU", name, name == L"Juiced" ? 0.0f : 1.0f);
        if (!jukebox().opened()) continue;
        if (wanted.size() == name.size() && wanted == name) {
            if (menu) s.selectMenuItem("PLMENU", static_cast<size_t>(place));
            m_current = place;
            m_currentName = name;
        }
    }
}

// 0x00483600: SELECT with an item highlighted in PLMENU (0x005AFB30): +0x48 its value cut toward zero (0x00424230),
// +0x4C its text, CLICK and opts_songlist (mode 7). BACK: CLICK, opts_audio (mode 7), +0x48 and +0x4C back to +0x24
// and +0x28.
void Playlist::update(WindowHost& host, ui::Screen& s, uint32_t event) {
    if (event == input::kSelect) {
        if (s.find("PLMENU") < 0 || s.menuSize("PLMENU") == 0) return;
        const size_t item = s.selectedMenuItem("PLMENU");
        m_chosen = x87::chop(s.menuItemValue("PLMENU", item));
        m_chosenName = s.menuItemText("PLMENU", item);
        host.sound("CLICK");
        host.request("opts_songlist", SwitchMode::Swap);
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("opts_audio", SwitchMode::Swap);
        m_chosen = m_current;
        m_chosenName = m_currentName;
    }
}

// 0x004835D0: PLMENU's item +0x24 highlighted (0x005AF8A0), then the menu emptied.
void Playlist::leave(ui::Screen& s) {
    if (s.find("PLMENU") < 0) return;
    s.selectMenuItem("PLMENU", static_cast<size_t>(m_current));
    s.clearMenu("PLMENU");
}

// 0x00483720: with the game's music object ([G+0x48]) the name of the sound its bank plays read (+0xEC's +0x20 and
// +0x28; kept by nothing), the object freed with its song and audio\music opened into a new one (0x00408B60:
// [G+0x48], [0x0075EF20] and [0x0075EF1C]); with the sound on the new one's playlist is made from the driver's
// settings (0x00408D20: the songs of their bits, shuffled with rand()).
void Playlist::remakeMusic(WindowHost& host) {
    GameData* data = host.gameData();
    Jukebox& music = jukebox();
    if (!music.opened()) return;
    music.reopen(GameFiles(std::filesystem::current_path()));
    if (audio::system().on() && data != nullptr) data->shuffleMusic(host.optionSettings(), gameRandom());
    log::info("frontend: the game's music made again");
}

// opts_ctl_port (record 0x00721BAC; no code asks for it). Enter 0x004838C0 (not the windows' enter): MENU highlights
// the item of the first player's device's place (the input manager's +0x28: the keyboard 0, or -1 before one is taken,
// past the items: 0 either way).
void ControllerPorts::enter(ui::Screen& s) {
    if (s.find("MENU") >= 0) s.selectMenuItem("MENU", 0);
}

// 0x004838F0: backmenu highlights MENU's item; each of the input manager's devices turns MENU's item of its place on
// (its +0x00 by the manager's method +0x20; Stock problem: a device past MENU's items writes through a null item; the
// keyboard alone does not). SELECT on MENU's highlighted item (past the items the highlight goes to the first): an
// item on gives CLICK and [0x0075EF2C] its place; place 4 (done) the options' menu, places 0 and 1 opts_ctl_ps2,
// places 2 and 3 opts_wheel with [0x0075EF2C] less 2 (mode 7; neither name is registered on the PC: a window of
// nothing comes up); an item off or no item NO. BACK: CLICK and the options' menu (mode 7).
void ControllerPorts::update(OptionsHost& host, ui::Screen& s, uint32_t event) {
    const bool menu = s.find("MENU") >= 0;
    if (menu && s.find("backmenu") >= 0) s.selectMenuItem("backmenu", s.selectedMenuItem("MENU"));
    for (size_t device = 0; device < kInputDevices; ++device) {
        if (menu && device < s.menuSize("MENU")) s.setMenuItemEnabledAt("MENU", device, true);
    }
    if (event == input::kSelect) {
        if (!menu) return;
        if (s.menuSize("MENU") != 0) {
            if (s.selectedMenuItem("MENU") >= s.menuSize("MENU")) s.selectMenuItem("MENU", 0, true);
            const size_t place = s.selectedMenuItem("MENU");
            if (s.menuItemEnabled("MENU", place)) {
                host.sound("CLICK");
                m_port = static_cast<int32_t>(place);
                if (place == 4) {
                    host.request(host.optionsMenu(), SwitchMode::Swap);
                } else if (place < 2) {
                    host.request("opts_ctl_ps2", SwitchMode::Swap);
                } else if (place < 4) {
                    m_port = static_cast<int32_t>(place) - 2;
                    host.request("opts_wheel", SwitchMode::Swap);
                }
                return;
            }
        }
        host.sound("NO");
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request(host.optionsMenu(), SwitchMode::Swap);
    }
}

// opts_ctl_pc (record 0x00721BB8; no code asks for it): no enter, update 0x00483C40: BACK gives CLICK and the
// options' menu (mode 7).
void PcControls::update(OptionsHost& host, uint32_t event) {
    if (event != input::kBack) return;
    host.sound("CLICK");
    host.request(host.optionsMenu(), SwitchMode::Swap);
}

// 0x004D31D0 (the class of the records 0x00722138 and 0x007225D8): BACK gives CLICK and CMSMainMenu (mode 1).
void BackToCareerMenu::update(WindowHost& host, uint32_t event) {
    if (event != input::kBack) return;
    host.sound("CLICK");
    host.request("CMSMainMenu", SwitchMode::Replace);
}

}  // namespace oj::frontend
