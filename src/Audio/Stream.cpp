#include "Audio/Stream.h"

#include "Audio/SoundSystem.h"

#include <vector>

namespace oj::audio {

namespace {

std::vector<Stream*>& streams() {
    static std::vector<Stream*> s;
    return s;
}

}  // namespace

Stream::Stream(const std::filesystem::path& file) {
    streams().push_back(this);
    m_out = system().output().openStream(file);
    m_state = 1;
}

Stream::~Stream() {
    m_out.reset();
    auto& all = streams();
    for (size_t i = 0; i < all.size(); ++i) {
        if (all[i] == this) {
            all.erase(all.begin() + static_cast<ptrdiff_t>(i));
            break;
        }
    }
}

void Stream::stop() {
    m_out->stop();
    m_state = 1;
}

void Stream::play(float start, float length) {
    constexpr int32_t units = 10000;  // [0x00725844]
    // fild, fmul, fistp with the control word cut: 10000 x start in extended precision, cut.
    const int32_t a = static_cast<int32_t>(static_cast<double>(units) * static_cast<double>(start));
    const int64_t scale = 10000000 / units;
    const int64_t current = static_cast<int64_t>(a) * scale;
    const int64_t b = static_cast<int64_t>(static_cast<double>(units) * static_cast<double>(length));
    const int64_t stopAt = (static_cast<int64_t>(static_cast<int32_t>(b) + a)) * scale;
    if (m_out->setPositions(current, stopAt)) {
        m_out->run();
        m_stop = stopAt;
        m_stopAsked = true;
    }
    m_state = 3;
}

void Stream::setVolume(int32_t mb) {
    m_volume = mb;
    m_out->setVolume(mb);
}

void Stream::pause(int paused) {
    if (paused == 1) {
        m_out->setVolume(-10000);
        m_out->stop();
        m_state = 2;
        return;
    }
    if (!active()) return;
    m_out->run();
    m_out->setVolume(m_volume);
    m_state = 3;
}

int Stream::state() {
    if (!m_out->seekable()) return m_state;
    int64_t current = 0;
    int64_t end = 0;
    m_out->positions(current, end);
    if (current == end) m_state = 1;
    if (current >= m_stop) m_state = 1;
    return m_state;
}

void Stream::focus(bool active) {
    for (Stream* s : streams()) {
        if (!active) {
            if (s->m_state == 3) {
                s->m_focusPaused = true;
                s->pause(1);
            }
        } else if (s->m_state == 2 && s->m_focusPaused) {
            s->m_focusPaused = false;
            s->pause(0);
        }
    }
}

}  // namespace oj::audio
