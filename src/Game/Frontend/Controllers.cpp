#ifdef OJ_MACOS
#include "Mac/Controls.h"
#endif
// The watch of the players' controllers and the critical notes it opens.

#include "Game/Frontend/Frontend.h"
#include "Game/Frontend/FrontendInternal.h"

#include "Core/Base/Log.h"
#include "Core/Data/GameTexts.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Data/JetTexts.h"

namespace oj {

using namespace frontend;

// 0x005A8030: with no note up, a note of a lower priority up or the one up closing, the note opens (msgbox_critical,
// type 0: no buttons, nothing waits on it) with the text and the priority kept ([0x00722FEC]); its number (+0x108)
// is returned (0 when not opened).
bool Frontend::openCritical(uint32_t priority, const std::wstring& text) {
    if (m_critical && !(priority < m_criticalPriority) && !m_criticalClose) return false;
    Message m;
    m.screen = makeScreen("msgbox_critical");
    if (!m.screen) return false;
    m_criticalPriority = priority;
    m.state.type = ui::MessageType::Note;
    ui::openMessage(*m.screen, ui::MessageType::Note, text);
    log::info("frontend: note of a lost controller \"{}\"", toUtf8(text));
    m_critical = std::move(m);
    m_criticalClose = false;
    return true;
}

// 0x005A7F70: the note up closes when its priority is not above the one given.
void Frontend::closeCritical(uint32_t priority) {
    if (m_critical && priority <= m_criticalPriority) m_criticalClose = true;
}

bool Frontend::watchController(int player, bool connected) {
    bool& noNote = m_noNote[static_cast<size_t>(player)];
    if (connected) {
        if (noNote) return false;
        closeCritical(1);
        pauseGameSounds(false, m_sounds);
        m_sounds->setVolume(std::bit_cast<float>(settingsValue(save::Settings::kEffectsVolume)));
        // Both players' notes counted as closed (+0x1C and +0x24 set). With the front end's +0x30 (set by nothing
        // found) the network game would end here and menu be asked.
        m_noNote = {true, true};
        return true;
    }
    if (!noNote) return false;
    // 0x0046C0A4 .. 0x0046C11A: the text 1632 and "<device + 1>." made, then emptied before the note opens.
    const bool opened = openCritical(1, std::wstring());
    if (opened) noNote = false;
    if (net::NetGame* n = net::netGame(); n != nullptr && n->session.stage() < net::SessionStage::Loading)
        n->session.sendReady(0, 0);
    if (opened) {
        pauseGameSounds(true, m_sounds);
        m_sounds->setVolume(0.0f);
    }
    return false;
}

void Frontend::controllerUnwatched() {
    closeCritical(1);
}

void Frontend::watchSecondController(bool connected, int32_t device, bool opens) {
    if (connected) {
        if (m_noNote[1]) return;
        closeCritical(1);
        m_noNote = {true, true};
        return;
    }
    if (!m_noNote[1] || !opens) return;
    m_noNote[1] = !openCritical(1, m_gameTexts.at(kTextReconnectController) + std::format(L"{}.", device + 1));
}

// 0x004080D0 with 1 (then 0x00407FB0, the controller let go): the first player's controller made again with the
// DirectInput mapping window (the input manager's method +0x04: IDirectInput8::ConfigureDevices with DICD_EDIT,
// 0x00595717, [0x0075F770] set while it runs, the cursor shown for it; the game loop's, InputDevices::configure).
// Either way the devices' controllers are made again (0x005957D1: 0x00590EE0) and the map registered again.
void Frontend::controlsSetUp() {
#ifdef OJ_MACOS
    mac::requestControlsEditor();
#else
    log::info("frontend: the controls' set up: DirectInput's mapping window");
    m_inputConfigure = true;
    // 0x00480612 then 0x0048062C: the device made again with DRIVER1, then with the UI map.
    m_inputUi = true;
#endif
}

// 0x0059571E: the mapping window not given (Windows has had no such window since Vista: ConfigureDevices fails): the
// message box (MB_ICONHAND | MB_SETFOREGROUND | MB_TOPMOST, over the desktop) of scripts\jettext<letter>.txt's texts
// 111, a line's end and 112 ("There is a problem with your DirectInput registry settings." / "Please see the ReadMe
// file for further assistance."), titled with 113 ("DirectInput problem detected").
void Frontend::directInputProblem() {
    TextTable jet;
    jet.load(std::filesystem::path("scripts") / (std::string("jettext") + m_language.controlsLetter + ".txt"));
    const auto at = [&](size_t i) { return i < jet.size() ? jet.at(i) : std::wstring(); };
    errorBox(at(kJetDirectInputProblem) + L"\n" + at(kJetSeeReadMe), at(kJetDirectInputTitle), true);
}

}  // namespace oj
