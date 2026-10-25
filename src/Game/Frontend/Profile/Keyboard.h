#pragma once
// The name entry window (keyboard): a driver's, a crew's and an online account's names.

#include "Game/Frontend/Profile/ProfileHost.h"
#include "Game/UI/UiScreen.h"

#include <cstddef>
#include <functional>
#include <string>

namespace oj::frontend {

// What the window that opens keyboard sets up in its state object (0x00720C88); cleared when keyboard closes.
struct KeyboardSetup {
    std::wstring title;      // +0x00C, the INSTRUCTION text
    std::wstring ticker;     // +0x10C, the TICKER text
    std::wstring text;       // +0x20C, the text being entered
    size_t minLength = 0;    // +0x30C
    size_t maxLength = 0;    // +0x310
    std::function<bool(const std::wstring&)> accept;  // +0x314, called on DONE
    bool password = false;   // +0x31C
    std::string allowed;     // +0x320, the characters that may be entered
    std::string notFirst;    // +0x420, characters that may not start the text
    bool symbols = false;    // +0x520, the symbols page may be shown
    std::string back;        // +0x524, the state BACK goes to
    std::string next;        // +0x564, the state after DONE
    bool checkName = true;   // +0x5BC, the text goes through the name filter
    bool small = false;      // +0x5B8, the smaller text box (more than 26 characters)
    // The fixed build's lobby chat (Frontend/Network/LobbyChat): its lines above, DONE or ENTER says the text and
    // stays, the session leaving the lobby goes back.
    bool chat = false;
};

// The keys' letters (the keyboard's +0x5A4): lower case, upper case, the symbols page.
enum class LetterCase : int32_t { Lower = 0, Upper = 1, Symbols = 2 };

class Keyboard {
public:
    KeyboardSetup& setup() { return m_setup; }
    // 0x00460960
    void enter(ProfileHost& host, ui::Screen& s);
    // 0x00460B40; typed: the character typed this frame; shiftHeld: a shift key is down.
    void update(ProfileHost& host, ui::Screen& s, uint32_t event, char typed, bool shiftHeld);
    // 0x00460840: the state object cleared as the window closes.
    void clear() { *this = Keyboard{}; }

private:
    // 0x004601E0: a character the window takes.
    bool allows(char c) const;
    // 0x00460450: the keys' letter case.
    void setCase(ui::Screen& s, bool shiftHeld);
    // 0x00460250: the keys' colours.
    void colours(ui::Screen& s);

    KeyboardSetup m_setup;
    LetterCase m_letterCase = LetterCase::Upper;  // +0x5A4
    bool m_symbolsOn = false;  // +0x5A8
    bool m_capsLock = false;   // +0x5AC
    bool m_shiftOnce = false;  // +0x5B0, the SHIFT button
    bool m_fresh = true;       // +0x5B4, the text is the starting text and the first character replaces it
    std::string m_hovered;     // +0x5C0, the button with the hover colour
    int m_width = 300;         // +0x5C4: NAMEBOX's width in pixels, else 300 (keyboard.ui has none)
};

}  // namespace oj::frontend
