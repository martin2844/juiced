#include "Game/Platform/Platform.h"
#include "Android/Gamepad.h"
#include "Android/TouchControls.h"
#include "Audio/SoundSystem.h"
#include "Core/Base/Log.h"
#include "Core/Base/Text.h"
#include "Game/Platform/CommandLine.h"
#include "Game/Platform/InputDevices.h"
#include "Mods/Creative.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <fstream>
#include <map>
#ifdef __ANDROID__
#include <jni.h>
#endif
#include <windows.h>
namespace oj {
namespace {
std::vector<input::KeyEvent> events;
bool focused = true, screenshot = false;
FreeCameraMouse mouse;
bool mouseHook = false;
Language startupLanguage{};
bool overlayActive = false;
SDL_Gamepad *overlayPad = nullptr;
std::array<bool, 6> overlayHeld{};
android::TouchControls touch;
std::array<bool, android::TouchControls::Count> priorTouch{};
uint8_t scan(SDL_Scancode c) {
    static const uint8_t letters[] = {30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38, 50,
                                      49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44};
    if (c >= SDL_SCANCODE_A && c <= SDL_SCANCODE_Z)
        return letters[c - SDL_SCANCODE_A];
    if (c >= SDL_SCANCODE_1 && c <= SDL_SCANCODE_9)
        return c - SDL_SCANCODE_1 + 2;
    switch (c) {
    case SDL_SCANCODE_0:
        return 11;
    case SDL_SCANCODE_RETURN:
        return 28;
    case SDL_SCANCODE_AC_BACK:
    case SDL_SCANCODE_ESCAPE:
        return 1;
    case SDL_SCANCODE_SPACE:
        return 57;
    case SDL_SCANCODE_BACKSPACE:
        return 14;
    case SDL_SCANCODE_TAB:
        return 15;
    case SDL_SCANCODE_LSHIFT:
        return 42;
    case SDL_SCANCODE_RSHIFT:
        return 54;
    case SDL_SCANCODE_LCTRL:
        return 29;
    case SDL_SCANCODE_RCTRL:
        return 157;
    case SDL_SCANCODE_LALT:
        return 56;
    case SDL_SCANCODE_RALT:
        return 184;
    case SDL_SCANCODE_UP:
        return 200;
    case SDL_SCANCODE_DOWN:
        return 208;
    case SDL_SCANCODE_LEFT:
        return 203;
    case SDL_SCANCODE_RIGHT:
        return 205;
    case SDL_SCANCODE_DELETE:
        return 211;
    case SDL_SCANCODE_HOME:
        return 199;
    case SDL_SCANCODE_END:
        return 207;
    case SDL_SCANCODE_PAGEUP:
        return 201;
    case SDL_SCANCODE_PAGEDOWN:
        return 209;
    case SDL_SCANCODE_MINUS:
        return 12;
    case SDL_SCANCODE_EQUALS:
        return 13;
    default:
        return 0;
    }
}
} // namespace
void android::overlayInput(bool enabled) { overlayActive = enabled; }
void android::openControllerSettings() {
#ifdef __ANDROID__
    auto *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity)
        return;
    auto cls = env->GetObjectClass(activity);
    auto method = env->GetMethodID(cls, "openControllerSettings", "()V");
    env->CallVoidMethod(activity, method);
    if (env->ExceptionCheck())
        env->ExceptionClear();
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
#endif
}
void hookFreeCameraMouse(bool on) { mouseHook = on; }
FreeCameraMouse &freeCameraMouse() { return mouse; }
Language chooseLanguage(int32_t id, const std::string &name) {
    auto language = *Language::byIndex(languageOfLangId(id < 0 ? 0x409 : id));
    if (!name.empty())
        if (auto chosen = Language::byName(name))
            language = *chosen;
    return language;
}
void setStartupLanguage(const Language &l) { startupLanguage = l; }
std::wstring startupText(int) {
    return L"Juiced could not load the game data. Check the --game folder and the log.";
}
void errorBox(const std::wstring &text, const std::wstring &caption, bool) {
    MessageBoxW(nullptr, text.c_str(), caption.c_str(), 0);
}
bool runConfigurationTool() {
    log::info("Android settings are stored in the app data directory");
    return true;
}
Settings Settings::defaults(const LaunchOptions &options) {
    Settings s;
    s.width = 1280;
    s.height = 720;
    s.depth = 32;
    s.antialiasing = 0;
    s.shaders = 3;
    s.audioFlags = 4 | 32;
    readSettings(s);
    s.audioFlags |= options.audioFlagsOr;
    if (options.audioFlagsOne)
        s.audioFlags = 1;
    return s;
}
std::optional<std::array<uint8_t, 16>> readAdapterId() { return std::array<uint8_t, 16>{}; }
int32_t readLanguageSetting() { return -1; }
void readSettings(Settings &s) {
    const char *home = getenv("JUICED_SUPPORT_DIR");
    if (!home)
        return;
    auto dir = std::filesystem::path(home);
    std::filesystem::create_directories(dir);
    auto path = dir / "settings.ini";
    std::ifstream in(path);
    if (!in) {
        std::ofstream(path) << "width=1280\nheight=720\n";
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        try {
            int value = std::stoi(line.substr(eq + 1));
            auto key = line.substr(0, eq);
            if (key == "width")
                s.width = std::clamp(value, 640, 3840);
            if (key == "height")
                s.height = std::clamp(value, 480, 2160);
        } catch (...) {
        }
    }
}
bool Platform::start(const char *title, const LaunchOptions &options, Settings &settings) {
    events.clear();
    touch.clear();
    priorTouch = {};
    focused = true;
    screenshot = false;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        log::error("SDL: {}", SDL_GetError());
        return false;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#ifndef __ANDROID__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    auto flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;
#ifdef __ANDROID__
    flags |= SDL_WINDOW_FULLSCREEN;
#endif
    if (options.test)
        flags |= SDL_WINDOW_HIDDEN;
    m_window = reinterpret_cast<HWND>(SDL_CreateWindow(title, settings.width, settings.height, flags));
    if (!m_window) {
        log::error("window: {}", SDL_GetError());
        return false;
    }
    return true;
}
Platform::~Platform() {
    audio::system().stop();
    if (m_window)
        SDL_DestroyWindow(reinterpret_cast<SDL_Window *>(m_window));
    if (overlayPad) {
        SDL_CloseGamepad(overlayPad);
        overlayPad = nullptr;
    }
    SDL_Quit();
}
bool Platform::pumpMessages() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            m_quit = true;
            break;
        case SDL_EVENT_DID_ENTER_FOREGROUND:
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            focused = true;
            android::controllerConfig().load();
            audio::setActive(true);
            break;
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            focused = false;
            touch.clear();
            audio::setActive(false);
            for (int i = 1; i < 256; i++)
                events.push_back({(uint8_t)i, false});
            break;
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
            touch.update(e.tfinger.fingerID, e.tfinger.x, e.tfinger.y);
            break;
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
            touch.release(e.tfinger.fingerID);
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            if (e.key.repeat)
                break;
            if (e.key.scancode == SDL_SCANCODE_PRINTSCREEN && e.key.down)
                screenshot = true;
            auto key = scan(e.key.scancode);
            if (key) {
                events.push_back({key, e.key.down});
                if (e.key.down)
                    mods::neonKey(key);
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            if (mouseHook) {
                mouse.moveX = e.motion.xrel;
                mouse.moveY = e.motion.yrel;
                mouse.x = e.motion.x;
                mouse.y = e.motion.y;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (mouseHook) {
                if (e.button.button == SDL_BUTTON_LEFT)
                    mouse.forward = e.button.down;
                if (e.button.button == SDL_BUTTON_RIGHT)
                    mouse.back = e.button.down;
            }
            break;
        default:
            break;
        }
    }
    auto held = touch.held();
    constexpr uint8_t keys[] = {203, 205, 200, 208, 28, 1, 42, 57, 18, 16, 46, 19};
    constexpr uint8_t modern[] = {30, 32, 17, 31};
    for (size_t i = 0; i < held.size(); ++i)
        if (held[i] != priorTouch[i]) {
            events.push_back({keys[i], held[i]});
            if (i < 4)
                events.push_back({modern[i], held[i]});
        }
    priorTouch = held;
    return !m_quit;
}
bool Platform::active() const { return focused; }
bool Platform::closedFromSystemMenu() const { return m_quit; }
bool Platform::takeScreenshotRequest() { return std::exchange(screenshot, false); }
std::vector<input::KeyEvent> Platform::takeKeyEvents() {
    android::connectGamepad(overlayPad);
    std::array<bool, 6> held{};
    if (overlayActive && focused && overlayPad) {
        auto b = [&](SDL_GamepadButton button) {
            return SDL_GetGamepadButton(overlayPad, android::controllerConfig().button(button));
        };
        held = {
            b(SDL_GAMEPAD_BUTTON_DPAD_UP) || SDL_GetGamepadAxis(overlayPad, SDL_GAMEPAD_AXIS_LEFTY) < -16000,
            b(SDL_GAMEPAD_BUTTON_DPAD_DOWN) || SDL_GetGamepadAxis(overlayPad, SDL_GAMEPAD_AXIS_LEFTY) > 16000,
            b(SDL_GAMEPAD_BUTTON_DPAD_LEFT),
            b(SDL_GAMEPAD_BUTTON_DPAD_RIGHT),
            b(SDL_GAMEPAD_BUTTON_SOUTH),
            b(SDL_GAMEPAD_BUTTON_EAST)};
    }
    const uint8_t keys[] = {200, 208, 203, 205, 28, 1};
    for (size_t i = 0; i < held.size(); ++i)
        if (held[i] != overlayHeld[i])
            events.push_back({keys[i], held[i]});
    overlayHeld = held;
    return std::exchange(events, {});
}
namespace {
class SdlController final : public input::ControllerPort {
    SDL_Gamepad *pad = nullptr;
    input::MapFormat format;
    bool ui = true;
    void connect() { android::connectGamepad(pad); }

  public:
    ~SdlController() override {
        if (pad && SDL_WasInit(SDL_INIT_GAMEPAD))
            SDL_CloseGamepad(pad);
    }
    void unacquire() override {}
    void setActionMap(const input::MapFormat &f) override {
        format = f;
        ui = f.name.find("UI") != std::string::npos;
    }
    void setRawFormat() override { format = {}; }
    void recreate() override {
        if (pad)
            SDL_CloseGamepad(pad);
        pad = nullptr;
        connect();
    }
    input::ControllerRead read(bool mapped) override {
        connect();
        const auto &settings = android::controllerConfig();
        input::ControllerRead result;
        auto button = [&](SDL_GamepadButton b) {
            b = settings.button(b);
            return focused && pad && SDL_GetGamepadButton(pad, b) ? 128 : 0;
        };
        auto axis = [&](SDL_GamepadAxis a) {
            if (!pad || !focused)
                return 0;
            int v = SDL_GetGamepadAxis(pad, a);
            int percent = std::clamp(v * 100 / 32767, -100, 100);
            if (a == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || a == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
                return percent;
            if (std::abs(percent) <= settings.deadzone)
                return 0;
            return (percent < 0 ? -1 : 1) * (std::abs(percent) - settings.deadzone) * 100 /
                   (100 - settings.deadzone);
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
            switch (a.semantic) {
            case 0xFF00C201u:
                value = axis(SDL_GAMEPAD_AXIS_LEFTX);
                if (button(SDL_GAMEPAD_BUTTON_DPAD_LEFT))
                    value = -100;
                if (button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
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
    SdlController controller;
};
InputDevices::InputDevices() : m_state(std::make_unique<State>()) {}
InputDevices::~InputDevices() = default;
bool InputDevices::start(HWND) { return true; }
std::vector<input::ControllerPort *> InputDevices::controllers() const { return {&m_state->controller}; }
input::KeyboardPort *InputDevices::keyboard() const { return nullptr; }
bool InputDevices::configure(const input::MapFormat &) {
    android::openControllerSettings();
    return true;
}
void Platform::beginControlsWindow() {}
void Platform::endControlsWindow() {}
void InputDevices::startWheels(HWND) {}
void InputDevices::stopWheels() {}
} // namespace oj
