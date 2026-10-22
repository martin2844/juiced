#include "Mods/CreativeMenu.h"
#include "Core/Base/Text.h"
#include "Game/UI/MessageWindow.h"
#include "Mods/Packages.h"
#include <algorithm>
namespace oj::mods {
CreativeMenu::CreativeMenu(render::IRenderDevice &d, const Language &l, Request r)
    : m_device(d), m_background(d, l), m_menu(d, l), m_message(d, l), m_request(std::move(r)), m_last(seconds()) {
    m_background.load("opts_bg");
    m_menu.load("opts_menu");
    m_message.load("msgbox");
    fill();
}
void CreativeMenu::fill() {
    m_menu.clearMenu("Menu");
    m_menu.clearMenu("BackMenu");
    m_menu.setMenuActive("BackMenu", false);
    const bool pending = std::any_of(packages::list().begin(), packages::list().end(),
                                     [](const auto &p) { return p.active != p.enabled; });
    m_menu.setText("TITLE", m_request.management   ? (pending ? L"MODS - RESTART REQUIRED" : L"MODS")
                            : m_request.onboarding ? L"YOUR DRIVER PHOTO"
                                                   : L"MOD GARAGE");
    auto add = [&](std::string label, std::string help) {
        auto i = m_menu.menuSize("Menu");
        m_menu.addMenuItem("Menu", toWide(label));
        m_menu.setMenuItemHelp("Menu", i, toWide(help));
        m_menu.addMenuItem("BackMenu", L"", 0, "", "ui_pan5_ora");
        m_menu.setMenuItemAdditive("BackMenu", i, true);
    };
    m_items.clear();
    if (m_request.management) {
        for (const auto &p : packages::list()) {
            const bool pending = p.enabled != p.active;
            m_items.push_back({p.name + (p.enabled ? " : ON" : " : OFF") + (pending ? " *" : ""),
                               pending ? "Restart the game to apply this change."
                                       : "Enter: toggle. Changes apply after restarting the game.",
                               p.id});
        }
    } else {
        m_items = runtime::imageMenu(hasImage(true), hasImage(false), m_request.onboarding);
    }
    for (const auto &item : m_items)
        add(item.label, item.help);
    add(m_request.onboarding ? "CONTINUE" : "BACK", "Return to the game.");
    m_focus = std::clamp(m_focus, 0, int(m_items.size()));
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
            m_focus = m_menu.loaded()
                          ? int(m_menu.selectedMenuItem("Menu"))
                          : (m_focus + (k.code == 200 ? -1 : 1) + int(m_items.size()) + 1) % (int(m_items.size()) + 1);
            m_menu.selectMenuItem("BackMenu", m_focus);
            continue;
        }
        if (k.code == 28) {
            if (m_focus >= int(m_items.size()))
                return false;
            const auto action = m_items[m_focus].action;
            if (m_request.management) {
                const auto error = packages::toggle(action);
                fill();
                if (!error.empty())
                    notice(error);
            } else if (action == "import_portrait" || action == "import_logo") {
                auto error = importImage(action == "import_portrait");
                fill();
                notice(error.empty() ? "IMAGE SAVED FOR THIS DRIVER" : error);
            } else if (action == "clear_portrait" || action == "clear_logo") {
                if (!clearImage(action == "clear_portrait"))
                    notice("Could not save image settings.");
                fill();
            }
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
