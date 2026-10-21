// The windows that run the same as the base window or as a window over it (the cut windows', the voice options):
// their enter, update and leave by the state.

#include "Game/Frontend/Frontend.h"
#include "Game/Frontend/FrontendInternal.h"

#include "Audio/SoundSystem.h"
#include "Core/Base/Log.h"
#include "Core/Base/Random.h"
#include "Core/Base/X87.h"
#include "Game/Media/Music.h"
#include <cstring>

namespace oj {

using namespace frontend;

namespace {

bool same(const std::string& a, const char* b) {
    return _stricmp(a.c_str(), b) == 0;
}

constexpr const char* kPlainWindow = "unregistered:0x0072212C";      // vtable 0x006F5598
constexpr const char* kBackWindowA = "unregistered:0x00722138";      // vtable 0x006F5D00
constexpr const char* kBackWindowB = "unregistered:0x007225D8";      // vtable 0x006F5D00
constexpr const char* kEmptyWindow = "unregistered:0x0072173C";      // vtable 0x006F2BDC

}  // namespace

bool Frontend::enterSharedWindow(ui::Screen& s, const std::string& state) {
    if (same(state, "opts_voice")) {
        m_voiceOptions.enter(*this, s);
    } else if (same(state, "opts_playlist")) {
        m_playlist.enter(*this, s);
    } else if (same(state, "opts_ctl_port")) {
        m_controllerPorts.enter(s);
    } else if (same(state, "opts_ctl_pc") || same(state, kEmptyWindow)) {
        // 0x0042E470: nothing.
    } else if (same(state, kPlainWindow) || same(state, kBackWindowA) || same(state, kBackWindowB)) {
        // 0x00473A10: the windows' enter (baseEnter) and nothing more.
    } else {
        return false;
    }
    return true;
}

bool Frontend::updateSharedWindow(ui::Screen& s, const std::string& state, uint32_t event) {
    if (same(state, "opts_voice")) {
        m_voiceOptions.update(*this, s, event);
    } else if (same(state, "opts_playlist")) {
        m_playlist.update(*this, s, event);
    } else if (same(state, "opts_ctl_port")) {
        m_controllerPorts.update(*this, s, event);
    } else if (same(state, "opts_ctl_pc")) {
        m_pcControls.update(*this, event);
    } else if (same(state, kBackWindowA) || same(state, kBackWindowB)) {
        m_backToCareerMenu.update(*this, event);
    } else if (same(state, kPlainWindow) || same(state, kEmptyWindow)) {
        // 0x0042E470: nothing.
    } else {
        return false;
    }
    return true;
}

bool Frontend::leaveSharedWindow(ui::Screen& s, const std::string& state) {
    if (same(state, "opts_voice")) {
        m_voiceOptions.leave();
        return true;
    }
    if (same(state, "opts_playlist")) {
        m_playlist.leave(s);
        return true;
    }
    return false;
}

}  // namespace oj
