#include "TouchControls.h"
namespace oj::android {
void TouchControls::update(int64_t finger, float x, float y) {
    int key = -1;
    if (y > .65f && y <= 1 && x >= 0 && x <= 1) {
        if (x < .18f)
            key = Left;
        else if (x < .36f)
            key = Right;
        else if (x > .82f)
            key = Throttle;
        else if (x > .64f)
            key = Brake;
    } else if (x > .82f && x <= 1 && y >= 0 && y < .2f)
        key = Select;
    else if (x >= 0 && x < .18f && y >= 0 && y < .2f)
        key = Back;
    fingers[finger] = key;
}
std::array<bool, TouchControls::Count> TouchControls::held() const {
    std::array<bool, Count> out{};
    for (auto [finger, key] : fingers)
        if (key >= 0)
            out[key] = true;
    return out;
}
} // namespace oj::android
