#include "Game/Platform/InputDevices.h"

#include "Core/Base/Log.h"

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <array>
#include <cstring>
#include <format>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace oj {

namespace {

// A map's DIACTIONFORMAT (0x005954D0): the device's DIACTIONs (guidInstance GUID_NULL, dwObjID and dwHow 0 as made),
// the map's GUID and name, its genre, buffer and axes; the actions' names kept with it.
struct DiFormat {
    std::vector<std::string> labels;
    std::vector<DIACTIONA> actions;
    DIACTIONFORMATA format{};

    explicit DiFormat(const input::MapFormat& f) {
        actions.assign(f.actions.size(), DIACTIONA{});
        for (const input::MapAction& a : f.actions) labels.push_back(a.label);
        for (size_t i = 0; i < f.actions.size(); ++i) {
            DIACTIONA& d = actions[i];
            d.uAppData = f.actions[i].appData;
            d.dwSemantic = f.actions[i].semantic;
            d.dwFlags = f.actions[i].flags;
            d.lptszActionName = labels[i].c_str();
            d.guidInstance = GUID_NULL;
            d.dwObjID = 0;
            d.dwHow = 0;
        }
        format.dwSize = sizeof(format);
        format.dwActionSize = sizeof(DIACTIONA);
        format.dwDataSize = static_cast<DWORD>(f.actions.size() * 4);
        format.dwNumActions = static_cast<DWORD>(f.actions.size());
        format.rgoAction = actions.empty() ? nullptr : actions.data();
        std::memcpy(&format.guidActionMap, f.guid.data(), sizeof(GUID));
        format.dwGenre = f.genre;
        format.dwBufferSize = f.bufferSize;
        format.lAxisMin = f.axisMin;
        format.lAxisMax = f.axisMax;
        format.hInstString = nullptr;
        strncpy_s(format.tszActionMap, f.name.c_str(), _TRUNCATE);
    }
    DiFormat(const DiFormat&) = delete;
    DiFormat& operator=(const DiFormat&) = delete;
};

// A device's game controller (its +0x3C): an IDirectInputDevice8 of the input manager's DirectInput.
class DiController final : public input::ControllerPort {
public:
    DiController(IDirectInput8A* di, IDirectInputDevice8A* device, const GUID& instance, HWND window)
        : m_di(di), m_device(device), m_instance(instance), m_window(window) {}
    ~DiController() override {
        if (m_device != nullptr) {
            m_device->Unacquire();
            m_device->Release();
        }
    }

    void unacquire() override {
        if (m_device != nullptr) m_device->Unacquire();
    }

    // 0x005954D0: the map's DIACTIONFORMAT: BuildActionMap, then SetActionMap.
    void setActionMap(const input::MapFormat& f) override {
        if (m_device == nullptr) return;
        m_format = std::make_unique<DiFormat>(f);
        const HRESULT built = m_device->BuildActionMap(&m_format->format, nullptr, 0);
        const HRESULT set = m_device->SetActionMap(&m_format->format, nullptr, 0);
        log::info("input: controller map {} ({} actions): BuildActionMap 0x{:08X}, SetActionMap 0x{:08X}", f.name,
                  f.actions.size(), static_cast<uint32_t>(built), static_cast<uint32_t>(set));
    }

    void setRawFormat() override {
        if (m_device != nullptr) m_device->SetDataFormat(&c_dfDIJoystick2);
    }

    // 0x00594C20's controller part: Poll; failing, Acquire; that failing too, Unacquire, whose result counts. Then with
    // a map GetDeviceData (room for 32), else GetDeviceState (DIJOYSTATE2). The device reads as connected while the
    // last result is not an error.
    input::ControllerRead read(bool mapped) override {
        input::ControllerRead r;
        if (m_device == nullptr) {
            r.ok = false;
            return r;
        }
        HRESULT hr = m_device->Poll();
        if (FAILED(hr)) {
            hr = m_device->Acquire();
            if (FAILED(hr)) {
                hr = m_device->Unacquire();
                r.ok = SUCCEEDED(hr);
                return r;
            }
        }
        if (mapped) {
            std::array<DIDEVICEOBJECTDATA, 32> data{};
            DWORD count = static_cast<DWORD>(data.size());
            hr = m_device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), data.data(), &count, 0);
            if (count != 0 && SUCCEEDED(hr)) {
                for (DWORD i = 0; i < count; ++i) {
                    r.events.emplace_back(static_cast<uint32_t>(data[i].uAppData), data[i].dwData);
                }
            }
        } else {
            DIJOYSTATE2 state{};
            hr = m_device->GetDeviceState(sizeof(state), &state);
            if (SUCCEEDED(hr)) std::memcpy(r.buttons.data(), state.rgbButtons, r.buttons.size());
        }
        r.ok = SUCCEEDED(hr);
        return r;
    }

    // 0x00596210: released and made again from its instance GUID; c_dfDIJoystick2, cooperative level 0x16
    // (DISCL_NONEXCLUSIVE | DISCL_FOREGROUND | DISCL_NOWINKEY; Stock problem: DISCL_NOWINKEY is a keyboard's, so
    // DirectInput refuses it and the level stays its default).
    void recreate() override {
        if (m_device == nullptr) return;
        m_device->Release();
        m_device = nullptr;
        m_di->CreateDevice(m_instance, &m_device, nullptr);
        if (m_device == nullptr) return;
        m_device->SetDataFormat(&c_dfDIJoystick2);
        m_device->SetCooperativeLevel(m_window, 0x16);
    }

private:
    IDirectInput8A* m_di = nullptr;
    IDirectInputDevice8A* m_device = nullptr;
    GUID m_instance{};
    HWND m_window = nullptr;
    std::unique_ptr<DiFormat> m_format;
};

// The input manager's keyboard (its +0x40, GUID_SysKeyboard) as far as the maps go: 0x005954D0's BuildActionMap and
// SetActionMap on it give each action its key, DirectInput's own map of the user's (the controls' mapping window's,
// kept by DirectInput) when there is one. The keys themselves are read from the window's messages.
class DiKeyboard final : public input::KeyboardPort {
public:
    explicit DiKeyboard(IDirectInputDevice8A* device) : m_device(device) {}
    ~DiKeyboard() override {
        if (m_device != nullptr) m_device->Release();
    }

    std::vector<std::pair<uint8_t, uint32_t>> setActionMap(const input::MapFormat& f) override {
        std::vector<std::pair<uint8_t, uint32_t>> keys;
        if (m_device == nullptr) return keys;
        DiFormat d(f);
        const HRESULT built = m_device->BuildActionMap(&d.format, nullptr, 0);
        m_device->SetActionMap(&d.format, nullptr, 0);
        size_t user = 0;
        for (const DIACTIONA& a : d.actions) {
            if (a.dwHow == DIAH_UNMAPPED || a.guidInstance != GUID_SysKeyboard) continue;
            // A key's object ID holds its DIK code as the instance.
            keys.emplace_back(static_cast<uint8_t>(DIDFT_GETINSTANCE(a.dwObjID) & 0xFF),
                              static_cast<uint32_t>(a.uAppData));
            if ((a.dwHow & DIAH_USERCONFIG) != 0) ++user;
        }
        log::info("input: keyboard map {}: BuildActionMap 0x{:08X}, {} keys ({} of the user's map)", f.name,
                  static_cast<uint32_t>(built), keys.size(), user);
        return keys;
    }

private:
    IDirectInputDevice8A* m_device = nullptr;
};


// A channel of the FF manager: an IDirectInputDevice8 of its own DirectInput and the effects made on it
// (IDirectInputEffect, the handle its pointer).
class DiWheel final : public ff::WheelDevice {
public:
    explicit DiWheel(IDirectInputDevice8A* device) : m_device(device) {}

    int32_t acquire() override { return m_device->Acquire(); }
    int32_t unacquire() override { return m_device->Unacquire(); }
    int32_t poll() override { return m_device->Poll(); }
    int32_t actuatorsOn() override { return m_device->SendForceFeedbackCommand(DISFFC_SETACTUATORSON); }
    int32_t state(std::array<uint8_t, 0x110>& out) override {
        static_assert(sizeof(DIJOYSTATE2) == 0x110);
        return m_device->GetDeviceState(sizeof(DIJOYSTATE2), out.data());
    }
    // DIPROP_FFGAIN of the device (DIPH_DEVICE).
    int32_t gain(uint32_t& out) override {
        DIPROPDWORD p{};
        p.diph.dwSize = sizeof(p);
        p.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        p.diph.dwHow = DIPH_DEVICE;
        const HRESULT hr = m_device->GetProperty(DIPROP_FFGAIN, &p.diph);
        out = p.dwData;
        return hr;
    }
    int32_t setGain(uint32_t value) override {
        DIPROPDWORD p{};
        p.diph.dwSize = sizeof(p);
        p.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        p.diph.dwHow = DIPH_DEVICE;
        p.dwData = value;
        return m_device->SetProperty(DIPROP_FFGAIN, &p.diph);
    }
    int32_t createEffect(const ff::EffectParams& p, uintptr_t& handle) override {
        Effect e(p);
        IDirectInputEffect* effect = nullptr;
        const HRESULT hr = m_device->CreateEffect(typeOf(p.type), &e.effect, &effect, nullptr);
        handle = reinterpret_cast<uintptr_t>(effect);
        return hr;
    }
    // DIEP_TYPESPECIFICPARAMS | DIEP_DIRECTION (0x140).
    int32_t setParameters(uintptr_t handle, const ff::EffectParams& p) override {
        Effect e(p);
        return effectOf(handle)->SetParameters(&e.effect, DIEP_TYPESPECIFICPARAMS | DIEP_DIRECTION);
    }
    int32_t start(uintptr_t handle) override { return effectOf(handle)->Start(1, 0); }
    int32_t stop(uintptr_t handle) override { return effectOf(handle)->Stop(); }
    int32_t status(uintptr_t handle, uint32_t& out) override {
        DWORD flags = 0;
        const HRESULT hr = effectOf(handle)->GetEffectStatus(&flags);
        out = flags;
        return hr;
    }
    int32_t unload(uintptr_t handle) override { return effectOf(handle)->Unload(); }
    void releaseEffect(uintptr_t handle) override { effectOf(handle)->Release(); }
    void release() override {
        m_device->Unacquire();
        m_device->Release();
        m_device = nullptr;
    }

private:
    // A DIEFFECT of the parameters with its axes, directions, envelope and type's parameters.
    struct Effect {
        DIEFFECT effect{};
        DWORD axes[2]{};
        LONG directions[2]{};
        DIENVELOPE envelope{};
        DICONSTANTFORCE constant{};
        DIPERIODIC periodic{};
        DICONDITION conditions[2]{};

        explicit Effect(const ff::EffectParams& p) {
            axes[0] = p.axes[0];
            axes[1] = p.axes[1];
            directions[0] = p.directions[0];
            directions[1] = p.directions[1];
            envelope = {sizeof(DIENVELOPE), p.attackLevel, p.attackTime, p.fadeLevel, p.fadeTime};
            effect.dwSize = sizeof(DIEFFECT);
            effect.dwFlags = p.flags;
            effect.dwDuration = p.duration;
            effect.dwSamplePeriod = p.samplePeriod;
            effect.dwGain = p.gain;
            effect.dwTriggerButton = p.triggerButton;
            effect.dwTriggerRepeatInterval = p.triggerRepeat;
            effect.cAxes = p.axisCount;
            effect.rgdwAxes = axes;
            effect.rglDirection = directions;
            effect.lpEnvelope = p.envelope ? &envelope : nullptr;
            effect.dwStartDelay = p.startDelay;
            switch (p.type) {
            case ff::EffectType::Constant:
                constant.lMagnitude = p.magnitude;
                effect.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
                effect.lpvTypeSpecificParams = &constant;
                break;
            case ff::EffectType::Spring:
            case ff::EffectType::Damper:
                for (size_t i = 0; i < 2; ++i) {
                    const ff::Condition& c = p.conditions[i];
                    conditions[i] = {c.offset, c.positiveCoefficient, c.negativeCoefficient, c.positiveSaturation,
                                     c.negativeSaturation, c.deadBand};
                }
                effect.cbTypeSpecificParams = static_cast<DWORD>(p.axisCount * sizeof(DICONDITION));
                effect.lpvTypeSpecificParams = conditions;
                break;
            default:
                periodic = {static_cast<DWORD>(p.magnitude), p.offset, p.phase, p.period};
                effect.cbTypeSpecificParams = sizeof(DIPERIODIC);
                effect.lpvTypeSpecificParams = &periodic;
                break;
            }
        }
    };

    static const GUID& typeOf(ff::EffectType t) {
        switch (t) {
        case ff::EffectType::Constant: return GUID_ConstantForce;
        case ff::EffectType::Square: return GUID_Square;
        case ff::EffectType::Sine: return GUID_Sine;
        case ff::EffectType::Triangle: return GUID_Triangle;
        case ff::EffectType::SawtoothUp: return GUID_SawtoothUp;
        case ff::EffectType::SawtoothDown: return GUID_SawtoothDown;
        case ff::EffectType::Spring: return GUID_Spring;
        case ff::EffectType::Damper: return GUID_Damper;
        default: return GUID_NULL;
        }
    }
    static IDirectInputEffect* effectOf(uintptr_t handle) { return reinterpret_cast<IDirectInputEffect*>(handle); }

    IDirectInputDevice8A* m_device = nullptr;
};

}  // namespace

struct InputDevices::State {
    IDirectInput8A* di = nullptr;
    IDirectInputDevice8A* mouse = nullptr;
    HWND window = nullptr;
    std::vector<IDirectInputDevice8A*> found;  // +0x40, the devices made as they are listed
    std::vector<GUID> instances;               // +0x60
    std::vector<IDirectInputDevice8A*> forceFeedback;  // +0x50
    std::vector<std::unique_ptr<DiController>> controllers;  // the devices' +0x3C
    std::unique_ptr<DiKeyboard> keyboard;                    // the input manager's +0x40, for the maps
    bool comInitialized = false;
    // The FF manager's DirectInput ([0x0075F748]) and its channels' devices ([0x0075D9AC]) with their kinds
    // ([0x0075D9B4] a joystick, [0x0075D9A8] a gamepad, [0x0075D9A4] a wheel, by DI8DEVTYPE; read by nothing) and
    // force feedback axes ([0x0075D99C]).
    IDirectInput8A* wheelInput = nullptr;
    std::array<IDirectInputDevice8A*, 2> wheelDevices{};
    std::array<std::array<bool, 3>, 2> wheelKinds{};
    std::array<DWORD, 2> wheelAxes{};
    std::array<std::unique_ptr<DiWheel>, 2> wheels;
};

namespace {

// 0x00591770 (by 0x00590EC0): an axis object (DIDFT_AXIS) gets the range -1000 .. 1000 (DIPROP_RANGE by its ID).
// Stock problem: the range is set on the first controller listed, whichever controller's objects are listed.
BOOL CALLBACK axisRange(LPCDIDEVICEOBJECTINSTANCEA object, LPVOID context) {
    auto* s = static_cast<InputDevices::State*>(context);
    if ((object->dwType & DIDFT_AXIS) == 0) return DIENUM_CONTINUE;
    DIPROPRANGE range{};
    range.diph.dwSize = sizeof(range);
    range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    range.diph.dwObj = object->dwType;
    range.diph.dwHow = DIPH_BYID;
    range.lMin = -1000;
    range.lMax = 1000;
    if (FAILED(s->found.front()->SetProperty(DIPROP_RANGE, &range.diph))) return DIENUM_STOP;
    return DIENUM_CONTINUE;
}

// 0x00591650 (by 0x00590EA0): the controller made (CreateDevice), kept with its instance GUID; one with force feedback
// (DIDC_FORCEFEEDBACK) noted ("Found force feedback device!", the debugger's output) and kept again; c_dfDIJoystick2;
// cooperative level 0x14 (Stock problem: DISCL_FOREGROUND | DISCL_NOWINKEY, neither exclusive nor not, which DirectInput
// refuses, so the default level stays); its axes' ranges (0x00591770).
BOOL CALLBACK deviceFound(LPCDIDEVICEINSTANCEA instance, LPVOID context) {
    auto* s = static_cast<InputDevices::State*>(context);
    IDirectInputDevice8A* device = nullptr;
    s->di->CreateDevice(instance->guidInstance, &device, nullptr);
    s->found.push_back(device);
    s->instances.push_back(instance->guidInstance);
    if (device == nullptr) return DIENUM_CONTINUE;
    DIDEVCAPS caps{};
    caps.dwSize = sizeof(caps);
    if (SUCCEEDED(device->GetCapabilities(&caps)) && (caps.dwFlags & DIDC_FORCEFEEDBACK) != 0) {
        log::info("Found force feedback device!");
        s->forceFeedback.push_back(device);
    }
    device->SetDataFormat(&c_dfDIJoystick2);
    device->SetCooperativeLevel(s->window, 0x14);
    device->EnumObjects(&axisRange, s, DIDFT_ALL);
    log::info("input: game controller \"{}\"", instance->tszInstanceName);
    return DIENUM_CONTINUE;
}

}  // namespace

InputDevices::InputDevices() : m_state(std::make_unique<State>()) {}

InputDevices::~InputDevices() {
    stopWheels();
    State& s = *m_state;
    s.controllers.clear();
    s.keyboard.reset();
    if (s.mouse != nullptr) {
        s.mouse->Unacquire();
        s.mouse->Release();
    }
    if (s.di != nullptr) s.di->Release();
    if (s.comInitialized) CoUninitialize();
}

bool InputDevices::start(HWND window) {
    State& s = *m_state;
    s.window = window;
    s.comInitialized = SUCCEEDED(CoInitialize(nullptr));
    if (FAILED(DirectInput8Create(GetModuleHandleA(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8A,
                                  reinterpret_cast<void**>(&s.di), nullptr))) {
        s.di = nullptr;
        log::warning("input: no DirectInput");
        return false;
    }
    if (FAILED(s.di->EnumDevices(DI8DEVCLASS_GAMECTRL, &deviceFound, &s, DIEDFL_ATTACHEDONLY))) {
        log::warning("input: the game controllers not listed");
    }
    // The mouse: c_dfDIMouse, cooperative level 0x0A (DISCL_NONEXCLUSIVE | DISCL_BACKGROUND), never acquired nor read.
    if (SUCCEEDED(s.di->CreateDevice(GUID_SysMouse, &s.mouse, nullptr)) && s.mouse != nullptr) {
        s.mouse->SetDataFormat(&c_dfDIMouse);
        s.mouse->SetCooperativeLevel(window, 0x0A);
    }
    // The keyboard (GUID_SysKeyboard), for the maps only: never acquired, its keys come from the window.
    IDirectInputDevice8A* keyboard = nullptr;
    if (SUCCEEDED(s.di->CreateDevice(GUID_SysKeyboard, &keyboard, nullptr)) && keyboard != nullptr) {
        s.keyboard = std::make_unique<DiKeyboard>(keyboard);
    }
    // A device for each controller (0x00594950 holds its own reference; the list's is let go).
    for (size_t i = 0; i < s.found.size(); ++i) {
        if (s.found[i] == nullptr) continue;
        s.controllers.push_back(std::make_unique<DiController>(s.di, s.found[i], s.instances[i], window));
    }
    log::info("input: {} game controller(s), {} with force feedback", s.controllers.size(), s.forceFeedback.size());
    s.found.clear();
    s.instances.clear();
    return true;
}

std::vector<input::ControllerPort*> InputDevices::controllers() const {
    std::vector<input::ControllerPort*> out;
    for (const auto& c : m_state->controllers) out.push_back(c.get());
    return out;
}

input::KeyboardPort* InputDevices::keyboard() const {
    return m_state->keyboard.get();
}

bool InputDevices::configure(const input::MapFormat& f) {
    State& s = *m_state;
    if (s.di == nullptr) return false;
    DiFormat d(f);
    DICONFIGUREDEVICESPARAMSA p{};
    p.dwSize = sizeof(p);
    p.dwcUsers = 0;
    p.lptszUserNames = nullptr;
    p.dwcFormats = 1;
    p.lprgFormats = &d.format;
    p.hwnd = GetDesktopWindow();
    p.dics.dwSize = sizeof(DICOLORSET);
    p.dics.cTextFore = 0xFF9CC4CE;
    p.dics.cTextHighlight = 0xFFECEC01;
    p.dics.cCalloutLine = 0xFFECEC01;
    p.dics.cCalloutHighlight = 0xFFECEC01;
    p.dics.cBorder = 0xFFECEC01;
    p.dics.cControlFill = 0xFF18536D;
    p.dics.cHighlightFill = 0xFF18536D;
    p.dics.cAreaFill = 0xFF253E4A;
    p.lpUnkDDSTarget = nullptr;
    const HRESULT hr = s.di->ConfigureDevices(nullptr, &p, DICD_EDIT, nullptr);
    log::info("input: the controls' mapping window (ConfigureDevices, {}): 0x{:08X}", f.name,
              static_cast<uint32_t>(hr));
    return hr == DI_OK;
}

namespace {

void wheelSay(const std::string& text) {
    log::debugger(text);
}

// 0x00596980 -> 0x005968B0: a game controller listed takes the first free channel (none: passed over), made
// (CreateDevice), its kind kept by its type (DI8DEVTYPE_JOYSTICK, _GAMEPAD, _DRIVING) and its autocenter turned off
// (DIPROP_AUTOCENTER, DIPROPAUTOCENTER_OFF).
BOOL CALLBACK wheelFound(LPCDIDEVICEINSTANCEA instance, LPVOID context) {
    auto* s = static_cast<InputDevices::State*>(context);
    size_t at = 0;
    while (at < 2 && s->wheelDevices[at] != nullptr) ++at;
    if (at == 2) return DIENUM_CONTINUE;
    if (FAILED(s->wheelInput->CreateDevice(instance->guidInstance, &s->wheelDevices[at], nullptr))) {
        s->wheelDevices[at] = nullptr;
        wheelSay(std::format("ERROR: Failed to obtain an interface for the enumerated FF device {}.\n", at));
        return DIENUM_CONTINUE;
    }
    switch (instance->dwDevType & 0xFF) {
    case DI8DEVTYPE_JOYSTICK: s->wheelKinds[at][0] = true; break;
    case DI8DEVTYPE_GAMEPAD: s->wheelKinds[at][1] = true; break;
    case DI8DEVTYPE_DRIVING: s->wheelKinds[at][2] = true; break;
    default: break;
    }
    DIPROPDWORD off{};
    off.diph.dwSize = sizeof(off);
    off.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    off.diph.dwHow = DIPH_DEVICE;
    off.dwData = DIPROPAUTOCENTER_OFF;
    if (FAILED(s->wheelDevices[at]->SetProperty(DIPROP_AUTOCENTER, &off.diph))) {
        wheelSay(std::format(
            "ERROR: failed to disable autocenter property for device {}. OK if non FF wheel plugged in.\n", at));
    }
    log::info("force feedback: channel {} \"{}\"", at, instance->tszInstanceName);
    return DIENUM_CONTINUE;
}

// 0x005969A0: an axis with an actuator (DIDOI_FFACTUATOR) counted, at most 2.
BOOL CALLBACK wheelAxis(LPCDIDEVICEOBJECTINSTANCEA object, LPVOID context) {
    auto* count = static_cast<DWORD*>(context);
    if ((object->dwFlags & DIDOI_FFACTUATOR) != 0) ++*count;
    if (*count > 2) *count = 2;
    return DIENUM_CONTINUE;
}

// 0x00596AC0 -> 0x00596A50: an axis object (DIDFT_AXIS) gets the range -10000 .. 10000 on its device; a failure ends
// the listing.
BOOL CALLBACK wheelRange(LPCDIDEVICEOBJECTINSTANCEA object, LPVOID context) {
    auto* device = static_cast<IDirectInputDevice8A*>(context);
    if (device == nullptr || (object->dwType & DIDFT_AXIS) == 0) return DIENUM_CONTINUE;
    DIPROPRANGE range{};
    range.diph.dwSize = sizeof(range);
    range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    range.diph.dwObj = object->dwType;
    range.diph.dwHow = DIPH_BYID;
    range.lMin = -10000;
    range.lMax = 10000;
    if (FAILED(device->SetProperty(DIPROP_RANGE, &range.diph))) {
        wheelSay("ERROR: SetProperty failed\n");
        return DIENUM_STOP;
    }
    return DIENUM_CONTINUE;
}

}  // namespace

void InputDevices::startWheels(HWND window) {
    State& s = *m_state;
    ff::Wheels& wheels = m_wheels;
    m_wheelsStarted = true;
    s.wheelDevices = {};
    s.wheelKinds = {};
    s.wheelAxes = {};
    const auto opened = [&] {
        std::array<ff::WheelDevice*, 2> devices{};
        for (size_t i = 0; i < 2; ++i) {
            s.wheels[i].reset();
            if (s.wheelDevices[i] == nullptr) continue;
            s.wheels[i] = std::make_unique<DiWheel>(s.wheelDevices[i]);
            devices[i] = s.wheels[i].get();
        }
        wheels.open(devices, {s.wheelAxes[0], s.wheelAxes[1]});
    };
    if (window == nullptr) {
        opened();
        return;
    }
    if (FAILED(DirectInput8Create(GetModuleHandleA(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8A,
                                  reinterpret_cast<void**>(&s.wheelInput), nullptr))) {
        s.wheelInput = nullptr;
        wheelSay("ERROR: failed to create a DInput object\n");
        opened();
        return;
    }
    // 0x00596790: the attached game controllers (not only those with force feedback); the first channel's then the
    // second's force feedback axes (a failure ends it there).
    if (FAILED(s.wheelInput->EnumDevices(DI8DEVCLASS_GAMECTRL, &wheelFound, &s, DIEDFL_ATTACHEDONLY))) {
        wheelSay("ERROR: could not enumerate devices\n");
    } else {
        for (size_t i = 0; i < 2 && s.wheelDevices[i] != nullptr; ++i) {
            DWORD count = 0;
            if (FAILED(s.wheelDevices[i]->EnumObjects(&wheelAxis, &count, DIDFT_AXIS))) {
                wheelSay(std::format("ERROR: failed to enumerate force feedback axes for device {}\n", i));
                if (i == 0) s.wheelDevices[1] = nullptr;
                break;
            }
            s.wheelAxes[i] = count;
        }
    }
    // 0x005969E0: c_dfDIJoystick2 and the cooperative level 5 (DISCL_EXCLUSIVE | DISCL_FOREGROUND) for each; a failure
    // ends it there.
    for (size_t i = 0; i < 2; ++i) {
        IDirectInputDevice8A* d = s.wheelDevices[i];
        if (d == nullptr) continue;
        if (FAILED(d->SetDataFormat(&c_dfDIJoystick2))) {
            wheelSay(std::format("ERROR: couldn't set data format for wheel on channel {}\n", i));
            break;
        }
        if (FAILED(d->SetCooperativeLevel(window, DISCL_EXCLUSIVE | DISCL_FOREGROUND))) {
            wheelSay(std::format("ERROR: couldn't set cooperative level for wheel on channel {}\n", i));
            break;
        }
    }
    // 0x00591F80: each channel's axes -10000 .. 10000.
    for (size_t i = 0; i < 2; ++i) {
        IDirectInputDevice8A* d = s.wheelDevices[i];
        if (d == nullptr) continue;
        if (FAILED(d->EnumObjects(&wheelRange, d, DIDFT_ALL))) {
            wheelSay(std::format("ERROR: couldn't enumerate objects for wheel on channel {}\n", i));
        }
    }
    opened();
    log::info("force feedback: {} channel(s), force feedback axes {} and {}",
              (s.wheelDevices[0] != nullptr ? 1 : 0) + (s.wheelDevices[1] != nullptr ? 1 : 0), s.wheelAxes[0],
              s.wheelAxes[1]);
}

void InputDevices::stopWheels() {
    State& s = *m_state;
    if (!m_wheelsStarted) return;
    m_wheelsStarted = false;
    m_wheels.shutdown();
    m_wheels.releaseAll();
    s.wheels = {};
    s.wheelDevices = {};
    if (s.wheelInput != nullptr) {
        s.wheelInput->Release();
        s.wheelInput = nullptr;
    }
}

}  // namespace oj
