#include "Game/Race/RaceIntro.h"

#include "Core/Base/GameFiles.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace oj::scene {

namespace {

using Quaternion = std::array<float, 4>;  // w, x, y, z

// 0x0041C010: the Catmull-Rom curve through four points at t (the shot's positions with the first and the last
// doubled), each step rounded to a float.
std::array<float, 4> catmullRom(const std::array<float, 4>& p0, const std::array<float, 4>& p1,
                                const std::array<float, 4>& p2, const std::array<float, 4>& p3, float t) {
    const float t2 = t * t;
    const float t3 = t2 * t;
    std::array<float, 4> out{};
    for (int i = 0; i < 3; ++i) {
        const float a = p1[i] + p1[i];
        const float b = (p2[i] - p0[i]) * t;
        const float c = (((p0[i] + p0[i]) - p1[i] * 5.0f) + p2[i] * 4.0f) - p3[i];
        const float d = ((-p0[i] + p1[i] * 3.0f) - p2[i] * 3.0f) + p3[i];
        out[i] = (((a + b) + c * t2) + d * t3) * 0.5f;
    }
    out[3] = p1[3];
    return out;
}

// 0x00407960: sin of a float, the result stored as a float.
float sinFloat(float v) {
    return static_cast<float>(std::sin(static_cast<double>(v)));
}

// 0x00554830: from a toward b by t the shorter way (b negated for a negative dot product); within 1e-6 of no angle
// a straight blend.
Quaternion slerp(const Quaternion& a, const Quaternion& b, float t) {
    float dot = ((a[0] * b[0] + a[1] * b[1]) + a[2] * b[2]) + a[3] * b[3];
    Quaternion to = b;
    if (!(0.0f <= dot)) {
        dot = -dot;
        for (float& v : to) v = v * -1.0f;
    }
    const auto angle = static_cast<float>(std::acos(static_cast<double>(dot)));
    float fromScale = 1.0f - t;
    float toScale = t;
    if (!(angle > -1e-6f) || !(angle < 1e-6f)) {
        const float inverse = 1.0f / sinFloat(angle);
        fromScale = sinFloat((1.0f - t) * angle) * inverse;
        toScale = sinFloat(angle * t) * inverse;
    }
    Quaternion out{};
    for (int i = 0; i < 4; ++i) out[i] = a[i] * fromScale + to[i] * toScale;
    return out;
}

}  // namespace

bool RaceIntro::load(const std::string& route) {
    m_shots.clear();
    m_shot = 0;
    m_index = 0.0f;
    m_fadeLeft = 0.0f;
    m_waited = 0.0f;
    m_done = false;
    std::string name = route;
    for (char& ch : name) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    const GameFiles files(std::filesystem::current_path());
    const auto bytes = files.read(std::filesystem::path("scripts") / (name + "_intro.cam"));
    if (!bytes) return false;
    std::vector<uint8_t> data(bytes->size());
    if (!data.empty()) std::memcpy(data.data(), bytes->data(), data.size());
    auto shots = parseIntroCam(data);
    if (!shots || shots->empty()) return false;
    m_shots = std::move(*shots);
    return true;
}

void RaceIntro::clear() {
    m_shot = 0;
    m_index = 0.0f;
    m_fadeLeft = 0.0f;
    m_waited = 0.0f;
}

void RaceIntro::place(const std::array<float, 4>& p, const Quaternion& q, float fov, render::Camera& camera) {
    // The rotation of (w, x, y, z), then the position (0x0041B520 with a translation).
    oj::Mat34& m = camera.local;
    m = rotationOf(q);
    m[3] = p[0];
    m[7] = p[1];
    m[11] = p[2];
    camera.invalidate();
    m_fov = fov;
}

bool RaceIntro::update(float dt, render::Camera& camera, bool wideScreen) {
    if (m_done || m_shots.empty()) return false;
    // The fade of a new shot (the race's fade object: its progress 1 - left, then its end).
    if (m_fadeLeft > 0.0f) m_fadeLeft -= dt;

    bool going = true;
    const IntroShot& shot = m_shots[m_shot];
    if (m_waited < shot.wait) {
        m_waited = dt + m_waited;
        if (!shot.keys.empty()) place(shot.keys[0].position, shot.keys[0].rotation, shot.keys[0].fov, camera);
    } else {
        const auto count = static_cast<uint32_t>(shot.keys.size());
        // The key: cut toward zero (x87 chop, or CVTSS2SI with the game's MXCSR rounding toward zero).
        const auto from = static_cast<uint32_t>(static_cast<int32_t>(m_index));
        const float time = from < count ? shot.keys[from].time : 1.0f;
        m_index = (1.0f / time) * dt + m_index;
        const auto k = static_cast<uint32_t>(static_cast<int32_t>(m_index));
        if (k < count) {
            const float t = m_index - static_cast<float>(static_cast<int32_t>(k));
            const IntroKey& key = shot.keys[k];
            std::array<float, 4> position = key.position;
            if (k != count - 1) {
                auto at = [&](int64_t i) -> const std::array<float, 4>& {
                    return shot.keys[static_cast<size_t>(std::clamp<int64_t>(i, 0, count - 1))].position;
                };
                position = catmullRom(at(int64_t{k} - 1), at(k), at(int64_t{k} + 1), at(int64_t{k} + 2), t);
            }
            Quaternion rotation = key.rotation;
            float fov = key.fov;
            if (k + 1 < count) {
                const IntroKey& next = shot.keys[k + 1];
                rotation = slerp(key.rotation, next.rotation, t);
                fov = (1.0f - t) * key.fov + t * next.fov;
            }
            place(position, rotation, fov, camera);
        } else {
            ++m_shot;
            m_index = 0.0f;
            m_waited = 0.0f;
            if (m_shot >= m_shots.size()) {
                m_shot = 0;
                m_fadeLeft = 0.0f;
                m_done = true;
                going = false;
            } else {
                m_fadeLeft = 1.0f;  // [0x00711A08]
            }
        }
    }
    // 0x00405BE0: the field of view brought into 0.05 .. 1.5, the far plane 2000000, the camera's field of view
    // that x 1.1623551.
    float fov = 0.05f;
    if (0.05f <= m_fov) fov = 1.5f < m_fov ? 1.5f : m_fov;
    m_fov = fov;
    camera.setFar(2000000.0f);
    camera.setFov(fov * std::bit_cast<float>(0x3F94C80Du), 0.0f, wideScreen);  // 1.1623551 (0x006FFBC8)
    return going;
}

}  // namespace oj::scene
