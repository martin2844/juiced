#pragma once
// A window manager's windows (docs/spec/02-frontend.md, "The windows a manager keeps"). The stock manager
// (0x005A5F90) opens a window by its name (0x005A6DB0, 0x005A7040): one of the windows it keeps (its +0x10 .. +0x14
// list, made by 0x005A8430), its transitions started again (0x005AA0A0) and the rest as its last visit left it, or
// else one made from its file (0x005A9490). A window closed (0x005A6D50) is freed unless the list holds it: here the
// last holder of a window not kept lets it go.

#include "Core/Data/Language.h"
#include "Game/UI/UiScreen.h"
#include "Render/RenderDevice.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace oj::ui {

class WindowManager {
public:
    WindowManager(render::IRenderDevice& device, const Language& language);

    // What a window is given before its file is read (its sounds, its 3d elements' views).
    std::function<void(Screen&)> prepare;
    // The file of a window's name: the name itself unless set; empty for a window of no file (no elements).
    std::function<std::string(const std::string&)> fileOf;

    // 0x005A8430: a window made and kept.
    void keep(const std::string& name);
    // The kept window of the name (the first, case aside) with its transitions started again, else a new one (a
    // file that does not load gives a window without elements: Screen::loaded false).
    std::shared_ptr<Screen> open(const std::string& name);
    size_t keptCount() const { return m_kept.size(); }

    render::IRenderDevice& device() const { return m_device; }
    const Language& language() const { return m_language; }

private:
    std::shared_ptr<Screen> make(const std::string& name);

    render::IRenderDevice& m_device;
    const Language& m_language;
    std::vector<std::pair<std::string, std::shared_ptr<Screen>>> m_kept;
};

}  // namespace oj::ui
