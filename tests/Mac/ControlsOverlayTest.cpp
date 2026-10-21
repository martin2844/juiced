#include "Mac/ControlsOverlay.h"
#include "../Framework/Fakes/NullRenderDevice.h"
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace oj;
using namespace oj::mac;
using Result = ControlsOverlay::Result;
void require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    const auto folder = std::filesystem::temp_directory_path() / ("juiced-overlay-" + std::to_string(getpid()));
    const auto file = folder / "controls.ini";
    try {
        if (argc > 1) std::filesystem::current_path(argv[1]);
        test::NullRenderDevice device;
        const auto language = *Language::byIndex(0);
        ControlsOverlay menu(device, language, file);
        if (argc > 1) require(menu.loaded(), "Retail options and message-box templates must load");
        auto key = [&](uint8_t code, bool shortcut = false) {
            return menu.update({{code, true}}, shortcut);
        };
        key(28); // Driving category.
        key(28); // Accelerate.
        require(menu.captureRow() == 0, "Native menu selection opens binding capture");
        key(1);
        require(menu.captureRow() == -1, "Escape cancels capture without leaving the category");
        key(28);
        key(31, true);
        require(menu.captureRow() == 0, "Command shortcut cannot become a binding");
        key(28);
        require(menu.captureRow() == 0, "Enter remains reserved");
        key(42);
        require(menu.bindings()[0] == "DIKEYBOARD_LSHIFT" && menu.bindings()[4] == "DIKEYBOARD_W",
                "Shift capture swaps conflict");
        key(1); // Categories.
        key(208); key(208); key(208); key(28); // Restore defaults.
        require(menu.bindings() == defaultBindings(), "Restore defaults stays in editor");
        key(28); // Dismiss the stock message box.
        key(200); key(200); key(28); key(28); // Gears, first binding.
        key(20);
        require(menu.bindings()[7] == "DIKEYBOARD_T", "Second category edits the gear binding");
        key(1); key(208); key(28); key(28); // Crew, first binding.
        key(33);
        require(menu.bindings()[14] == "DIKEYBOARD_F", "Third category edits crew commands");
        key(1); key(208); key(208); key(208);
        require(key(28) == Result::Saved, "Save and return commits the bindings");
        const auto saved = loadBindings(file);
        require(saved == menu.bindings(), "Saved values survive reopening");
        ControlsOverlay cancel(device, language, file);
        auto cancelKey = [&](uint8_t code) { return cancel.update({{code, true}}, false); };
        cancelKey(28); cancelKey(28); cancelKey(32); cancelKey(1);
        require(cancelKey(1) == Result::Cancelled && loadBindings(file) == saved,
                "Escape from root discards draft and preserves saved values");
        std::filesystem::remove_all(folder);
        std::cout << "Native menu navigation, capture, conflict, defaults, save and cancel checks passed\n";
    } catch (const std::exception &e) {
        std::filesystem::remove_all(folder);
        std::cerr << e.what() << '\n';
        return 1;
    }
}
