// Unit tests: PadPrompts (not stock): the keyboard's hint pictures drawn as the pad's buttons while a pad is in use.
#include "Framework/Test.h"
#include "Core/Formats/Ddt.h"
#include "Core/Input/PadPrompts.h"
#include <cstring>
#include <string>

namespace {
using oj::test::check;
}  // namespace

OJ_UNIT_TEST(PadPrompts, SwapWhilePadInUse) {
    oj::input::usedPad(false);
    check(oj::input::promptTexture("ui_selecta") == nullptr, "keyboard in use: the stock picture");
    oj::input::usedPad(true);
    const char* a = oj::input::promptTexture("ui_selecta");
    check(a != nullptr && std::strcmp(a, "oj_pad_a") == 0, "pad in use: Enter's hint as A");
    check(oj::input::promptTexture("ui_back") != nullptr, "pad in use: Esc's hint swapped");
    check(oj::input::promptTexture("ui_garage") == nullptr, "other pictures stay");
    oj::input::usedPad(false);
    check(oj::input::promptTexture("ui_selecta") == nullptr, "a key: the stock picture again");
}

OJ_UNIT_TEST(PadPrompts, PicturesInflate) {
    for (const char* name : {"oj_pad_a", "oj_pad_b", "oj_pad_x", "oj_pad_y"}) {
        const auto bytes = oj::input::promptBytes(name);
        oj::Ddt ddt;
        check(bytes && ddt.parse(*bytes) && ddt.format == oj::Ddt::kA8R8G8B8 && ddt.width == 100 && ddt.height == 100,
              std::string(name) + ": a 100x100 A8R8G8B8 picture");
    }
    check(!oj::input::promptBytes("ui_selecta"), "a stock name: none");
}
