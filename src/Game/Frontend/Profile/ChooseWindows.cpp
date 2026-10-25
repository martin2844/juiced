#if defined(OJ_MACOS) || defined(OJ_LINUX) || defined(OJ_ANDROID)
#include "Mods/Creative.h"
#endif
// The windows that choose a new driver's logo and phone.

#include "Game/Frontend/Profile/ChooseWindows.h"

#include "Core/Base/Log.h"
#include "Core/Data/GameTexts.h"
#include "Game/Frontend/Profile/Keyboard.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

namespace {

constexpr char kCrewCharacters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ";

}  // namespace

// 0x004C3B70: the first 19 crew logos of the table 0x00718A28 (ui_chooselogo_01 .. _18, then _20: logo 19 is never
// offered), value = their place in it. The fixed build's table has logo 19 in its place (ExeTables): all 20.
void ChooseLogo::enter(ProfileHost& host, ui::Screen& s) {
    s.clearMenu("Menu");
    const GameData* data = host.gameData();
    const std::vector<std::string>* logos = data != nullptr ? &data->tables.logos : nullptr;
    for (size_t i = 0; logos != nullptr && i < 20 && i < logos->size(); ++i) {
        const std::string& name = (*logos)[i];
        s.addMenuItem("Menu", L"", static_cast<float>(i), name, name);
    }
#if defined(OJ_MACOS) || defined(OJ_LINUX) || defined(OJ_ANDROID)
    if(mods::feature("logo"))s.addMenuItem("Menu",L"IMPORT YOUR LOGO",19,"ui_chooselogo_01","ui_chooselogo_01");
#endif
}

// The crew logo (0x004C3BD0): SELECT keeps the logo in the settings and goes on to the phone, BACK opens the name
// window again with the name entered.
void ChooseLogo::update(ProfileHost& host, ui::Screen& s, uint32_t event) {
    std::optional<save::Profile>& profile = host.profile();
    if (event == input::kSelect) {
#if defined(OJ_MACOS) || defined(OJ_LINUX) || defined(OJ_ANDROID)
        if(mods::feature("logo") && s.selectedMenuItem("Menu")==19){mods::requestMenu();s.selectMenuItem("Menu",0);return;}
#endif
        if (profile) {
            const size_t selected = s.selectedMenuItem("Menu");
            const float value = s.menuItemValue("Menu", selected);
            profile->settings.setU32(save::Settings::kLogo, static_cast<uint32_t>(x87::chop(value)));
        }
        host.sound("CLICK");
        host.request("cmsmobilechoose", SwitchMode::Replace);
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.setUpDriverName(profile ? profile->name : host.gameTexts().at(kTextPlayer));
        host.request("keyboard", SwitchMode::Replace);
    }
}

// The phone (0x004C3DD0): MOBILENAMES (the names) and PHONEMENU (the pictures) move together. For a new career
// SELECT keeps the phone (+0x08 of the career object +0x5C) and opens the crew name window; BACK returns to the
// logo.
void ChoosePhone::update(ProfileHost& host, ui::Screen& s, uint32_t event) {
    if (event == input::kSelect) {
        const TextTable& texts = host.gameTexts();
        m_phone = static_cast<int>(x87::chop(s.menuItemValue("MOBILENAMES", s.selectedMenuItem("MOBILENAMES"))));
        if (GameData* data = host.gameData(); data != nullptr) {
            data->career->phone.model = m_phone;
        }
        KeyboardSetup& k = host.keyboardSetup();
        k.title = texts.at(kTextNewCrewName);
        k.text = texts.at(kTextCrew);
        k.minLength = 1;
        k.maxLength = 15;
        k.small = k.maxLength > 26;
        k.checkName = true;
        ProfileHost* owner = &host;
        k.accept = [owner](const std::wstring& name) {
            // 0x004675A0: the crew name goes into the settings and the crew's text lines (0x00409F30).
            if (owner->profile()) {
                owner->profile()->settings.setCrewName(name);
                owner->setCrewNameTexts(name);
            }
            log::info("frontend: crew {}", toUtf8(name));
            return true;
        };
        k.allowed = kCrewCharacters;
        k.back = "cmsmobilechoose";
        k.next = "saveprofile";
        host.sound("CLICK");
        host.request("keyboard", SwitchMode::Replace);
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("cmschooselogo", SwitchMode::Replace);
    }
}

}  // namespace oj::frontend
