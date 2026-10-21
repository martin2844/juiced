#include "Mac/ControlsOverlay.h"
#include "../Framework/Fakes/NullRenderDevice.h"
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace oj;
using namespace oj::mac;
using Result = ControlsOverlay::Result;
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
int main() {
    const auto folder = std::filesystem::temp_directory_path() / ("juiced-overlay-" + std::to_string(getpid()));
    const auto file = folder / "controls.ini";
    try {
        test::NullRenderDevice device;
        const auto language = *Language::byIndex(0);
        ControlsOverlay overlay(device, language, file);
        auto key = [&](uint8_t code, bool shortcut = false) {
            return overlay.update({{code, true}}, 0, 0, false, shortcut);
        };
        auto click = [&](float x, float y) {
            overlay.update({}, 0, 0, false, false);
            return overlay.update({}, x * .64f, 16 + y * .64f, true, false);
        };
        key(28);
        require(overlay.captureRow() == 0, "Enter starts selected binding capture");
        key(1);
        require(overlay.captureRow() == -1, "Escape cancels capture without closing menu");
        key(28);
        key(31, true);
        require(overlay.captureRow() == 0, "Command shortcut cannot become a binding");
        key(28);
        require(overlay.captureRow() == 0, "Enter remains reserved");
        key(42);
        require(overlay.bindings()[0] == "DIKEYBOARD_LSHIFT" && overlay.bindings()[4] == "DIKEYBOARD_W",
                "Shift capture swaps conflict");
        click(100, 620);
        require(overlay.bindings() == defaultBindings(), "Restore defaults stays in editor");
        click(400, 160);
        click(750, 222);
        key(20);
        require(overlay.bindings()[7] == "DIKEYBOARD_T", "Mouse selects second tab and first row");
        click(700, 160);
        click(750, 222);
        key(33);
        require(overlay.bindings()[14] == "DIKEYBOARD_F", "Third tab edits crew commands");
        require(click(850, 620) == Result::Saved, "Save closes editor successfully");
        const auto saved = loadBindings(file);
        require(saved == overlay.bindings(), "Saved values survive reopening");
        ControlsOverlay cancel(device, language, file);
        cancel.update({}, 0, 0, false, false);
        cancel.update({}, 100 * .64f, 16 + 620 * .64f, true, false);
        require(cancel.bindings() == defaultBindings(), "Reset edits draft only");
        require(cancel.update({{1, true}}, 0, 0, false, false) == Result::Cancelled && loadBindings(file) == saved,
                "Cancel discards draft and preserves saved values");
        std::filesystem::remove_all(folder);
        std::cout << "In-game keyboard, mouse, capture, conflict, save and cancel checks passed\n";
    } catch (const std::exception &e) {
        std::filesystem::remove_all(folder);
        std::cerr << e.what() << '\n';
        return 1;
    }
}
