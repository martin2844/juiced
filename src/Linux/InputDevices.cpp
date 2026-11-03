#include "Game/Platform/InputDevices.h"
#include "Core/Base/Log.h"
#include "Linux/Gamepad.h"
#include <array>
#include <set>
namespace oj {
namespace {
class Gamepads {
    std::array<SDL_Gamepad *, 4> pads{};
    std::set<SDL_JoystickID> ignored;

  public:
    ~Gamepads() {
        if (SDL_WasInit(SDL_INIT_GAMEPAD))
            for (auto *pad : pads)
                if (pad)
                    SDL_CloseGamepad(pad);
    }
    SDL_Gamepad *get(size_t slot) {
        for (auto *&pad : pads)
            if (pad && !SDL_GamepadConnected(pad)) {
                log::info("gamepad slot {} disconnected", &pad - pads.data() + 1);
                SDL_CloseGamepad(pad);
                pad = nullptr;
            }
        int count = 0;
        auto *ids = SDL_GetGamepads(&count);
        for (int i = 0; i < count; ++i) {
            if (ignored.contains(ids[i]))
                continue;
            if (std::any_of(pads.begin(), pads.end(), [&](auto *p) { return p && SDL_GetGamepadID(p) == ids[i]; }))
                continue;
            auto at = std::find(pads.begin(), pads.end(), nullptr);
            if (at == pads.end())
                break;
            auto *candidate = SDL_OpenGamepad(ids[i]);
            if (linux_port::hasDrivingAxes(candidate)) {
                *at = candidate;
                log::info("gamepad slot {}: {}", at - pads.begin() + 1, SDL_GetGamepadName(candidate));
            } else if (candidate) {
                ignored.insert(ids[i]);
                SDL_CloseGamepad(candidate);
            }
        }
        SDL_free(ids);
        return pads[slot];
    }
};
class SdlController final : public input::ControllerPort {
    SDL_Gamepad *pad = nullptr;
    Gamepads &pads;
    size_t slot;
    bool wasConnected = false;
    input::MapFormat format;
    bool ui = true;
    void connect() {
        pad = pads.get(slot);
    }

  public:
    SdlController(Gamepads &owner, size_t index) : pads(owner), slot(index) {}
    void unacquire() override {}
    void setActionMap(const input::MapFormat &f) override {
        format = f;
        ui = f.name.find("UI") != std::string::npos;
    }
    void setRawFormat() override {
        format = {};
    }
    void recreate() override {
        connect();
    }
    input::ControllerRead read(bool mapped) override {
        connect();
        const auto &settings = linux_port::controllerConfig();
        input::ControllerRead result;
        // Submit one neutral sample before reporting removal, clearing held input.
        result.ok = pad != nullptr || wasConnected;
        wasConnected = pad != nullptr;
        auto button = [&](SDL_GamepadButton b) {
            b = settings.button(b);
            return pad && SDL_GetGamepadButton(pad, b) ? 128 : 0;
        };
        auto axis = [&](SDL_GamepadAxis a) {
            if (!pad)
                return 0;
            int v = SDL_GetGamepadAxis(pad, a);
            int percent = std::clamp(v * 100 / 32767, -100, 100);
            if (a == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || a == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
                return percent;
            if (std::abs(percent) <= settings.deadzone)
                return 0;
            return (percent < 0 ? -1 : 1) * (std::abs(percent) - settings.deadzone) * 100 / (100 - settings.deadzone);
        };
        if (!mapped) {
            if (pad)
                for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++)
                    result.buttons[i] = button((SDL_GamepadButton)i);
            return result;
        }
        for (auto &a : format.actions) {
            if ((a.semantic >> 24) == 0x81)
                continue; // keyboard values are supplied independently
            int value = 0;
            if (!ui && a.semantic >= 0x7F000100u && a.semantic <= 0x7F000105u) {
                const bool third = button(SDL_GAMEPAD_BUTTON_BACK) != 0;
                const SDL_GamepadButton directions[] = {SDL_GAMEPAD_BUTTON_DPAD_UP,   SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
                                                        SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT,
                                                        SDL_GAMEPAD_BUTTON_DPAD_UP,   SDL_GAMEPAD_BUTTON_DPAD_DOWN};
                const auto slot = a.semantic - 0x7F000100u;
                value = (third == (slot >= 4)) ? button(directions[slot]) : 0;
                result.events.emplace_back(a.appData, static_cast<uint32_t>(value));
                continue;
            }
            switch (a.semantic) {
            case 0xFF00C201u:
                value = axis(SDL_GAMEPAD_AXIS_LEFTX);
                if (ui && button(SDL_GAMEPAD_BUTTON_DPAD_LEFT))
                    value = -100;
                if (ui && button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
                    value = 100;
                break;
            case 0xFF014201u:
                if (ui) {
                    value = axis(SDL_GAMEPAD_AXIS_LEFTY);
                    if (button(SDL_GAMEPAD_BUTTON_DPAD_UP))
                        value = -100;
                    if (button(SDL_GAMEPAD_BUTTON_DPAD_DOWN))
                        value = 100;
                }
                break;
            case 0xFF00C202u:
                value = axis(SDL_GAMEPAD_AXIS_RIGHTX);
                break;
            case 0xFF014202u:
                value = axis(SDL_GAMEPAD_AXIS_RIGHTY);
                break;
            case 0x01039202u:
                value = -std::max(0, axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
                break;
            case 0x01041203u:
                value = -std::max(0, axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
                break;
            case 0xFF004400u:
                value = button(SDL_GAMEPAD_BUTTON_SOUTH);
                break;
            case 0xFF004401u:
                value = button(SDL_GAMEPAD_BUTTON_NORTH);
                break;
            case 0xFF004402u:
                value = button(SDL_GAMEPAD_BUTTON_EAST);
                break;
            case 0xFF004404u:
                value = button(SDL_GAMEPAD_BUTTON_WEST);
                break;
            case 0xFF004405u:
                value = button(SDL_GAMEPAD_BUTTON_LEFT_STICK);
                break;
            case 0xFF004407u:
                value = button(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
                break;
            case 0x010004FDu:
            case 0x010044FCu:
                value = button(SDL_GAMEPAD_BUTTON_START);
                break;
            case 0x01000C02u:
                value = button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
                break;
            case 0x01000C01u:
                value = button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
                break;
            default:
                break;
            }
            result.events.emplace_back(a.appData, static_cast<uint32_t>(value));
        }
        return result;
    }
};

} // namespace
struct InputDevices::State {
    Gamepads pads;
    std::array<SdlController, 4> controllers{{{pads, 0}, {pads, 1}, {pads, 2}, {pads, 3}}};
};
InputDevices::InputDevices() : m_state(std::make_unique<State>()) {}
InputDevices::~InputDevices() = default;
bool InputDevices::start(HWND) {
    return SDL_InitSubSystem(SDL_INIT_GAMEPAD);
}
std::vector<input::ControllerPort *> InputDevices::controllers() const {
    // A dedicated keyboard port plus four stable, hot-pluggable gamepad slots.
    std::vector<input::ControllerPort *> ports{nullptr};
    for (auto &pad : m_state->controllers)
        ports.push_back(&pad);
    return ports;
}
input::KeyboardPort *InputDevices::keyboard() const {
    return nullptr;
}
bool InputDevices::configure(const input::MapFormat &) {
    return false;
}
void InputDevices::startWheels(HWND, WheelInput*) {}
void InputDevices::stopWheels() {}
} // namespace oj
