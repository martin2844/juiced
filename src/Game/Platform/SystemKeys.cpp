#include "Game/Platform/SystemKeys.h"

#include <windows.h>

namespace oj {

struct SystemKeys::Saved {
    STICKYKEYS sticky{sizeof(STICKYKEYS), 0};
    TOGGLEKEYS toggle{sizeof(TOGGLEKEYS), 0};
    FILTERKEYS filter{sizeof(FILTERKEYS), 0, 0, 0, 0, 0};
};

namespace {

// SKF_HOTKEYACTIVE | SKF_CONFIRMHOTKEY (the same bits for ToggleKeys and FilterKeys)
constexpr DWORD kShortcutBits = 0x4 | 0x8;

}  // namespace

SystemKeys::SystemKeys(bool enabled) {
    if (!enabled) {
        return;
    }
    m_saved = new Saved;
    SystemParametersInfoW(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &m_saved->sticky, 0);
    SystemParametersInfoW(SPI_GETTOGGLEKEYS, sizeof(TOGGLEKEYS), &m_saved->toggle, 0);
    SystemParametersInfoW(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &m_saved->filter, 0);

    if ((m_saved->sticky.dwFlags & SKF_STICKYKEYSON) == 0) {
        STICKYKEYS off = m_saved->sticky;
        off.dwFlags &= ~kShortcutBits;
        SystemParametersInfoW(SPI_SETSTICKYKEYS, sizeof(off), &off, 0);
    }
    if ((m_saved->toggle.dwFlags & TKF_TOGGLEKEYSON) == 0) {
        TOGGLEKEYS off = m_saved->toggle;
        off.dwFlags &= ~kShortcutBits;
        SystemParametersInfoW(SPI_SETTOGGLEKEYS, sizeof(off), &off, 0);
    }
    if ((m_saved->filter.dwFlags & FKF_FILTERKEYSON) == 0) {
        FILTERKEYS off = m_saved->filter;
        off.dwFlags &= ~kShortcutBits;
        SystemParametersInfoW(SPI_SETFILTERKEYS, sizeof(off), &off, 0);
    }
}

SystemKeys::~SystemKeys() {
    if (m_saved == nullptr) {
        return;
    }
    SystemParametersInfoW(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &m_saved->sticky, 0);
    SystemParametersInfoW(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &m_saved->toggle, 0);
    SystemParametersInfoW(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &m_saved->filter, 0);
    delete m_saved;
}

}  // namespace oj
