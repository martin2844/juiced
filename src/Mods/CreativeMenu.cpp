#include "Mods/CreativeMenu.h"
#include "Core/Base/Text.h"
#include "Game/UI/MessageWindow.h"
#include <algorithm>
namespace oj::mods {
namespace {
constexpr const char *effects[] = {"GLOW", "PULSATING", "BACK AND FORTH", "RGB CYCLE"};
}
CreativeMenu::CreativeMenu(render::IRenderDevice &d, const Language &l, Request r)
    : m_device(d), m_background(d, l), m_menu(d, l), m_message(d, l), m_request(std::move(r)), m_last(seconds()) {
    m_background.load("opts_bg");
    m_menu.load("opts_menu");
    m_message.load("msgbox");
    if (m_request.car)
        m_draft = neon(*m_request.car);
    fill();
}
void CreativeMenu::fill() {
    m_menu.clearMenu("Menu");
    m_menu.clearMenu("BackMenu");
    m_menu.setMenuActive("BackMenu", false);
    m_menu.setText("TITLE", m_request.car          ? L"CUSTOM NEON"
                            : m_request.onboarding ? L"YOUR DRIVER PHOTO"
                                                   : L"MOD GARAGE");
    auto add = [&](std::string label, std::string help) {
        auto i = m_menu.menuSize("Menu");
        m_menu.addMenuItem("Menu", toWide(label));
        m_menu.setMenuItemHelp("Menu", i, toWide(help));
        m_menu.addMenuItem("BackMenu", L"", 0, "", "ui_pan5_ora");
        m_menu.setMenuItemAdditive("BackMenu", i, true);
    };
    if (m_request.car) {
        for (int i = 0; i < 3; ++i)
            add(std::string(i == 0   ? "RED"
                            : i == 1 ? "GREEN"
                                     : "BLUE") +
                    " : " + std::to_string((m_draft.rgb >> (16 - i * 8)) & 255),
                "LEFT / RIGHT: adjust 0–255. ENTER: +10. Changes apply after purchase.");
        add(std::string("EFFECT : ") + effects[m_draft.effect], "LEFT / RIGHT or ENTER: choose the neon effect.");
        add("SPEED : " + std::to_string(m_draft.speed), "LEFT / RIGHT or ENTER: animation speed, 1 slow to 5 fast.");
        add("APPLY", "Continue to the game's normal neon purchase confirmation.");
        add("CANCEL", "Discard these changes and return to the garage.");
    } else {
        add(hasImage(true) ? "CHANGE DRIVER PHOTO" : "CHOOSE DRIVER PHOTO",
            "Choose a PNG or JPEG. Your portrait is cropped to a square.");
        if (!m_request.onboarding) {
            add(hasImage(false) ? "CHANGE CUSTOM LOGO" : "IMPORT CUSTOM LOGO",
                "Transparent PNG recommended. Choose the first crew logo; use CUSTOM LOGO under decals.");
            add("REMOVE DRIVER PHOTO", "Restore the original silhouette for this profile.");
            add("REMOVE CUSTOM LOGO", "Restore the original crew logo and decals.");
        }
        add(m_request.onboarding ? "CONTINUE" : "BACK",
            m_request.onboarding ? "Continue creating your driver. You can add a photo later in Options / Mod Garage."
                                 : "Return to the game. Images are saved separately for this profile.");
    }
    m_menu.selectMenuItem("Menu", m_focus);
    m_menu.selectMenuItem("BackMenu", m_focus);
    m_menu.refreshMenuHelp("Menu");
}
void CreativeMenu::notice(const std::string &text) {
    m_notice = true;
    m_message.restartTransitions();
    ui::openMessage(m_message, ui::MessageType::Note, toWide(text + "\n\nENTER: CONTINUE"));
}
bool CreativeMenu::update(const std::vector<input::KeyEvent> &keys) {
    for (auto k : keys) {
        if (!k.down)
            continue;
        if (m_notice) {
            if (k.code == 28 || k.code == 1)
                m_notice = false;
            continue;
        }
        if (k.code == 1)
            return false;
        if (k.code == 200 || k.code == 208) {
            m_menu.finishTransitions();
            m_menu.menuEvent(k.code == 200 ? input::kUp : input::kDown);
            m_focus = m_menu.loaded() ? int(m_menu.selectedMenuItem("Menu"))
                                      : (m_focus + (k.code == 200 ? -1 : 1) +
                                         (m_request.car          ? 7
                                          : m_request.onboarding ? 2
                                                                 : 5)) %
                                            (m_request.car          ? 7
                                             : m_request.onboarding ? 2
                                                                    : 5);
            m_menu.selectMenuItem("BackMenu", m_focus);
            continue;
        }
        if (m_request.car) {
            if (k.code != 203 && k.code != 205 && k.code != 28)
                continue;
            int delta = k.code == 203 ? -1 : 1;
            if (m_focus < 3) {
                int shift = 16 - m_focus * 8;
                int value = int((m_draft.rgb >> shift) & 255);
                value = std::clamp(value + delta * (k.code == 28 ? 10 : 1), 0, 255);
                m_draft.rgb = (m_draft.rgb & ~(255u << shift)) | (uint32_t(value) << shift);
            } else if (m_focus == 3)
                m_draft.effect = (m_draft.effect + delta + 4) % 4;
            else if (m_focus == 4)
                m_draft.speed = std::clamp(m_draft.speed + delta, 1, 5);
            else if (k.code == 28) {
                if (m_focus == 5 && m_request.apply)
                    m_request.apply(m_draft);
                return false;
            }
            fill();
        } else if (k.code == 28) {
            if (m_focus == 0 || (!m_request.onboarding && m_focus == 1)) {
                auto error = importImage(m_focus == 0);
                fill();
                notice(error.empty() ? "IMAGE SAVED FOR THIS DRIVER" : error);
            } else if (!m_request.onboarding && (m_focus == 2 || m_focus == 3)) {
                if (!clearImage(m_focus == 2))
                    notice("Could not save this profile’s image settings.");
                fill();
            } else
                return false;
        }
    }
    return true;
}
void CreativeMenu::draw() {
    double now = seconds();
    float dt = std::clamp(float(now - m_last), 0.f, .1f);
    m_last = now;
    m_device.setRenderTarget(0, 0);
    m_device.setRenderState(render::RenderState::AlphaTestEnable, 0);
    m_device.setClip(nullptr);
    m_background.update(dt);
    m_menu.update(dt);
    m_background.draw(float(now), dt);
    m_menu.draw(float(now), dt);
    if (m_notice) {
        m_message.update(dt);
        m_message.draw(float(now), dt);
    }
}
} // namespace oj::mods
