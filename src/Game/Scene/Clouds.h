#pragma once
// The fixed build's procedural clouds (SKY HI and ULTRA; the user, 2026-10-28: the stock's clouds are flat textures and
// their shadows on the ground a texture of their own; wanted procedural clouds, a noise baked once and kept in memory,
// the shadows on the ground those of the clouds; then SKY its own row, ULTRA clouds of three dimensions):
// JuicedConfigExt's CloudPass draws, with HI, a layer of clouds kCloudHeight above the view from a tiling noise baked
// once, its shapes and details moved by the wind. The layer lies in the world's cloud shadows' own coordinates (the
// effects' cCloudProject, c46 and c47: the world projected along the sun's direction; the renderer's cloudScale gives
// kCloudTile a tile and no scroll), so each frame's map of the clouds' light (IRenderDevice::cloudMap), in the effects'
// cloudcast texture's place (Engine::setCloudCast), shades the ground where the sun casts the clouds' shadows; the sky
// dome drawn with the clouds over its colours (IRenderDevice::cloudSky, its reflections in the cubes and the wet road
// too) in place of the sky's own clouds (skyCloud, left out). With ULTRA the clouds are a volume from kCloudHeight to
// kCloudTop above the view in the world's own places (CloudView::volume): the baked noise their weather, a volume
// noise their shapes (16 bits: the user, 2026-10-29, eight made them ribbed); the main view's clouds marched through it
// at half its size (kCloudSteps, each step's light through it toward the sun) and blended over the frames
// (IRenderDevice::cloudView: no noise of the steps, no ripple as the view moves), the dome laying them over its
// colours; the reflections' marched in their own draws, fewer steps and none toward the sun (reflectionView); the map a
// window of kCloudWindow about the view, each texel the light along the sun's way through the volume. The light case's
// set gives the cover, the shadows' depth and the opacity (Game/Scene/LightSets).

#include "Core/Base/Matrix.h"
#include "Render/RenderDevice.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace oj::render {
class Camera;
class Renderer3d;
}  // namespace oj::render

namespace oj::scene {

struct LightSet;

constexpr float kCloudHeight = 150000.0f;     // the layer 1.5 km above the view
constexpr float kCloudTile = 400000.0f;       // the map's tile and the noise's: 4 km
constexpr float kCloudFade = 3000000.0f;      // the layer into the sky's colour 30 km along it
// The wind by the race's weather and time of day (the user, 2026-10-29: clouds pass some 110 .. 220 m in 11 s; the
// stock's sky quads turn at up to 0.002 .. 0.007 a second, its cloud shadows scroll at some 100 m/s): in the rain
// 144 km/h, by morning and evening 100 km/h, by noon and night 70 km/h (the user: night slow, as the day; then each
// twice as fast).
constexpr float kCloudWindRain = 4000.0f;
constexpr float kCloudWindDusk = 2778.0f;
constexpr float kCloudWindNoon = 1944.0f;
// The wind of a race's time of day (TrackSetup's: 1 morning, 3 evening, 4 night, else noon) and weather.
float cloudWind(uint32_t timeOfDay, bool wet);
constexpr float kCloudDetailWind = 1.6f;      // the details' over the shapes', their own way
constexpr float kCloudDetailScale = 8.0f;     // the details' coordinates over the shapes'
constexpr float kCloudSunStep = 30000.0f;     // a step toward the sun along the layer: 300 m
constexpr float kCloudSharpness = 6.0f;
constexpr uint32_t kCloudMapSize = 512;       // 8 m a texel
constexpr float kCloudCoverEase = 5.0f;       // seconds: a change of the cover eased over them
// The map's light (the world's materials take 0.5 + 0.7 x it): the stock texture's mean (0.44) under the clouds'
// shadows' depth 0, under a clear sky and a whole cloud kCloudClear and kCloudShadow at depth 1.
constexpr float kCloudMean = 0.44f;
constexpr float kCloudClear = 0.56f;
constexpr float kCloudShadow = 0.08f;
// The volume (SKY ULTRA).
constexpr float kCloudTop = 270000.0f;           // its top 2.7 km above the view (1.2 km thick)
constexpr float kCloudWindow = 800000.0f;        // the map's window about the view: 8 km (16 m a texel)
constexpr float kCloudWeather = 1000000.0f;      // the weather's tile (the baked noise): 10 km
constexpr float kCloudShapes = 160000.0f;        // the shapes' tile (the volume noise): 1.6 km
constexpr float kCloudErosion = 60000.0f;        // the details': 600 m (no finer than the steps through them)
constexpr float kCloudExtinction = 0.00015f;     // of the light a world unit through a whole cloud (1.5 % a metre)
constexpr float kCloudSteps = 40.0f;             // the main view's steps through the volume
constexpr float kCloudLightSteps = 5.0f;         // each step's toward the sun
constexpr float kCloudReflectionSteps = 8.0f;    // the reflections' (none toward the sun)
constexpr float kCloudFarthest = 4000000.0f;     // the farthest distance along a view through it: 40 km
constexpr float kCloudOvercast = 0.7f;           // a cover past this lays a deck under the clouds (all of it at 1)

// The sky's own clouds, left out under the procedural ones: an object of a sky's .dlh (the file's name has "sky": the
// districts' _sky, _sky_wet, _sky_night, anc_sky, EAI_sky01) named for a cloud (donna_cloud*, cloud3; Unlit ABlend
// and Standard Alpha Blend quads), not the moon (donna_cloudmoon) nor the lightning (donna_cloudlightning1); the
// stars and the rainbow stay.
bool skyCloud(std::string_view file, std::string_view object);

// The view of the clouds: the renderer's cloud rows (c46, c47 of setLightDirection), the eye of world matrix `eye`,
// the light's direction (toward the ground) and colours (the atmosphere's: the sun's and the ambient, 0 .. 255), the
// atmosphere's zenith colour (the sky's own light) and whether it is a night one (the clouds' shade takes the sky's
// light by night: the user, 2026-10-29, a night atmosphere's ambient is brighter than a noon one's), the wind's way so
// far (world units), the set's cover, shadows and opacity; with `volume` the main view's volume.
render::CloudView cloudView(const std::array<std::array<float, 4>, 2>& rows, const Mat34& eye, const Vec4& light,
                            const Vec4& sunColour, const Vec4& ambient, uint32_t zenith, bool night, double travel,
                            const LightSet& set, bool volume);
// The reflections' view of the same clouds: the volume's fewer steps, none toward the sun.
render::CloudView reflectionView(const render::CloudView& view);

class Clouds {
public:
    explicit Clouds(render::IRenderDevice& device);
    ~Clouds();
    Clouds(const Clouds&) = delete;
    Clouds& operator=(const Clouds&) = delete;

    // The frame's clouds (a volume with `volume`) seen from `camera` under the renderer's light at its time, moved
    // on by `wind` (world units a second) since the last, the set's look (its cover eased), the map drawn; false when
    // it cannot be (the device's).
    bool update(render::Renderer3d& r, render::Camera& camera, const LightSet& set, bool volume, float wind);
    render::TextureId map() const { return m_map; }
    const render::CloudView& view() const { return m_view; }
    const render::CloudView& reflectedView() const { return m_reflected; }

private:
    render::IRenderDevice& m_device;
    render::TextureId m_map = 0;
    render::CloudView m_view;
    render::CloudView m_reflected;
    float m_cover = -1.0f;   // eased toward the set's
    float m_time = 0.0f;     // the world's time of the last update
    double m_travel = 0.0;   // the wind's way so far
    uint32_t m_frames = 0;   // the frames of update (the volume's march every other)
    bool m_marched = false;  // the volume's main view was marched (its screen for the dome)
};

}  // namespace oj::scene
