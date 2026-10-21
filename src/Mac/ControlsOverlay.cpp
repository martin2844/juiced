#include "Mac/ControlsOverlay.h"
#include "Core/Base/Text.h"
#include <algorithm>
#include <cctype>
namespace oj::mac {
ControlsOverlay::ControlsOverlay(render::IRenderDevice &device, const Language &language, std::filesystem::path path)
    : m_device(device), m_text(device, language), m_background(device, language), m_path(std::move(path)), m_draft(loadBindings(m_path)) {
    m_background.load("opts_bg");
    m_background.finishTransitions();
}
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
// Slanted strips follow the original game's header, selection and footer graphics.
void ControlsOverlay::ribbon(float x, float y, float w, float h, uint32_t colour) {
    const float skew = 22;
    const auto vertex = [&](float px, float py) {
        return render::QuadVertex{left() + px * scale(), top() + py * scale(), 0, 0, colour};
    };
    const render::QuadVertex q[4] = {vertex(x + skew, y), vertex(x + w, y),
                                   vertex(x, y + h), vertex(x + w - skew, y + h)};
    m_device.drawQuad(q, 0, render::Blend::Alpha);
}
void ControlsOverlay::text(const std::string &value, float x, float y, float size, uint32_t colour, float width) {
    m_text.drawOverlayText(toWide(value), left() + (x + 1.5f) * scale(), top() + (y + 2) * scale(), size * scale(),
                           0xFF000000, width * scale());
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
    m_device.setRenderState(render::RenderState::AlphaTestEnable, 0);
    m_device.setClip(nullptr);
    box(0, 0, 1000, 700, 0xFF080C12);
    if (m_background.loaded()) m_background.draw(0);
    // Keep the game's artwork around a dark, legible menu surface.
    box(0, 32, 1000, 630, 0xE6081017);
    ribbon(-24, 44, 1048, 68, 0xFF1600FF);
    ribbon(-24, 108, 650, 5, 0xFF32E800);
    text("CONTROLS", 56, 62, 34, 0xFFFFFFFF);
    text("KEYBOARD", 780, 78, 16, 0xFFFFFFFF, 164);
    const char *groups[] = {"DRIVING", "GEARS / CAMERA", "CREW COMMANDS"};
    for (int tab = 0; tab < 3; ++tab) {
        if (tab == m_tab) ribbon(56 + tab * 300, 144, 288, 44, 0xE51600FF);
        text(groups[tab], 74 + tab * 300, 158, 16, tab == m_tab ? 0xFF00DFFF : 0xFFB8BEC5, 258);
    }
    box(56, 195, 888, 2, 0xFF38E500);
    for (int row = 0; row < 7; ++row) {
        const size_t index = m_tab * 7 + row;
        const float y = 208 + row * 44;
        const bool capture = m_capture == static_cast<int>(index);
        const bool selected = row == m_focus;
        if (selected) ribbon(56, y, 888, 40, capture ? 0xB52BA400 : 0xB51600FF);
        else box(76, y + 39, 848, 1, 0x283E8BA3);
        if (selected) text(">", 69, y + 11, 18, 0xFFFFFFFF, 22);
        std::string label = bindingActions()[index].label;
        std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) { return std::toupper(c); });
        text(label, 100, y + 12, 17, selected ? 0xFF00DFFF : 0xFFFFFFFF, 550);
        text(capture ? "PRESS A KEY..." : bindingKey(m_draft[index])->label, 714, y + 12, 17,
             selected ? 0xFF00DFFF : 0xFFFFFFFF, 204);
    }
    text(m_status, 76, 537, 13, 0xFFFFFFFF, 848);
    text("LEFT / RIGHT: CATEGORY    UP / DOWN: ACTION    ENTER: CHANGE", 76, 570, 12, 0xFF00DFFF, 848);
    const float x[] = {56, 614, 784};
    const float widths[] = {264, 150, 160};
    const char *captions[] = {"DEFAULTS", "CANCEL", "SAVE"};
    ribbon(-24, 598, 1048, 50, 0xFF1600FF);
    for (int i = 0; i < 3; ++i) {
        const bool selected = m_focus == i + 7;
        if (selected) ribbon(x[i], 600, widths[i], 44, 0xFF208C00);
        text(captions[i], x[i] + 22, 614, 18, selected ? 0xFF00DFFF : 0xFFFFFFFF, widths[i] - 30);
    }
    text("ESC: BACK WITHOUT SAVING", 56, 669, 12, 0xFFFFFFFF, 400);
    text("ENTER: SELECT", 744, 669, 12, 0xFFFFFFFF, 200);
}
} // namespace oj::mac
