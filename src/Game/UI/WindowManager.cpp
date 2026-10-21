#include "Game/UI/WindowManager.h"

#include <cstring>

namespace oj::ui {

WindowManager::WindowManager(render::IRenderDevice& device, const Language& language)
    : m_device(device), m_language(language) {}

void WindowManager::keep(const std::string& name) {
    m_kept.emplace_back(name, make(name));
}

std::shared_ptr<Screen> WindowManager::open(const std::string& name) {
    for (auto& [kept, screen] : m_kept) {
        if (_stricmp(kept.c_str(), name.c_str()) != 0) continue;
        screen->restartTransitions();
        return screen;
    }
    return make(name);
}

std::shared_ptr<Screen> WindowManager::make(const std::string& name) {
    auto screen = std::make_shared<Screen>(m_device, m_language);
    if (prepare) prepare(*screen);
    const std::string file = fileOf ? fileOf(name) : name;
    if (!file.empty()) screen->load(file);
    return screen;
}

}  // namespace oj::ui
