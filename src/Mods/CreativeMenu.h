#pragma once
#include "Core/Input/Input.h"
#include "Game/UI/UiScreen.h"
#include "Mods/Creative.h"
#include "Mods/Runtime.h"
namespace oj::mods {
class CreativeMenu {
  public:
    CreativeMenu(render::IRenderDevice &device, const Language &language, Request request);
    ~CreativeMenu() {
        if (m_request.finished)
            m_request.finished();
    }
    bool update(const std::vector<input::KeyEvent> &keys);
    void draw();

  private:
    render::IRenderDevice &m_device;
    ui::Screen m_background, m_menu, m_message;
    Request m_request;
    std::vector<runtime::MenuItem> m_items;
    int m_focus = 0;
    bool m_notice = false;
    double m_last = 0;
    void fill();
    void notice(const std::string &text);
};
} // namespace oj::mods
