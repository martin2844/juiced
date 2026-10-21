#pragma once
#include "Core/Input/Input.h"
#include "Game/UI/UiScreen.h"
#include "Mac/Controls.h"
namespace oj::mac {
class ControlsOverlay {
  public:
    enum class Result { Open, Saved, Cancelled };
    ControlsOverlay(render::IRenderDevice &device, const Language &language, std::filesystem::path path);
    // Pointer coordinates are back-buffer pixels, just like the renderer.
    Result update(const std::vector<input::KeyEvent> &keys, float x, float y, bool down, bool shortcuts);
    void draw();
    const Bindings &bindings() const {
        return m_draft;
    }
    int captureRow() const {
        return m_capture;
    }

  private:
    render::IRenderDevice &m_device;
    ui::Screen m_text;
    ui::Screen m_background;
    std::filesystem::path m_path;
    Bindings m_draft;
    int m_tab = 0, m_focus = 0, m_capture = -1;
    bool m_down = true;
    std::string m_status = "Choose an action and press ENTER to change its key.";
    float scale() const;
    float left() const;
    float top() const;
    void box(float x, float y, float w, float h, uint32_t colour);
    void ribbon(float x, float y, float w, float h, uint32_t colour);
    void text(const std::string &value, float x, float y, float size, uint32_t colour, float width = 900);
    Result activate(int item);
};
} // namespace oj::mac
