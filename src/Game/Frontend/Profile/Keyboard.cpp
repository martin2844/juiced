// The name entry window (keyboard): a driver's, a crew's and an online account's names.

#include "Game/Frontend/Profile/Keyboard.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/Network/LobbyChat.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

namespace {

// Colours of the keyboard window's buttons (0x00460250, 0x00460B40).
constexpr uint32_t kKeyColour = 0xFF00B4FF;
constexpr uint32_t kKeyHighlight = 0xFFECEC01;
constexpr uint32_t kKeyUnavailable = 0xC8800000;
constexpr uint32_t kKeyBox = 0xAF000000;
constexpr uint32_t kKeyHoverBox = 0xAF404040;

}  // namespace

// A character the keyboard window takes (0x004601E0): one of the allowed characters, and not one of notFirst at
// the start of the text.
bool Keyboard::allows(char c) const {
    if ((m_fresh || m_setup.text.empty()) && c != 0 && m_setup.notFirst.find(c) != std::string::npos) {
        return false;
    }
    return c != 0 && m_setup.allowed.find(c) != std::string::npos;
}

// The letter case of the keys (0x00460450): the symbols page, else upper case while shift is held or the SHIFT button
// is on, turned over by caps.
void Keyboard::setCase(ui::Screen& s, bool shiftHeld) {
    LetterCase mode = LetterCase::Symbols;
    if (!m_symbolsOn) {
        mode = (shiftHeld || m_shiftOnce) ? (m_capsLock ? LetterCase::Lower : LetterCase::Upper)
                                          : (m_capsLock ? LetterCase::Upper : LetterCase::Lower);
    }
    if (mode == m_letterCase) {
        return;
    }
    m_letterCase = mode;
    if (mode != LetterCase::Symbols) {
        for (char c = 'A'; c <= 'Z'; ++c) {
            const wchar_t letter =
                mode == LetterCase::Lower ? static_cast<wchar_t>(c - 'A' + 'a') : static_cast<wchar_t>(c);
            s.setText(std::string("B") + c, std::wstring(1, letter));
        }
    }
    colours(s);
}

// The colours of the keys (0x00460250): keys of characters that may not be entered, and DONE while the text is too
// short, are dimmed.
void Keyboard::colours(ui::Screen& s) {
    for (const std::string& b : s.buttons()) {
        bool available = true;
        if (b.size() == 2) {
            const std::wstring caption = s.textOf(b);
            available = allows(caption.empty() ? '\0' : static_cast<char>(caption[0]));
        } else if (_stricmp(b.c_str(), "SPACE") == 0) {
            available = allows(' ');
        } else if (_stricmp(b.c_str(), "DONE") == 0) {
            available = m_setup.text.size() >= m_setup.minLength;
        }
        if (available) {
            s.setFontColours(b, kKeyColour, kKeyHighlight);
        } else {
            s.setFontColours(b, kKeyUnavailable, kKeyUnavailable);
        }
    }
}

// The keyboard window opens (0x00460960).
void Keyboard::enter(ProfileHost&, ui::Screen& s) {
    s.setText("INSTRUCTION", m_setup.title);
    s.setText("TICKER", m_setup.ticker);
    if (!m_setup.symbols) {
        s.setHidden("SELECTC", true);
        s.setHidden("SYMBOLS_TEXT", true);
    }
    s.setHidden("STRING", m_setup.small);
    s.setHidden("STRING_SMALL", !m_setup.small);
    const int box = s.pixelWidth("NAMEBOX");
    m_width = box >= 0 ? box : 300;
    m_letterCase = LetterCase::Upper;
    m_symbolsOn = false;
    m_capsLock = false;
    m_shiftOnce = false;
    setCase(s, false);
    m_fresh = true;
    m_hovered.clear();
    lobbyChatLayout(s, m_setup.chat);
}

// The keyboard window (0x00460B40): the keys on screen, the characters typed, DONE and BACK.
void Keyboard::update(ProfileHost& host, ui::Screen& s, uint32_t event, char typed, bool shiftHeld) {
    std::wstring& text = m_setup.text;
    if (m_setup.chat) {
        // The fixed build's lobby chat: gone with the session's lobby; ENTER typed says the text (the frame's SELECT
        // with it left alone).
        if (!lobbyChatFrame(s)) {
            host.request(m_setup.back, SwitchMode::Replace);
            return;
        }
        if (typed == '\r') {
            if (lobbyChatSay(text)) {
                text.clear();
                m_fresh = false;
                host.sound("CLICK");
            }
            typed = 0;
            event = input::kNone;
        }
    }
    auto append = [&](const std::wstring& added) {
        if (m_fresh) {
            text.clear();
            m_fresh = false;
        }
        if (text.size() < m_setup.maxLength) {
            text += added;
            host.sound("CLICK");
        }
    };
    if (event == input::kSelect) {
        const bool wasShift = m_shiftOnce;
        m_shiftOnce = false;
        const std::string button = s.currentButton();
        if (_stricmp(button.c_str(), "DONE") == 0 && m_setup.chat) {
            // The fixed build's lobby chat: the text said, the keyboard kept.
            if (lobbyChatSay(text)) {
                text.clear();
                m_fresh = false;
                host.sound("CLICK");
            } else {
                host.sound("NO");
            }
        } else if (_stricmp(button.c_str(), "DONE") == 0) {
            if (text.size() < m_setup.minLength) {
                host.sound("NO");
            } else if (m_setup.checkName && host.nameRefused(text)) {
                // 0x00460BF3: the name filter's note (type 1).
                host.message(MessageType::Continue, host.gameTexts().at(kTextIllegalName), false, nullptr);
            } else if (!m_setup.accept || m_setup.accept(text)) {
                host.sound("CLICK");
                host.request(m_setup.next, SwitchMode::Replace);
            }
        } else if (_stricmp(button.c_str(), "BACKSPACE") == 0) {
            m_fresh = false;
            if (!text.empty()) {
                text.pop_back();
                host.sound("CLICK");
            }
            colours(s);
        } else if (_stricmp(button.c_str(), "SPACE") == 0) {
            if (!allows(' ')) {
                host.sound("NO");
            } else {
                append(L" ");
                colours(s);
            }
        } else if (_stricmp(button.c_str(), "SHIFT") == 0) {
            if (!wasShift) {
                m_shiftOnce = true;
            } else {
                setCase(s, shiftHeld);
            }
            host.sound("CLICK");
        } else {
            const std::wstring caption = s.textOf(button);
            if (!allows(caption.empty() ? '\0' : static_cast<char>(caption[0]))) {
                host.sound("NO");
            } else {
                append(caption);
                colours(s);
            }
        }
    } else if (event == input::kSelect2) {
        const bool wasShift = m_shiftOnce;
        m_symbolsOn = false;
        m_shiftOnce = false;
        if (!wasShift) {
            m_capsLock = !m_capsLock;
        }
        host.sound("CLICK");
    } else if (event == input::kSelect3) {
        if (m_setup.symbols) {
            m_shiftOnce = false;
            m_symbolsOn = !m_symbolsOn;
            host.sound("CLICK");
        }
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request(m_setup.back, SwitchMode::Replace);
    }

    switch (typed) {
    case 0:
    case 0x09:
    case 0x0A:
    case 0x0E:
    case 0x1B:
        break;
    case 0x08:
        if (m_fresh) {
            text.clear();
            m_fresh = false;
        }
        if (!text.empty()) {
            text.pop_back();
            host.sound("CLICK");
        }
        m_shiftOnce = false;
        colours(s);
        break;
    case 0x0F:
        m_shiftOnce = false;
        host.sound("CLICK");
        break;
    default: {
        char c = typed;
        if (m_letterCase == LetterCase::Lower) {
            c = static_cast<char>(c | 0x20);
        }
        if (!allows(c)) {
            host.sound("NO");
        } else {
            append(std::wstring(1, static_cast<wchar_t>(static_cast<uint8_t>(c))));
            colours(s);
        }
        m_shiftOnce = false;
        break;
    }
    }

    if (host.switchAsked()) {
        return;
    }
    setCase(s, shiftHeld);
    // 0x0046112F: the text within the width (its start cut off, 0x005A9060), a password as stars (its FontModifier
    // record started again as a text set does, 0x00461163: keyboard.ui has none).
    const char* field = m_setup.small ? "STRING_SMALL" : "STRING";
    const std::wstring shown =
        m_setup.password ? std::wstring(text.size(), L'*') : s.fitTextEnd(field, text, m_width);
    s.setText(field, shown);
    const std::string& current = s.currentButton();
    if (m_hovered != current) {
        if (!m_hovered.empty()) {
            s.setBoxColour(m_hovered, kKeyBox);
        }
        m_hovered = current;
        if (!current.empty()) {
            s.setBoxColour(current, kKeyHoverBox);
        }
    }
}

}  // namespace oj::frontend
