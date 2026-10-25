#include "Android/TouchBindings.h"
#undef NDEBUG
#include <cassert>
#include <iostream>
int main() {
    oj::ActionMap driver;
    driver.name = "DRIVER1";
    driver.device = "ACTION_MAP";
    oj::ControlAction steering;
    steering.name = "STEERING";
    steering.control = "DIKEYBOARD_LEFT";
    steering.control2 = "DIKEYBOARD_RIGHT";
    oj::ControlAction gas;
    gas.name = "THROTTLE";
    gas.control = "DIKEYBOARD_UP";
    oj::ControlAction gear;
    gear.name = "GEAR_UP";
    gear.control = "DIKEYBOARD_LCONTROL";
    driver.actions = {steering, gas, gear};
    auto stock = oj::android::resolveTouchBindings({driver});
    assert(stock[0] == 203 && stock[1] == 205 && stock[2] == 200 && stock[8] == 29);
    driver.actions[0].control = "DIKEYBOARD_A";
    driver.actions[0].control2 = "DIKEYBOARD_D";
    driver.actions[1].control = "DIKEYBOARD_W";
    driver.actions[2].control = "DIKEYBOARD_E";
    auto modded = oj::android::resolveTouchBindings({driver});
    assert(modded[0] == 30 && modded[1] == 32 && modded[2] == 17 && modded[8] == 18);
    driver.actions[1].control = "DIKEYBOARD_T";
    auto custom = oj::android::resolveTouchBindings({driver});
    assert(custom[2] == 20);
    oj::ActionMap ui = driver;
    ui.name = "UI";
    ui.actions[1].control = "DIKEYBOARD_0";
    assert(oj::android::resolveTouchBindings({driver, ui})[2] == 20);
    std::cout << "Touch bindings: stock, modded, custom and UI isolation passed\n";
}
