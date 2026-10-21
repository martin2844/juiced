#include "Mac/ControlsOverlay.h"
#include "Core/Base/Text.h"
#include "Game/UI/MessageWindow.h"
#include <algorithm>

namespace oj::mac {
namespace {
const char *groups[] = {"DRIVING", "GEARS AND CAMERA", "CREW COMMANDS"};
}
ControlsOverlay::ControlsOverlay(render::IRenderDevice &device, const Language &language, std::filesystem::path path)
    : m_device(device), m_background(device, language), m_menu(device, language), m_message(device, language),
      m_path(std::move(path)), m_draft(loadBindings(m_path)) {
    m_background.load("opts_bg");
    m_menu.load("opts_menu");
    m_message.load("msgbox");
    fillMenu();
}

void ControlsOverlay::fillMenu() {
    m_menu.clearMenu("Menu");
    m_menu.clearMenu("BackMenu");
    m_menu.setMenuActive("BackMenu", false);
    m_menu.setText("TITLE", m_group < 0 ? L"CONTROLS" : L"CONTROLS / " + toWide(groups[m_group]));
    const auto add = [&](const std::string &label, const std::string &help) {
        const size_t index = m_menu.menuSize("Menu");
        m_menu.addMenuItem("Menu", toWide(label));
        m_menu.setMenuItemHelp("Menu", index, toWide(help));
        // Exactly the options controller's highlight texture and additive blend.
        m_menu.addMenuItem("BackMenu", L"", 0, "", "ui_pan5_ora");
        m_menu.setMenuItemAdditive("BackMenu", index, true);
    };
    if (m_group < 0) {
        for (const char *group : groups) add(group, "Choose the controls to change.");
        add("RESTORE DEFAULTS", "Restore WASD, Shift for nitrous and Space for handbrake. Save to apply.");
        add("CANCEL", "Return without saving changes.");
        add("SAVE AND RETURN", "Save your keyboard controls and return to Options.");
    } else {
        for (int row = 0; row < 7; ++row) {
            const auto index = static_cast<size_t>(m_group * 7 + row);
            add(std::string(bindingActions()[index].label) + " : " + bindingKey(m_draft[index])->label,
                "Press ENTER to change this key. Keys already in use swap assignments.");
        }
        add("BACK", "Return to Controls. Changes stay pending until you save.");
    }
    m_menu.selectMenuItem("Menu", m_focus);
    m_menu.selectMenuItem("BackMenu", m_focus);
    m_menu.refreshMenuHelp("Menu");
}

void ControlsOverlay::refreshBindings() {
    for (int row = 0; row < 7; ++row) {
        const auto index = static_cast<size_t>(m_group * 7 + row);
        m_menu.setMenuItemText("Menu", row,
            toWide(std::string(bindingActions()[index].label) + " : " + bindingKey(m_draft[index])->label));
    }
}

void ControlsOverlay::message(const std::string &text) {
    m_message.restartTransitions();
    ui::openMessage(m_message, ui::MessageType::Note, toWide(text));
}

ControlsOverlay::Result ControlsOverlay::activate() {
    if (m_menu.sound) m_menu.sound("CLICK");
    if (m_group >= 0) {
        if (m_focus == 7) {
            m_focus = m_group;
            m_group = -1;
            fillMenu();
        } else {
            m_capture = m_group * 7 + m_focus;
            message(std::string(bindingActions()[m_capture].label) +
                    "\nPRESS A KEY\n\nESC: CANCEL\nAn assigned key swaps with the previous binding.");
        }
    } else if (m_focus < 3) {
        m_group = m_focus;
        m_focus = 0;
        fillMenu();
    } else if (m_focus == 3) {
        m_draft = defaultBindings();
        message("DEFAULT CONTROLS RESTORED\n\nChoose SAVE AND RETURN to apply.\nENTER: CONTINUE");
        m_notice = true;
    } else if (m_focus == 4) {
        return Result::Cancelled;
    } else {
        std::string error;
        if (saveBindings(m_path, m_draft, error)) return Result::Saved;
        message(error + "\n\nENTER: CONTINUE");
        m_notice = true;
    }
    return Result::Open;
}

ControlsOverlay::Result ControlsOverlay::update(const std::vector<input::KeyEvent> &keys, bool shortcuts) {
    for (const auto &event : keys) {
        if (!event.down) continue;
        if (m_capture >= 0) {
            if (event.code == 1) {
                m_capture = -1;
                break;
            }
            if (shortcuts) {
                message("Command and Option shortcuts are reserved.\nChoose another key.\n\nESC: CANCEL");
                break;
            }
            const auto found = std::find_if(bindableKeys().begin(), bindableKeys().end(),
                [&](const auto &key) { return keyboardCode(key.semantic) == event.code; });
            if (found == bindableKeys().end()) {
                message("That key is reserved.\nChoose a letter, number, arrow, Shift or Control.\n\nESC: CANCEL");
                break;
            }
            rebind(m_draft, m_capture, found->semantic);
            m_capture = -1;
            refreshBindings();
            break;
        }
        if (shortcuts) continue;
        if (m_notice) {
            if (event.code == 1 || event.code == 28) m_notice = false;
            break;
        }
        if (event.code == 1) {
            if (m_group < 0) return Result::Cancelled;
            m_focus = m_group;
            m_group = -1;
            fillMenu();
            break;
        }
        if (event.code == 200 || event.code == 208 || event.code == 15) {
            const bool up = event.code == 200;
            if (m_menu.loaded()) {
                m_menu.finishTransitions();
                m_menu.menuEvent(up ? input::kUp : input::kDown);
                m_focus = static_cast<int>(m_menu.selectedMenuItem("Menu"));
                m_menu.selectMenuItem("BackMenu", m_focus);
            } else {
                // Allow binding-state tests to run without copyrighted game assets.
                const int count = m_group < 0 ? 6 : 8;
                m_focus = (m_focus + (up ? count - 1 : 1)) % count;
            }
        }
        if (event.code == 28) return activate();
    }
    return Result::Open;
}

void ControlsOverlay::draw() {
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::clamp(std::chrono::duration<float>(now - m_last).count(), 0.f, .1f);
    m_last = now;
    m_seconds += dt;
    m_device.setRenderTarget(0, 0);
    m_device.setRenderState(render::RenderState::AlphaTestEnable, 0);
    m_device.setClip(nullptr);
    m_background.update(dt);
    m_menu.update(dt);
    m_background.draw(m_seconds, dt);
    m_menu.draw(m_seconds, dt);
    if (m_capture >= 0 || m_notice) {
        m_message.update(dt);
        m_message.draw(m_seconds, dt);
    }
}
} // namespace oj::mac
