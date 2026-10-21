#include "Game/Platform/Platform.h"
#include "Audio/SoundSystem.h"
#include "Core/Base/Log.h"
#include "Core/Base/Text.h"
#include "Game/Platform/CommandLine.h"
#include "Game/Platform/InputDevices.h"
#include <SDL3/SDL.h>
#include <fstream>
#include <windows.h>
namespace oj {
namespace {
std::vector<input::KeyEvent> events;
bool focused = true, screenshot = false;
FreeCameraMouse mouse;
bool mouseHook = false;
Language startupLanguage{};
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
void hookFreeCameraMouse(bool on) {
    mouseHook = on;
}
FreeCameraMouse &freeCameraMouse() {
    return mouse;
}
Language chooseLanguage(int32_t id, const std::string &name) {
    auto language = *Language::byIndex(languageOfLangId(id < 0 ? 0x409 : id));
    if (!name.empty())
        if (auto chosen = Language::byName(name))
            language = *chosen;
    return language;
}
void setStartupLanguage(const Language &l) {
    startupLanguage = l;
}
std::wstring startupText(int) {
    return L"Juiced could not load the game data. Check the --game folder and the log.";
}
void errorBox(const std::wstring &text, const std::wstring &caption) {
    MessageBoxW(nullptr, text.c_str(), caption.c_str(), 0);
}
bool runConfigurationTool() {
    log::info("macOS settings are stored in settings.ini in Application Support/Juiced");
    return true;
}
Settings Settings::defaults(const LaunchOptions &options) {
    Settings s;
    s.width = 1280;
    s.height = 960;
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
std::optional<std::array<uint8_t, 16>> readAdapterId() {
    return std::array<uint8_t, 16>{};
}
int32_t readLanguageSetting() {
    return -1;
}
void readSettings(Settings &s) {
    const char *home = getenv("HOME");
    if (!home)
        return;
    auto dir = std::filesystem::path(home) / "Library/Application Support/Juiced";
    std::filesystem::create_directories(dir);
    auto path = dir / "settings.ini";
    std::ifstream in(path);
    if (!in) {
        std::ofstream(path) << "width=1280\nheight=960\n";
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
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        log::error("SDL: {}", SDL_GetError());
        return false;
    }
    auto flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_METAL;
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
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            focused = true;
            audio::setActive(true);
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            focused = false;
            audio::setActive(false);
            for (int i = 1; i < 256; i++)
                events.push_back({(uint8_t)i, false});
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            if (e.key.repeat)
                break;
            if (e.key.scancode == SDL_SCANCODE_PRINTSCREEN && e.key.down)
                screenshot = true;
            auto key = scan(e.key.scancode);
            if (key)
                events.push_back({key, e.key.down});
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
    return !m_quit;
}
bool Platform::active() const {
    return focused;
}
bool Platform::closedFromSystemMenu() const {
    return m_quit;
}
bool Platform::takeScreenshotRequest() {
    return std::exchange(screenshot, false);
}
std::vector<input::KeyEvent> Platform::takeKeyEvents() {
    return std::exchange(events, {});
}
struct InputDevices::State {};
InputDevices::InputDevices() : m_state(std::make_unique<State>()) {}
InputDevices::~InputDevices() = default;
bool InputDevices::start(HWND) {
    return true;
}
std::vector<input::ControllerPort *> InputDevices::controllers() const {
    return {};
}
void InputDevices::startWheels(HWND) {}
void InputDevices::stopWheels() {}
} // namespace oj
