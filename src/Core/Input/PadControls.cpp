#include "Core/Input/PadControls.h"

#include <ControlsFile.h>
#if defined(_WIN32)
#include <WheelReader.h>
#endif

#include <string>

namespace oj::input {

namespace {

// The keys of the poll the actions read (PadActions' sources are plain functions).
const std::array<bool, 256>* g_keys = nullptr;

bool keyDown(const ControllerLayout::Key& key) {
    return g_keys != nullptr && key.scan >= 0 && key.scan < 256 && (*g_keys)[static_cast<size_t>(key.scan)];
}

// And the wheels' reader.
WheelReader::Reader* g_wheel = nullptr;

float wheelValue(int action) {
#if defined(_WIN32)
    return g_wheel != nullptr ? g_wheel->Value(action) : 0.0f;
#else
    return 0.0f; // Native SDL ports read wheels/controllers through ControllerPort.
#endif
}

bool wheelDown(int action) {
#if defined(_WIN32)
    return g_wheel != nullptr && g_wheel->Down(action);
#else
    return false;
#endif
}

bool wheelMenuDown(int menu) {
#if defined(_WIN32)
    return g_wheel != nullptr && g_wheel->MenuDown(menu);
#else
    return false;
#endif
}

constexpr uint32_t kConfirm = 0x1000 | 0x0010;  // XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_START

}  // namespace

const PadControls::Control* PadControls::find(std::string_view semantic) {
    return PadActions::Find(std::string(semantic).c_str());
}

void PadControls::setLayout(const ControllerLayout::Layout& layout) {
    m_layout = layout;
    m_dirty = true;
}

void PadControls::setGameKeys(const ControllerLayout::KeySlot& keys) {
    m_gameKeys = keys;
    m_dirty = true;
}

// The plugin's RefreshLayout: the actions resolved again when the layout or the controller's kind changed.
void PadControls::resolve() {
    const int kind = m_pad.playStation ? 1 : 0;
    if (!m_dirty && kind == m_kind) return;
    m_dirty = false;
    m_kind = kind;
    PadActions::Sources sources;
    sources.keyDown = &keyDown;
    sources.wheelValue = &wheelValue;
    sources.wheelDown = &wheelDown;
    sources.wheelMenuDown = &wheelMenuDown;
    m_actions.SetSources(sources);
    m_actions.Resolve(m_layout, kind, m_gameKeys);
}

void PadControls::update(const PadState& pad, const std::array<bool, 256>& keys, float dt) {
    m_pad = pad;
    if (!m_pad.connected) m_pad.pad = PadActions::Pad{};
    m_keys = keys;
    m_seconds += dt;
    resolve();
}

bool PadControls::down(const Control& control) {
    g_keys = &m_keys;
    g_wheel = m_wheel;
    return m_actions.IsDown(control, m_pad.pad, m_controllerOn, m_seconds);
}

float PadControls::value(const Control& control) {
    g_keys = &m_keys;
    g_wheel = m_wheel;
    return m_actions.Value(control, m_pad.pad, m_controllerOn, m_seconds);
}

bool PadControls::steeringFromController() const {
    static const int steering = ControllerLayout::FindAction("STEERING");
    return m_actions.FromController(steering);
}

bool PadControls::confirmDown() const {
    return (m_controllerOn && (m_pad.pad.buttons & kConfirm) != 0)
#if defined(_WIN32)
        || (m_wheel != nullptr && m_wheel->ConfirmDown())
#endif
        ;
}

std::string mergeLayout(std::string_view controls, bool controller, ControllerLayout::KeySlot& gameKeys) {
    const std::string without = ControlsFile::WithoutDriverKeys(std::string(controls), gameKeys);
    int merged = 0;
    return ControlsFile::Merge(without, ControlsFile::DefaultLayout(controller), merged);
}

}  // namespace oj::input
