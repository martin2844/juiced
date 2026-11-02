#include "Game/Scene/HeatHaze.h"

#include <windows.h>

#include <algorithm>
#include <cmath>

namespace oj::scene::haze {

namespace {

constexpr float kExhaustSize = 0.2f;
constexpr float kExhaustSpreadSize = 0.5f;
constexpr float kCoastSpread = 150.0f;
constexpr float kThrottleSpread = 40.0f;
constexpr float kThrottleBoost = 3.0f;
constexpr float kSpeedBoost = 2.0f;
constexpr float kFastSpeed = 4167.0f;

constexpr float kDrift = 150.0f;
constexpr float kDriftShare = 0.05f;
constexpr float kTrail = 80.0f;
constexpr float kTrailShare = 0.03f;
constexpr float kRise = 80.0f;
constexpr float kSink = 15.0f;

constexpr float kMapSize = 64.0f;  // textures\heathaze.ddt
constexpr float kMapDetail = 8.0f;
constexpr float kMaxVisible = 0.5f;
constexpr float kVisibleFade = 0.3f;

float dot(const Vec3& a, const Vec3& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// The distance along the ground from the emitter a particle is drawn at for its real one, behind a car at `speed`.
float trail(float distance, float speed) {
    const float t = kTrail + kTrailShare * speed;
    return t * (1.0f - std::exp(-distance / t));
}

float keptRise(float rise) {
    return rise >= 0.0f ? kRise * std::tanh(rise / kRise) : kSink * std::tanh(rise / kSink);
}

}  // namespace

void follow(Motion& m, const Vec3& position, float throttle, double now) {
    if (m.time < 0.0) {
        m = Motion{position, now, kStanding, 0.0f, {}, throttle};
        return;
    }
    const double step = now - m.time;
    if (step < kMotionStep) return;
    const Vec3 d = {position[0] - m.position[0], position[1] - m.position[1], position[2] - m.position[2]};
    m.speed = std::sqrt(dot(d, d)) / static_cast<float>(step);
    for (size_t a = 0; a < 3; ++a) m.velocity[a] = d[a] / static_cast<float>(step);
    const float target = kStanding + (1.0f - kStanding) * std::min(m.speed / kMovingSpeed, 1.0f);
    m.strength += (target - m.strength) * static_cast<float>(std::min(step / kMotionTime, 1.0));
    m.throttle += (throttle - m.throttle) * static_cast<float>(std::min(step / kThrottleTime, 1.0));
    m.position = position;
    m.time = now;
}

double clock() {
    LARGE_INTEGER now{}, frequency{};
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart != 0 ? static_cast<double>(now.QuadPart) / static_cast<double>(frequency.QuadPart) : 0.0;
}

float emitInterval(float step) {
    return step > 1e-4f && step < 1.0f ? kTimerStep * kEmitInterval / step : 0.0f;
}

float ownInterval(float speed) {
    return kStandingInterval + (kEmitInterval - kStandingInterval) * std::min(speed / kMovingSpeed, 1.0f);
}

float exhaustScale(float distance, float throttle) {
    const float spread = kCoastSpread + (kThrottleSpread - kCoastSpread) * throttle;
    return kExhaustSize + (kExhaustSpreadSize - kExhaustSize) * (1.0f - std::exp(-distance / spread));
}

float boost(float throttle, float speed) {
    return (1.0f + (kThrottleBoost - 1.0f) * throttle) * (1.0f + (kSpeedBoost - 1.0f) * std::min(speed / kFastSpeed, 1.0f));
}

Vec3 plume(const Vec3& d, const Vec3& up, float speed) {
    const float rise = dot(d, up);
    const float drift = std::min((kDrift + kDriftShare * speed) / std::max(speed, 1.0f), 1.0f);
    const Vec3 along = {(d[0] - up[0] * rise) * drift, (d[1] - up[1] * rise) * drift, (d[2] - up[2] * rise) * drift};
    const float distance = std::sqrt(dot(along, along));
    const float kept = distance > 1e-3f ? trail(distance, speed) / distance : 1.0f;
    const float riseKept = keptRise(rise);
    Vec3 drawn{};
    for (size_t a = 0; a < 3; ++a) drawn[a] = along[a] * kept + up[a] * riseKept;
    return drawn;
}

float orient(const Vec3& dir, const Vec3& a, const Vec3& b, Vec3& along, Vec3& across) {
    float x = dot(dir, a);
    float y = dot(dir, b);
    const float length = std::sqrt(dot(dir, dir));
    const float seen = std::sqrt(x * x + y * y);
    float share = 0.0f;
    if (length > 1e-6f && seen > 1e-6f) {
        share = seen / length;
        x /= seen;
        y /= seen;
        if (y < 0.0f) {
            x = -x;
            y = -y;
        }
    }
    x *= share;
    y = y * share + (1.0f - share);
    const float n = std::sqrt(x * x + y * y);
    x /= n;
    y /= n;
    for (size_t c = 0; c < 3; ++c) {
        along[c] = x * a[c] + y * b[c];
        across[c] = y * a[c] - x * b[c];
    }
    return share;
}

float mipBias(float pixels) {
    const float level = std::log2(kMapSize / std::max(pixels, 1.0f));
    return std::min(std::max(std::log2(kMapSize / kMapDetail) - level, -2.0f), 2.0f);
}

float visible(float screenHalf) {
    return std::min(std::max((kMaxVisible - screenHalf) / (kMaxVisible * kVisibleFade), 0.0f), 1.0f);
}

namespace {

// The end of a pipe: kPipeEnd along the -z of its node.
Vec3 pipeEnd(const Mat34& w) {
    return {w[3] - w[2] * kPipeEnd, w[7] - w[6] * kPipeEnd, w[11] - w[10] * kPipeEnd};
}

// The way out of a pipe: the -z of its node (unit; 0 without one).
Vec3 pipeAxis(const Mat34& w) {
    const Vec3 z = {-w[2], -w[6], -w[10]};
    const float length = std::sqrt(dot(z, z));
    return length > 1e-9f ? Vec3{z[0] / length, z[1] / length, z[2] / length} : Vec3{};
}

// A particle as drawn: its centre, its corners from it, its alpha (0 .. 255), the game's quad it comes from, its half
// size; at an exhaust its distance from the emitter and its direction along the plume.
struct Placed {
    Vec3 at{};
    std::array<Vec3, 4> corner{};
    float alpha = 0.0f;
    size_t quad = 0;
    float half = 0.0f;
    float distance = 0.0f;
    Vec3 dir{};
};

struct Facing {
    Vec3 a{}, b{};
    float aLength = 0.0f, bLength = 0.0f, stretch = 1.0f;
};

// The screen's half size of a quad (the larger of its spans / 2) and whether every corner is before the camera.
float screenHalf(const Vertex* corner, const float (&toClip)[16], bool& before) {
    float low[2] = {1e9f, 1e9f}, high[2] = {-1e9f, -1e9f};
    before = true;
    for (int k = 0; k < 4; ++k) {
        const float p[4] = {corner[k].x, corner[k].y, corner[k].z, 1.0f};
        float clip[4];
        for (int r = 0; r < 4; ++r) {
            clip[r] = p[0] * toClip[r * 4] + p[1] * toClip[r * 4 + 1] + p[2] * toClip[r * 4 + 2] + p[3] * toClip[r * 4 + 3];
        }
        if (clip[3] <= 1e-3f) {
            before = false;
            return 0.0f;
        }
        for (int a = 0; a < 2; ++a) {
            low[a] = std::min(low[a], clip[a] / clip[3]);
            high[a] = std::max(high[a], clip[a] / clip[3]);
        }
    }
    return std::max(high[0] - low[0], high[1] - low[1]) * 0.5f;
}

// `placed` drawn into `out` (once at each pipe of a double exhaust), faded by `strength` and by its size on the screen.
void put(const Placed& placed, const std::vector<Vertex>& game, const Exhaust& exhaust, float strength,
         const float (&toClip)[16], const Facing& facing, float viewWidth, std::vector<Vertex>& out) {
    std::array<Vec3, 4> corner = placed.corner;
    const Vertex* source = &game[placed.quad * 4];
    if (exhaust.valid) {
        // At an exhaust the corners along the plume: each on the side of the game's quad it is on.
        Vec3 along{}, across{};
        const float share = orient(placed.dir, facing.a, facing.b, along, across);
        const float length = placed.half * facing.bLength * (1.0f + (facing.stretch - 1.0f) * share);
        const float width = placed.half * facing.aLength;
        const Vec3 centre = {(source[0].x + source[1].x + source[2].x + source[3].x) * 0.25f,
                             (source[0].y + source[1].y + source[2].y + source[3].y) * 0.25f,
                             (source[0].z + source[1].z + source[2].z + source[3].z) * 0.25f};
        for (size_t k = 0; k < 4; ++k) {
            const Vec3 d = {source[k].x - centre[0], source[k].y - centre[1], source[k].z - centre[2]};
            const float sa = dot(d, facing.a) < 0.0f ? -1.0f : 1.0f;
            const float sb = dot(d, facing.b) < 0.0f ? -1.0f : 1.0f;
            for (size_t c = 0; c < 3; ++c) corner[k][c] = sa * width * across[c] + sb * length * along[c];
        }
    }
    const int copies = exhaust.valid && exhaust.twin ? 2 : 1;
    for (int c = 0; c < copies; ++c) {
        const float sign = copies == 1 ? 0.0f : (c == 0 ? -1.0f : 1.0f);
        const Vec3 centre = {placed.at[0] + (exhaust.valid ? exhaust.half[0] * sign : 0.0f),
                             placed.at[1] + (exhaust.valid ? exhaust.half[1] * sign : 0.0f),
                             placed.at[2] + (exhaust.valid ? exhaust.half[2] * sign : 0.0f)};
        Vertex quad[4];
        for (size_t k = 0; k < 4; ++k) {
            quad[k] = source[k];
            quad[k].x = centre[0] + corner[k][0];
            quad[k].y = centre[1] + corner[k][1];
            quad[k].z = centre[2] + corner[k][2];
        }
        bool before = true;
        const float half = screenHalf(quad, toClip, before);
        const float shown = (before ? visible(half) : 0.0f) * strength;
        const float fade = exhaust.valid ? 255.0f * std::pow(placed.alpha / 255.0f, kFade) : placed.alpha;
        const auto alpha = static_cast<uint32_t>(std::min(fade * shown, 255.0f) + 0.5f);
        const auto bias = static_cast<uint32_t>((mipBias(half * viewWidth) + 2.0f) / 8.0f * 255.0f + 0.5f);
        for (Vertex& v : quad) {
            v.colour = (v.colour & 0x00FF00FFu) | (alpha << 24) | (std::min<uint32_t>(bias, 255) << 8);
            out.push_back(v);
        }
    }
}

}  // namespace

Exhaust placeAtExhaust(const Mat34& root, const Mat34* pipe, const Mat34* other, float throttle, Vec3& local) {
    Exhaust e;
    if (pipe == nullptr && other == nullptr) return e;
    const Mat34& a = pipe != nullptr ? *pipe : *other;
    const Mat34& b = other != nullptr ? *other : *pipe;
    const Vec3 p = pipeEnd(a), q = pipeEnd(b);
    const Vec3 middle = {(p[0] + q[0]) * 0.5f, (p[1] + q[1]) * 0.5f, (p[2] + q[2]) * 0.5f};
    const Vec3 d = {middle[0] - root[3], middle[1] - root[7], middle[2] - root[11]};
    for (size_t j = 0; j < 3; ++j) {
        const Vec3 column = {root[j], root[4 + j], root[8 + j]};
        const float squared = dot(column, column);
        local[j] = squared > 1e-12f ? dot(column, d) / squared : 0.0f;
    }
    e.valid = true;
    e.twin = pipe != nullptr && other != nullptr;
    e.half = {(q[0] - p[0]) * 0.5f, (q[1] - p[1]) * 0.5f, (q[2] - p[2]) * 0.5f};
    e.emitter = middle;
    e.axis = pipeAxis(a);
    e.throttle = std::isfinite(throttle) ? std::min(std::max(throttle, 0.0f), 1.0f) : 0.0f;
    return e;
}

std::vector<Vertex> hazeQuads(const std::vector<Vertex>& game, const Axes& axes, const Exhaust& exhaust, float strength,
                              float speed, float throttle, const float (&toClip)[16], float viewWidth) {
    const size_t quads = game.size() / 4;
    std::vector<Vertex> out;
    if (quads == 0 || quads > kMaxParticles) return out;
    const float sideSquared = dot(axes.side, axes.side);
    const float upSquared = dot(axes.up, axes.up);
    if (sideSquared < 1e-12f || upSquared < 1e-12f) return out;
    Facing facing;
    facing.aLength = std::sqrt(sideSquared);
    facing.bLength = std::sqrt(upSquared);
    for (size_t c = 0; c < 3; ++c) {
        facing.a[c] = axes.side[c] / facing.aLength;
        facing.b[c] = axes.up[c] / facing.bLength;
    }
    facing.stretch = 1.0f + (kStretch - 1.0f) * std::min(speed / kMovingSpeed, 1.0f);

    std::vector<Placed> placed(quads);
    for (size_t q = 0; q < quads; ++q) {
        const Vertex* corner = &game[q * 4];
        const Vec3 centre = {(corner[0].x + corner[1].x + corner[2].x + corner[3].x) * 0.25f,
                             (corner[0].y + corner[1].y + corner[2].y + corner[3].y) * 0.25f,
                             (corner[0].z + corner[1].z + corner[2].z + corner[3].z) * 0.25f};
        const float size = std::fabs((corner[0].x - centre[0]) * axes.side[0] + (corner[0].y - centre[1]) * axes.side[1] +
                                     (corner[0].z - centre[2]) * axes.side[2]) /
                           sideSquared;
        // Where the particle is drawn: put back (+ view x size); at an exhaust in the plume, sized by its distance.
        Placed& p = placed[q];
        Vec3 drawn = {centre[0] + axes.view[0] * size, centre[1] + axes.view[1] * size, centre[2] + axes.view[2] * size};
        float scale = 1.0f;
        if (exhaust.valid) {
            const Vec3 d = {drawn[0] - exhaust.emitter[0], drawn[1] - exhaust.emitter[1], drawn[2] - exhaust.emitter[2]};
            const Vec3 shaped = plume(d, exhaust.up, speed);
            for (size_t a = 0; a < 3; ++a) drawn[a] = exhaust.emitter[a] + shaped[a];
            p.distance = std::sqrt(dot(shaped, shaped));
            scale = exhaustScale(p.distance, throttle);
            const float depth = toClip[12] * drawn[0] + toClip[13] * drawn[1] + toClip[14] * drawn[2] + toClip[15];
            if (depth > kFarSize) scale *= std::min(std::sqrt(depth / kFarSize), kMaxFarScale);
        }
        p.at = drawn;
        for (size_t k = 0; k < 4; ++k) {
            p.corner[k] = {(corner[k].x - centre[0]) * scale, (corner[k].y - centre[1]) * scale,
                           (corner[k].z - centre[2]) * scale};
        }
        p.alpha = static_cast<float>(corner[0].colour >> 24);
        p.quad = q;
        p.half = size * scale;
    }
    if (exhaust.valid) {
        // Along the plume: from the emitter on; the first one along the way out of the pipe, the others from their
        // neighbour before to after.
        std::stable_sort(placed.begin(), placed.end(),
                         [](const Placed& x, const Placed& y) { return x.distance < y.distance; });
        for (size_t i = 0; i < quads; ++i) {
            const Vec3* from = i > 0 ? &placed[i - 1].at : nullptr;
            const Vec3& to = i + 1 < quads ? placed[i + 1].at : placed[i].at;
            for (size_t c = 0; c < 3; ++c) placed[i].dir[c] = from != nullptr ? to[c] - (*from)[c] : exhaust.axis[c];
        }
    }
    for (size_t i = 0; i < quads; ++i) {
        put(placed[i], game, exhaust, strength, toClip, facing, viewWidth, out);
        if (!exhaust.valid || i + 1 == quads) continue;
        // Between this particle and the next one along the plume, where they are apart: quads half their size apart.
        const Placed& a = placed[i];
        const Placed& b = placed[i + 1];
        const Vec3 gap = {b.at[0] - a.at[0], b.at[1] - a.at[1], b.at[2] - a.at[2]};
        const float reach = std::max(0.5f * (a.half + b.half), 1e-3f);
        const int count = std::min(static_cast<int>(std::sqrt(dot(gap, gap)) / reach), kMaxBetween);
        for (int n = 1; n <= count; ++n) {
            const float s = static_cast<float>(n) / static_cast<float>(count + 1);
            Placed mid;
            for (size_t c = 0; c < 3; ++c) {
                mid.at[c] = a.at[c] + gap[c] * s;
                for (size_t k = 0; k < 4; ++k) mid.corner[k][c] = a.corner[k][c] + (b.corner[k][c] - a.corner[k][c]) * s;
                mid.dir[c] = gap[c];
            }
            mid.alpha = a.alpha + (b.alpha - a.alpha) * s;
            mid.quad = s < 0.5f ? a.quad : b.quad;
            mid.half = a.half + (b.half - a.half) * s;
            mid.distance = a.distance + (b.distance - a.distance) * s;
            put(mid, game, exhaust, strength, toClip, facing, viewWidth, out);
        }
    }
    return out;
}

}  // namespace oj::scene::haze
