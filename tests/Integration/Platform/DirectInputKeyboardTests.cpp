// Integration tests: DirectInputKeyboard.
// The keyboard's maps through DirectInput (InputDevices' keyboard, 0x005954D0's BuildActionMap on GUID_SysKeyboard),
// as the stock maps its keyboard, against each DIKEYBOARD_ action on its key (the maps without DirectInput). The maps
// take GUIDs of their own here, so no map of the user's kept by DirectInput for the game's (the controls' mapping
// window's) comes in: DirectInput's default map gives each key the controls file's action; it leaves out the keys the
// keyboard has not (UI's '@' on DIK_AT, 0x91, a key of the Japanese keyboards). A user's map changes what the user
// changed.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Base/GameFiles.h"
#include "Core/Data/Controls.h"
#include "Core/Input/Input.h"
#include "Game/Platform/InputDevices.h"

#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace {

using oj::test::check;

// The keys of the player's map (the UI map, or DRIVER1): each key's action, -1 for none.
std::vector<int64_t> keysOf(const std::vector<oj::ActionMap>& maps, oj::input::KeyboardPort* keyboard, bool driver) {
    oj::input::Input in;
    in.load(maps);
    in.setControllers({});
    in.setKeyboard(keyboard);
    in.setPlayerDevice(0, 0);
    if (driver) {
        in.useDriver();
    } else {
        in.useUi();
    }
    std::vector<int64_t> keys;
    for (int k = 0; k < 256; ++k) keys.push_back(in.keyAction(static_cast<uint8_t>(k)));
    return keys;
}

}  // namespace

OJ_INTEGRATION_TEST(DirectInputKeyboard, DefaultMapIsTheFiles) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const auto bytes = oj::GameFiles(std::filesystem::current_path()).read(std::filesystem::path("scripts") /
                                                                           "controls_dx9_e.txt");
    if (!bytes) {
        check(false, "scripts\\controls_dx9_e.txt read");
        return;
    }
    auto maps = oj::parseControls(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
    for (size_t i = 0; i < maps.size(); ++i) {
        maps[i].guid = std::format("0x0A5E5E{:02X}-0x0123-0x4567-0x89-0xAB-0xCD-0xEF-0x01-0x23-0x45-0x67", i);
    }
    oj::InputDevices devices;
    if (!devices.start(nullptr) || devices.keyboard() == nullptr) {
        check(false, "DirectInput and its keyboard");
        return;
    }
    for (const bool driver : {false, true}) {
        const std::vector<int64_t> direct = keysOf(maps, devices.keyboard(), driver);
        const std::vector<int64_t> files = keysOf(maps, nullptr, driver);
        size_t mapped = 0;
        size_t moved = 0;
        std::string left;
        for (size_t k = 0; k < direct.size(); ++k) {
            if (direct[k] == -1) {
                if (files[k] != -1) left += std::format(" 0x{:02X}", k);
                continue;
            }
            ++mapped;
            if (direct[k] != files[k]) ++moved;
        }
        check(mapped != 0 && moved == 0, std::string(driver ? "DRIVER1" : "UI") + ": DirectInput's default map gives " +
                                             std::to_string(mapped) + " keys, each the controls file's (left out:" +
                                             (left.empty() ? std::string(" none") : left) + ")");
    }
}
