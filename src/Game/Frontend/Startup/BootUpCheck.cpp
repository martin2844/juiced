// bootupcheck: the save device's start.

#include "Game/Frontend/Startup/BootUpCheck.h"

#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

// bootupcheck (record 0x00721648, vtable 0x006F25F0: enter 0x004673E0, update 0x004674F0, leave 0x004674D0; no
// window asks for it): the enter clears the requests of the save device and starts it again, +0x20 0, +0x30 and +0x34
// -1 (the messages' numbers and +0x1C 10, +0x2C 2.0 are read by nothing), hides each element named FRAME_BG (the
// _stricmp at 0x0046746C hides on a match; the window's only element) and asks for driver (mode 1). The update (not busy): +0x20 set closes the message up and asks for driver; an answer in +0x30
// (1 sets +0x20) is taken back to -1; else the profile device steps. The leave starts the device again.
void BootUpCheck::enter(SaveHost& host, ui::Screen& s) {
    *this = BootUpCheck{};
    if (GameData* data = host.gameData(); data != nullptr) {
        data->saveRequest.clear();
        data->saveDevice.reset();
    }
    for (int i = 0; i < s.size(); ++i) {
        if (_stricmp(s.nameOf(i).c_str(), "FRAME_BG") == 0) s.setHidden(i, true);
    }
    host.request("driver", SwitchMode::Replace);
}

void BootUpCheck::update(SaveHost& host) {
    host.setSaveBusy(0);
    if (m_toDriver != 0) {
        if (host.messageUp()) host.closeMessage();
        host.request("driver", SwitchMode::Replace);
        return;
    }
    if (m_answer != -1) {
        if (m_answer == 1) m_toDriver = 1;
        m_answer = -1;
        return;
    }
    if (GameData* data = host.gameData(); data != nullptr) {
        data->saveDevice.stepProfile(data->saveRequest, host.saveSide());
    }
}

}  // namespace oj::frontend
