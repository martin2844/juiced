// The players' messages and the HUD's times.

#include "Game/Race/Hud/HudRace.h"
#include "Game/Race/Hud/HudInternal.h"

#include <cmath>

namespace oj {

using namespace hud;

std::string hudTime(float seconds) {
    if (seconds < 0.0f) seconds = 0.0f;
    const auto whole = static_cast<uint32_t>(static_cast<int32_t>(seconds));
    const uint32_t minutes = whole / 60;
    float rest = seconds - static_cast<float>(static_cast<int32_t>(minutes * 60));
    if (rest < 0.0f) rest = 0.0f;
    const auto secs = static_cast<int32_t>(rest);
    double integral = 0.0;
    const double fraction = std::modf(static_cast<double>(rest), &integral);
    const auto hundredths = static_cast<int32_t>(static_cast<float>(fraction) * 99.0f);
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", static_cast<int>(minutes), secs, hundredths);
    return buffer;
}

void HudMessages::clear() {
    list.clear();
    changed = false;
}

bool HudMessages::push(const std::wstring& text, float time) {
    if (list.size() >= 10) return false;
    list.push_back({text, time, colour});
    changed = true;
    return true;
}

bool HudMessages::push(const std::wstring& text, float time, uint32_t ownColour) {
    if (list.size() >= 10) return false;
    list.push_back({text, time, ownColour});
    changed = true;
    return true;
}

void HudMessages::update(float dt) {
    if (list.empty()) {
        alpha = 0.0f;
        return;
    }
    const float step = dt < 0.0f ? 0.0f : dt > 0.1f ? 0.1f : dt;
    Message& first = list.front();
    first.time = first.time - step;
    alpha = first.time < 0.5f ? first.time + first.time : 1.0f;
    if (first.time < 0.0f) {
        list.erase(list.begin());
        if (!list.empty()) changed = true;
    }
}

}  // namespace oj
