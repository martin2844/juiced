#include "Mac/ControlsOverlay.h"
#include "Core/Base/Text.h"
#include <algorithm>
namespace oj::mac {
ControlsOverlay::ControlsOverlay(render::IRenderDevice &device, const Language &language, std::filesystem::path path)
    : m_device(device), m_text(device, language), m_path(std::move(path)), m_draft(loadBindings(m_path)) {}
float ControlsOverlay::scale() const {
    return std::min(m_device.width() / 1000.f, m_device.height() / 700.f);
}
float ControlsOverlay::left() const {
    return (m_device.width() - 1000 * scale()) * .5f;
}
float ControlsOverlay::top() const {
    return (m_device.height() - 700 * scale()) * .5f;
}
void ControlsOverlay::box(float x, float y, float w, float h, uint32_t colour) {
    x = left() + x * scale();
    y = top() + y * scale();
    w *= scale();
    h *= scale();
    const render::QuadVertex q[4] = {
        {x, y, 0, 0, colour}, {x + w, y, 1, 0, colour}, {x, y + h, 0, 1, colour}, {x + w, y + h, 1, 1, colour}};
    m_device.drawQuad(q, 0, render::Blend::Alpha);
}
void ControlsOverlay::text(const std::string &value, float x, float y, float size, uint32_t colour, float width) {
    m_text.drawOverlayText(toWide(value), left() + x * scale(), top() + y * scale(), size * scale(), colour,
                           width * scale());
}
ControlsOverlay::Result ControlsOverlay::activate(int item) {
    if (item < 7) {
        m_capture = m_tab * 7 + item;
        m_status = "Press a key. Escape cancels capture. A key already in use swaps assignments.";
    } else if (item == 7) {
        m_draft = defaultBindings();
        m_status = "Modern defaults restored. Save to apply, or Cancel to keep your previous bindings.";
    } else if (item == 8) {
        return Result::Cancelled;
    } else {
        std::string error;
        if (saveBindings(m_path, m_draft, error))
            return Result::Saved;
        m_status = error;
    }
    return Result::Open;
}
ControlsOverlay::Result ControlsOverlay::update(const std::vector<input::KeyEvent> &keys, float x, float y, bool down,
                                                bool shortcuts) {
    x = (x - left()) / scale();
    y = (y - top()) / scale();
    const bool clicked = down && !m_down;
    m_down = down;
    for (const auto &event : keys) {
        if (!event.down)
            continue;
        if (m_capture >= 0) {
            if (event.code == 1) {
                m_capture = -1;
                m_status = "Capture cancelled. Your binding is unchanged.";
                continue;
            }
            if (shortcuts) {
                m_status = "Command and Option shortcuts are reserved. Choose another key.";
                continue;
            }
            const auto found = std::find_if(bindableKeys().begin(), bindableKeys().end(),
                                            [&](const auto &k) { return keyboardCode(k.semantic) == event.code; });
            if (found == bindableKeys().end()) {
                m_status = "That key is reserved. Choose a letter, number, arrow, Shift or Control.";
                continue;
            }
            m_status = rebind(m_draft, m_capture, found->semantic);
            m_capture = -1;
            // Do not let a second simultaneous key navigate or close the editor.
            break;
        }
        if (shortcuts)
            continue;
        if (event.code == 1)
            return Result::Cancelled;
        if (event.code == 200)
            m_focus = (m_focus + 9) % 10;
        if (event.code == 208 || event.code == 15)
            m_focus = (m_focus + 1) % 10;
        if (event.code == 203) {
            m_tab = (m_tab + 2) % 3;
            m_focus = 0;
        }
        if (event.code == 205) {
            m_tab = (m_tab + 1) % 3;
            m_focus = 0;
        }
        if (event.code == 28)
            return activate(m_focus);
    }
    if (clicked) {
        if (y >= 144 && y <= 188 && x >= 56 && x < 944) {
            m_tab = std::clamp(static_cast<int>((x - 56) / 300), 0, 2);
            m_focus = 0;
            m_capture = -1;
        } else if (y >= 208 && y < 516 && x >= 56 && x < 944) {
            m_focus = std::clamp(static_cast<int>((y - 208) / 44), 0, 6);
            return activate(m_focus);
        } else if (y >= 600 && y <= 644) {
            if (x >= 56 && x <= 320) {
                m_focus = 7;
                m_capture = -1;
                return activate(7);
            }
            if (x >= 614 && x <= 764) {
                m_focus = 8;
                return activate(8);
            }
            if (x >= 784 && x <= 944 && m_capture < 0) {
                m_focus = 9;
                return activate(9);
            }
        }
    }
    return Result::Open;
}
void ControlsOverlay::draw() {
    m_device.setRenderTarget(0, 0);
    m_device.setRenderState(render::RenderState::AlphaTestEnable, 0); // UI is alpha-blended, never alpha-tested.
    m_device.setClip(nullptr);
    box(0, 0, 1000, 700, 0xFF11151E);
    box(56, 40, 48, 4, 0xFFFFAE35);
    text("CONTROLS", 56, 61, 34, 0xFFF5F6F8);
    text("YOUR KEYS. YOUR RACE.", 56, 110, 14, 0xFF9AA9BA);
    text("KEYBOARD", 800, 78, 13, 0xFFFFAE35, 144);
    const char *groups[] = {"DRIVING", "GEARS & CAMERA", "CREW COMMANDS"};
    for (int tab = 0; tab < 3; ++tab) {
        box(56 + tab * 300, 144, 288, 44, tab == m_tab ? 0xFF303A49 : 0xFF1D2430);
        if (tab == m_tab)
            box(56 + tab * 300, 184, 288, 4, 0xFFFFAE35);
        text(groups[tab], 72 + tab * 300, 159, 15, tab == m_tab ? 0xFFFFC66D : 0xFF9AA9BA, 260);
    }
    for (int row = 0; row < 7; ++row) {
        const size_t index = m_tab * 7 + row;
        const float y = 208 + row * 44;
        const bool capture = m_capture == static_cast<int>(index);
        box(56, y, 888, 40, row == m_focus ? 0xFF283344 : 0xFF19212D);
        if (row == m_focus)
            box(56, y, 3, 40, 0xFFFFAE35);
        text(bindingActions()[index].label, 76, y + 12, 16, 0xFFF1F4F8, 570);
        box(698, y + 5, 230, 30, capture ? 0xFFFFAE35 : 0xFF354256);
        text(capture ? "PRESS A KEY..." : bindingKey(m_draft[index])->label, 714, y + 12, 15,
             capture ? 0xFF11151E : 0xFFF5F6F8, 204);
    }
    text(m_status, 56, 540, 12, 0xFFFFC66D, 888);
    text("CLICK A BINDING OR USE ARROWS + ENTER. USED KEYS SWAP.", 56, 567, 12, 0xFF9AA9BA, 888);
    const float x[] = {56, 614, 784};
    const float widths[] = {264, 150, 160};
    const char *captions[] = {"RESTORE DEFAULTS", "CANCEL", "SAVE"};
    for (int i = 0; i < 3; ++i) {
        const bool selected = m_focus == i + 7;
        box(x[i], 600, widths[i], 44, i == 2 && m_capture < 0 ? 0xFFFFAE35 : selected ? 0xFF45556A : 0xFF293444);
        if (selected)
            box(x[i], 644, widths[i], 2, 0xFFFFAE35);
        text(captions[i], x[i] + 16, 615, 14, i == 2 && m_capture < 0 ? 0xFF11151E : 0xFFF5F6F8, widths[i] - 25);
    }
    text("CHANGES APPLY WHEN YOU SAVE. THE GAME IS PAUSED WHILE THIS MENU IS OPEN.", 56, 669, 11, 0xFF9AA9BA, 888);
}
} // namespace oj::mac
