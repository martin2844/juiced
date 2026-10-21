#pragma once
#include "Core/Input/Input.h"
#include "Game/UI/UiScreen.h"
#include "Mac/Controls.h"
#include <chrono>
namespace oj::mac {
// Binding editor backed by the retail options menu and message-box widgets.
class ControlsOverlay {
  public:
    enum class Result { Open, Saved, Cancelled };
    ControlsOverlay(render::IRenderDevice &device, const Language &language, std::filesystem::path path);
    Result update(const std::vector<input::KeyEvent> &keys, bool shortcuts);
    void draw();
    const Bindings &bindings() const { return m_draft; }
    int captureRow() const { return m_capture; }
    bool loaded() const { return m_menu.loaded() && m_background.loaded() && m_message.loaded(); }

  private:
    render::IRenderDevice &m_device;
    ui::Screen m_background, m_menu, m_message;
    std::filesystem::path m_path;
    Bindings m_draft;
    int m_group = -1, m_focus = 0, m_capture = -1;
    bool m_notice = false;
    float m_seconds = 0;
    std::chrono::steady_clock::time_point m_last = std::chrono::steady_clock::now();
    void fillMenu();
    void refreshBindings();
    void message(const std::string &text);
    Result activate();
};
} // namespace oj::mac
