// CMSCheats: the cheats' letters.

#include "Game/Frontend/Menus/Cheats.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

namespace {

// The codes (0x00718F88, each letter kept as its code + 0x5A).
constexpr const char* kCheatCodes[8] = {"PINT", "1OSH", "1ESP", "1ARS", "1REW", "1HAR", "1IN.", "1LL."};

}  // namespace

// 0x004C5CB0: L1 .. L4 their letters ("%c"); the first code the letters make; on SELECT (the window's event, CLICK):
// a code WARNING, the cheat (0x004A6E50), a note (type 1) of its line (395 + the cheat) and +0x20 set; else the note
// "Invalid Cheat Code." (1507). Stock problem: the letters run through A .. Z and "." only, so the seven codes that
// start with "1" cannot be made; only PINT can.
void Cheats::letters(WindowHost& host, ui::Screen& s, uint32_t event) {
    for (size_t i = 0; i < 4; ++i) {
        if (m_letterElements[i] < 0) continue;
        s.setText(m_letterElements[i], std::wstring(1, static_cast<wchar_t>(m_letters[i])));
    }
    int32_t found = -1;
    for (int32_t code = 0; code < 8 && found < 0; ++code) {
        if (std::equal(m_letters.begin(), m_letters.end(), kCheatCodes[code])) found = code;
    }
    if (event != input::kSelect) return;
    host.sound("CLICK");
    if (found < 0) {
        host.message(MessageType::Continue, host.gameTexts().at(kTextInvalidCheat), false, nullptr);
        return;
    }
    host.sound("WARNING");
    if (GameData* data = host.gameData(); data != nullptr && data->career)
        data->applyCheat(gameRandom(), static_cast<Cheat>(found));
    host.message(MessageType::Continue, host.gameTexts().at(kTextCheatDone + static_cast<size_t>(found)), false,
                 nullptr);
    m_leaving = 1;
}

// 0x004C54D0: L1 .. L4 found (the first element of each name) and their letters A; the letters (0x004C5CB0); +0x20 0.
void Cheats::enter(WindowHost& host, ui::Screen& s) {
    for (size_t i = 0; i < 4; ++i) {
        m_letterElements[i] = s.find(std::format("L{}", i + 1));
        m_letters[i] = 'A';
    }
    letters(host, s, input::kNone);
    m_leaving = 0;
}

// 0x004C55B0: +0x20 set: extras. The highlighted button Ln shows TOPARROWn and BOTTOMARROWn and hides every other
// arrow. BACK (CLICK) extras; UP and DOWN (SELECT) turn the highlighted letter on (Z then ".", "." then A) or back (A
// then ".", "." then Z); none highlighted turns +0x20's low byte. Then the letters (0x004C5CB0).
void Cheats::update(WindowHost& host, ui::Screen& s, uint32_t event) {
    if (m_leaving != 0) host.request("extras", SwitchMode::Replace);
    int current = -1;
    for (int n = 0; n < 4; ++n) {
        if (_stricmp(s.currentButton().c_str(), std::format("L{}", n + 1).c_str()) == 0) current = n;
    }
    if (current >= 0) {
        s.show(std::format("TOPARROW{}", current + 1), false);
        s.show(std::format("BOTTOMARROW{}", current + 1), false);
        for (int n = 0; n < 4; ++n) {
            if (n == current) continue;
            hideNamed(s, std::format("TOPARROW{}", n + 1), true);
            hideNamed(s, std::format("BOTTOMARROW{}", n + 1), true);
        }
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("extras", SwitchMode::Replace);
    } else if (event == input::kUp || event == input::kDown) {
        // Stock problem: with no letter's button highlighted the place is 4 (0x004C5BF6), so the byte after the
        // letters is turned: +0x20's low byte, which then sends the window back to extras.
        char spare = static_cast<char>(m_leaving & 0xFF);
        char& letter = current >= 0 ? m_letters[static_cast<size_t>(current)] : spare;
        host.sound("SELECT");
        if (event == input::kUp) {
            ++letter;
            if (letter == '[') letter = '.';
            if (letter == '/') letter = 'A';
        } else {
            --letter;
            if (letter == '@') letter = '.';
            if (letter == '-') letter = 'Z';
        }
        if (current < 0) m_leaving = (m_leaving & ~0xFF) | static_cast<uint8_t>(spare);
    }
    letters(host, s, event);
}

}  // namespace oj::frontend
