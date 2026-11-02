#include "Core/Input/PadRumble.h"

#include <algorithm>
#include <chrono>

namespace oj::input {

double PadRumble::now() {
    using Ms = std::chrono::duration<double, std::milli>;
    return std::chrono::duration_cast<Ms>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void PadRumble::frame(double ms, Motors motors) {
    m_newest = (m_newest + 1) % kHistory;
    Frame& f = m_frames[static_cast<size_t>(m_newest)];
    f.time = ms;
    f.motors = {std::clamp(motors.heavy, 0.0f, 1.0f), std::clamp(motors.light, 0.0f, 1.0f)};
}

Motors PadRumble::levels(double ms, float strength) const {
    if (m_newest < 0) return {};
    const Frame& newest = m_frames[static_cast<size_t>(m_newest)];
    const double age = ms - newest.time;
    if (age < 0.0 || age >= kHeld) return {};
    Motors m = newest.motors;
    for (int i = 1; i < kHistory; ++i) {
        const Frame& f = m_frames[static_cast<size_t>((m_newest - i + kHistory) % kHistory)];
        const double frameAge = ms - f.time;
        if (frameAge < 0.0 || frameAge >= kConsoleFrame) break;
        m.heavy = std::max(m.heavy, f.motors.heavy);
        m.light = std::max(m.light, f.motors.light);
    }
    return {std::min(1.0f, m.heavy * strength), std::min(1.0f, m.light * strength)};
}

Motors PadRumble::newest() const {
    return m_newest < 0 ? Motors{} : m_frames[static_cast<size_t>(m_newest)].motors;
}

void PadRumble::clear() {
    m_frames = {};
    m_newest = -1;
}

float slideLight(float slide) {
    if (slide <= 0.05f) return 0.0f;
    return std::min(1.0f, slide < 0.08f ? 0.2f + (slide - 0.06f) * 4.0f : slide + 0.2f);
}

}  // namespace oj::input
