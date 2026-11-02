#include "Game/Scene/Clouds.h"

#include "Game/Scene/LightSets.h"
#include "Render/Camera.h"
#include "Render/Renderer3d.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace oj::scene {

namespace {

bool contains(std::string_view text, std::string_view part) {
    return std::search(text.begin(), text.end(), part.begin(), part.end(), [](char a, char b) {
               return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
           }) != text.end();
}

// The part of an offset within its tile (the noise tiles at 1).
float within(double offset) {
    return static_cast<float>(offset - std::floor(offset));
}

}  // namespace

bool skyCloud(std::string_view file, std::string_view object) {
    return contains(file, "sky") && contains(object, "cloud") && !contains(object, "moon") &&
           !contains(object, "lightning");
}

float cloudWind(uint32_t timeOfDay, bool wet) {
    if (wet) return kCloudWindRain;
    return timeOfDay == 1 || timeOfDay == 3 ? kCloudWindDusk : kCloudWindNoon;
}

render::CloudView cloudView(const std::array<std::array<float, 4>, 2>& rows, const Mat34& eye, const Vec4& light,
                            const Vec4& sunColour, const Vec4& ambient, uint32_t zenith, bool night, double travel,
                            const LightSet& set, bool volume) {
    render::CloudView v;
    v.height = kCloudHeight;
    v.fade = 1.0f / kCloudFade;
    v.cover = std::clamp(set.cloudCover, 0.0f, 1.0f);
    v.sharpness = kCloudSharpness;
    v.project = rows;
    v.eye = {eye[3], eye[7], eye[11]};
    v.detailScale = kCloudDetailScale;
    // The shapes along (0.8, 0.6) of the map, the details faster their own way, (0.6, -0.8) of theirs.
    const double shapes = travel / kCloudTile;
    const double details = shapes * kCloudDetailWind * kCloudDetailScale;
    v.wind = {within(shapes * 0.8), within(shapes * 0.6), within(details * 0.6), within(-details * 0.8)};
    // Toward the sun: against the light; a step toward it across the ground in the map's coordinates.
    const float length = std::sqrt(light[0] * light[0] + light[1] * light[1] + light[2] * light[2]);
    if (length > 1e-6f) v.toSun = {-light[0] / length, -light[1] / length, -light[2] / length};
    const float across = std::sqrt(v.toSun[0] * v.toSun[0] + v.toSun[2] * v.toSun[2]);
    if (across > 1e-4f) {
        const float x = v.toSun[0] / across * kCloudSunStep, z = v.toSun[2] / across * kCloudSunStep;
        v.sunStep = {rows[0][0] * x + rows[0][2] * z, rows[1][0] * x + rows[1][2] * z};
    }
    // The atmosphere's colours (0 .. 255): the sun's light, the clouds' shade from the scene's ambient — which is the
    // fill light and no measure of the day: a night atmosphere's is brighter than a noon one's (Angel North Central:
    // 90, 80, 90 against 73, 73, 62), so the clouds would be brighter by night than by day (the user, 2026-10-29:
    // HI's night sky was too bright). By night the ambient takes a small share and the sky's own light the rest (the
    // atmosphere's zenith colour: 13, 9, 18 against the noon's 80, 131, 161), so that a night sky's clouds stay just
    // off the sky's colour, lit by the city about the horizon (the sky dome's own colour, the shaders' `colour`).
    // By day the stock's own share is kept.
    const float share = night ? 0.35f : 1.2f;
    for (size_t k = 0; k < 3; ++k) {
        v.sun[k] = std::clamp(sunColour[k] / 255.0f, 0.0f, 1.5f);
        const float zenithK = static_cast<float>(zenith >> (16 - 8 * k) & 0xFF) / 255.0f;
        v.ambient[k] = std::clamp(ambient[k] / 255.0f * share + (night ? zenithK * 1.2f : 0.0f) + 0.04f, 0.0f, 1.0f);
    }
    v.opacity = set.cloudOpacity;
    v.clearLight = kCloudMean + (kCloudClear - kCloudMean) * set.cloudShadows;
    v.cloudLight = kCloudMean + (kCloudShadow - kCloudMean) * set.cloudShadows;
    if (!volume) return v;
    // The volume: the weather moved by the wind along (0.8, 0.6) in the world's x and z, the shapes with it and
    // rising slowly, the details with them and rising faster (the clouds' edges billowing as they go); each offset
    // within its tile.
    v.volume = true;
    v.top = kCloudTop;
    v.extinction = kCloudExtinction;
    v.steps = kCloudSteps;
    v.lightSteps = kCloudLightSteps;
    v.farthest = kCloudFarthest;
    v.weatherScale = 1.0f / kCloudWeather;
    v.shapeScale = 1.0f / kCloudShapes;
    v.erosionScale = 1.0f / kCloudErosion;
    v.overcast = std::clamp((v.cover - kCloudOvercast) / (1.0f - kCloudOvercast), 0.0f, 1.0f);
    const double moved = travel;
    v.wind[0] = within(-moved * 0.8 / kCloudWeather);
    v.wind[1] = within(-moved * 0.6 / kCloudWeather);
    v.shapeDrift = {within(-moved * 0.8 / kCloudShapes), within(-moved * 0.1 / kCloudShapes),
                    within(-moved * 0.6 / kCloudShapes)};
    v.erosionDrift = {within(-moved * 0.8 / kCloudErosion), within(-moved * 0.3 / kCloudErosion),
                      within(-moved * 0.6 / kCloudErosion)};
    return v;
}

render::CloudView reflectionView(const render::CloudView& view) {
    render::CloudView v = view;
    v.steps = kCloudReflectionSteps;
    v.lightSteps = 0.0f;
    v.screen = false;
    return v;
}

Clouds::Clouds(render::IRenderDevice& device) : m_device(device) {}

Clouds::~Clouds() {
    if (m_map != 0) m_device.destroyTexture(m_map);
}

bool Clouds::update(render::Renderer3d& r, render::Camera& camera, const LightSet& set, bool volume, float wind) {
    if (m_map == 0) m_map = m_device.createRenderTarget(kCloudMapSize, kCloudMapSize, false);
    if (m_map == 0) return false;
    // The cover eased toward the set's, at once the first time and on a new clock.
    const float dt = r.time - m_time;
    if (m_cover < 0.0f || dt < 0.0f) {
        m_cover = set.cloudCover;
        m_travel = static_cast<double>(r.time) * wind;
    } else {
        m_cover += (set.cloudCover - m_cover) * std::min(dt / kCloudCoverEase, 1.0f);
        m_travel += static_cast<double>(dt) * wind;
    }
    m_time = r.time;
    LightSet look = set;
    look.cloudCover = m_cover;
    m_view = cloudView(r.cloudRows(), camera.world(), r.lightDirection, r.light1Colour, r.ambient, r.zenith, r.night,
                       m_travel, look, volume);
    m_reflected = reflectionView(m_view);
    if (!m_device.cloudMap(m_map, m_view)) return false;
    // The volume's main view at half its size, blended over the frames (its clip rows: a place at no distance). The
    // march itself every other frame (the user, 2026-10-29: the clouds cost frame time): the frames' blend and the
    // dome's sampling of the last one hold it either way, so the picture is the same at half the cost. The map, the
    // ground's shadows, is drawn every frame (it is the view's own window in the world, and a frame behind would show
    // as the shadows' sliding at speed).
    if (volume) {
        const Mat44& m = camera.worldToClip();
        for (size_t k = 0; k < 3; ++k) {
            m_view.clip[0][k] = m[k];
            m_view.clip[1][k] = m[4 + k];
            m_view.clip[2][k] = m[12 + k];
        }
        if (m_frames % 2 == 0) {
            m_view.seconds = r.time;
            m_device.profileMark("clouds' view");
            m_marched = m_device.cloudView(m_view);
        }
        m_view.screen = m_marched;
    }
    ++m_frames;
    return true;
}

}  // namespace oj::scene
