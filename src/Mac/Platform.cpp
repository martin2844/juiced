#ifdef OJ_LINUX
#include "Linux/Paths.h"
#endif
#include "Game/Platform/Platform.h"
#include "Audio/SoundSystem.h"
#include "Core/Base/Log.h"
#include "Core/Base/Text.h"
#include "Game/Platform/CommandLine.h"
#include "Game/Platform/InputDevices.h"
#include <SDL3/SDL.h>
#include "Mac/Controls.h"
#include <fstream>
#include <windows.h>
namespace oj {
namespace {
std::vector<input::KeyEvent> events;
bool focused = true, screenshot = false;
FreeCameraMouse mouse;
bool mouseHook = false;
Language startupLanguage{};
uint8_t scan(SDL_Scancode c) { return mac::gameScanCode(c); }

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
void errorBox(const std::wstring &text, const std::wstring &caption, bool) {
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
    #ifdef OJ_LINUX
    auto dir = oj::linux_port::configPath();
#else
    auto dir = std::filesystem::path(home) / "Library/Application Support/Juiced";
#endif
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
    #ifdef OJ_LINUX
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    auto flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;
#else
    auto flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_METAL;
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
input::KeyboardPort *InputDevices::keyboard() const { return nullptr; }
bool InputDevices::configure(const input::MapFormat&) { return false; }
// The Mac controls overlay stays in the game window; these DirectInput hooks are unused.
void Platform::beginControlsWindow() {}
void Platform::endControlsWindow() {}
void InputDevices::startWheels(HWND) {}
void InputDevices::stopWheels() {}
} // namespace oj
