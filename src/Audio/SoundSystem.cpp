#include "Audio/SoundSystem.h"

#include "Audio/Player.h"
#include "Audio/Stream.h"

#include <windows.h>

#include <cmath>

namespace oj::audio {

namespace {

constexpr int32_t kVolumeMin = -10000;  // DSBVOLUME_MIN

}  // namespace

namespace {
System* g_system = nullptr;
}

System& system() {
    static System s;
    return g_system != nullptr ? *g_system : s;
}

void setSystem(System* system) {
    g_system = system;
}

namespace {
bool g_active = true;
}

void setActive(bool active) { g_active = active; }
bool active() { return g_active; }

void System::update() {
    Stream::focus(g_active);
    Player::updateAll();
}

int32_t volumeMb(float volume) {
    if (0.0f < volume) {
        const double v = volume <= 1.0f ? volume : 1.0;
        // x87: log2 (fyl2x), times log10(2) and 2000 in extended precision, stored as a float, then fistp.
        const float mb = static_cast<float>(0.3010299956639812 * std::log2(v) * 2000.0);
        const int32_t r = static_cast<int32_t>(std::nearbyint(mb));
        if (-10001 < r) return r > 0 ? 0 : r;
    }
    return kVolumeMin;
}

uint32_t clockSeconds() {
    static const float frequency = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return static_cast<float>(f.QuadPart);
    }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    const float seconds = static_cast<float>(static_cast<double>(c.QuadPart) / frequency);
    return static_cast<uint32_t>(static_cast<int32_t>(seconds));
}

System::System() : m_output(makeOutput()) {}

void System::enableEax(bool on) {
    m_flags = on ? (m_flags & ~kNoEax) : (m_flags | kNoEax);
    if (m_output) m_output->enableEax(on);
}

System::System(std::unique_ptr<Output> output) : m_output(std::move(output)) {}

System::~System() = default;

bool System::start(HWND window, uint32_t configFlags) {
    if ((configFlags & kNoSound) != 0) {
        m_flags = kNoSound;
        return false;
    }
    if (m_on) return true;
    OutputStart started;
    started.minFrequency = m_minFrequency;
    started.hardware3dStreaming = m_hw3d;
    const bool on = m_output->start(window, configFlags, started);
    m_flags = started.flags;
    m_eaxDevice = started.eaxDevice;
    m_eaxFlags = started.eaxFlags;
    m_minFrequency = started.minFrequency;
    m_hw3d = started.hardware3dStreaming;
    if (!on) return false;
    m_on = true;
    return true;
}

bool System::setEax(const GUID& set, uint32_t id, const void* data, uint32_t bytes) {
    return m_output->setEax(set, id, data, bytes);
}

bool System::getEax(const GUID& set, uint32_t id, void* data, uint32_t bytes) {
    return m_output->getEax(set, id, data, bytes);
}

void System::stop() {
    m_output->stop();
    m_on = false;
}

bool System::hardwarePlays3d() {
    if (m_hardwareAsked >= 0) return m_hardwareAsked != 0;
    const bool plays = m_output->hardwarePlays3d(m_hw3d);
    m_hardwareAsked = plays ? 1 : 0;
    return plays;
}

}  // namespace oj::audio
