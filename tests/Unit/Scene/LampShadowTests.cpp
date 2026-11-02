// Unit tests: WORLD LIGHT ULTRA's cars' shadows from the street lamps by night (Game/Scene/LampShadows.h,
// Render/Renderer3d.h): a lamp's camera looking down, the point the lamps are looked for about, the pass's view of a
// camera and of the lamps, the cars marked in the view's depth.

#include "Framework/Test.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Data/ExeTables.h"
#include "Game/Scene/AmbientOcclusion.h"
#include "Game/Scene/LampShadows.h"
#include "Render/Camera.h"
#include "Render/Materials.h"
#include "Render/Program.h"
#include "Render/Renderer3d.h"

#include <cmath>
#include <vector>

namespace {

using oj::test::check;

bool near(float a, float b) {
    return std::fabs(a - b) < 1e-3f;
}

// A device keeping the pixel shader set last and its constant c4 (the depth's marks).
struct MarkDevice : oj::test::NullRenderDevice {
    oj::render::ShaderId pixel = 0;
    oj::render::ShaderId vertex = 0;
    float mark[4] = {};
    float puddles[4] = {};
    uint32_t depthWrite = 1;
    void setRenderState(oj::render::RenderState state, uint32_t value) override {
        if (state == oj::render::RenderState::ZWriteEnable) depthWrite = value;
    }
    void setPixelShader(oj::render::ShaderId shader) override { pixel = shader; }
    void setVertexShader(oj::render::ShaderId shader) override { vertex = shader; }
    void setPixelShaderConstantF(uint32_t start, const float* data, uint32_t count) override {
        if ((start != 4 && start != 5) || count == 0) return;
        for (int i = 0; i < 4; ++i) (start == 4 ? mark : puddles)[i] = data[i];
    }
};

// The rain's water: its texture and noise, a tiling of 3 x 5.
class PuddleWater : public oj::render::Material {
public:
    explicit PuddleWater(oj::render::Renderer3d& r) : Material(oj::kWater, 0, r) {}
    void setParam(uint32_t, const void*) override {}
    bool setTexture(uint32_t, const oj::render::GameTexture*) override { return false; }
    void bind(uint32_t) override {}
    oj::render::Program* program() override { return nullptr; }
    bool puddles(const oj::render::GameTexture*& texture, const oj::render::GameTexture*& noise,
                 float tile[2]) const override {
        texture = &m_texture;
        noise = &m_noise;
        tile[0] = 3.0f;
        tile[1] = 5.0f;
        return true;
    }

private:
    oj::render::GameTexture m_texture;
    oj::render::GameTexture m_noise;
};

// A material of a class and nothing else.
class PlainMaterial : public oj::render::Material {
public:
    PlainMaterial(uint32_t classId, oj::render::Renderer3d& r) : Material(classId, 0, r) {}
    void setParam(uint32_t, const void*) override {}
    bool setTexture(uint32_t, const oj::render::GameTexture*) override { return false; }
    void bind(uint32_t) override {}
    oj::render::Program* program() override { return nullptr; }
};

// A lamp's camera as LampShadows makes it.
void lampCamera(oj::render::Camera& c, const oj::Vec4& place) {
    const auto fov = static_cast<float>(std::atan(oj::scene::kLampShadowTan) * 2.0);
    c.setFov(fov, fov, false);
    c.setNear(10.0f);
    c.setFar(oj::scene::kLampShadowRange);
    c.local = oj::scene::lampFrame(place);
    c.invalidate();
    c.update();
}

}  // namespace

// A lamp's camera at its place looking straight down, its up the world's z; the lamps looked for kLampShadowAhead
// along a camera's look.
OJ_UNIT_TEST(LampShadows, FrameAndCentre) {
    const oj::Mat34 f = oj::scene::lampFrame({100.0f, 900.0f, -50.0f, 0.0f});
    check(f[3] == 100.0f && f[7] == 900.0f && f[11] == -50.0f, "at the lamp's place");
    check(f[2] == 0.0f && f[6] == -1.0f && f[10] == 0.0f, "its look straight down");
    check(f[0] == 1.0f && f[4] == 0.0f && f[8] == 0.0f && f[1] == 0.0f && f[5] == 0.0f && f[9] == 1.0f,
          "its side the world's x, its up the world's z");
    oj::render::Camera camera;
    camera.resetMatrices();
    camera.setPosition(10.0f, 20.0f, 30.0f);
    camera.invalidate();
    const oj::Vec4 ahead = oj::scene::lampShadowCentre(camera.world());
    check(near(ahead[0], 10.0f) && near(ahead[1], 20.0f) && near(ahead[2], 30.0f + oj::scene::kLampShadowAhead),
          "the centre ahead of a camera looking along the z");
    oj::rotateY(camera.local, 1.5707964f);
    camera.invalidate();
    const oj::Vec4 turned = oj::scene::lampShadowCentre(camera.world());
    check(near(std::fabs(turned[0] - 10.0f), oj::scene::kLampShadowAhead) && near(turned[2], 30.0f),
          "turned a quarter: the centre along the x");
}

// The pass's view: the camera's half angles and rows, the depth's range, the strength, the bias and a map's texel; each
// lamp's place, look, range and its map's clip rows (the lamp's foot in the map's middle, the look's edge at 1); four
// lamps at most.
OJ_UNIT_TEST(LampShadows, View) {
    oj::render::Camera camera;
    camera.setFov(static_cast<float>(std::atan(0.75) * 2.0), static_cast<float>(std::atan(0.5) * 2.0), false);
    camera.resetMatrices();
    camera.setPosition(1.0f, 2.0f, 3.0f);
    camera.invalidate();
    camera.update();
    const oj::Vec4 lamp{0.0f, 900.0f, 500.0f, 0.5f};
    oj::render::Camera lampView;
    lampCamera(lampView, lamp);
    const oj::render::LampShadowView v = oj::scene::lampShadowView(camera, {lamp}, {&lampView});
    check(near(v.tanX, 0.75f) && near(v.tanY, 0.5f) && v.range == oj::scene::kOcclusionRange,
          "the half angles' tangents, the depth's range");
    const oj::Mat34& w = camera.world();
    check(v.camera[0][3] == w[3] && v.camera[1][3] == w[7] && v.camera[2][3] == w[11] && v.camera[0][0] == w[0] &&
              v.camera[2][2] == w[10],
          "the view to the world: the camera's rows");
    check(v.strength == oj::scene::kLampShadowStrength && v.bias == oj::scene::kLampShadowBias &&
              near(v.texel, 1.0f / static_cast<float>(oj::scene::kLampShadowSize)) && v.count == 1,
          "the strength, the bias, a map's texel, one lamp");
    const oj::render::LampShadowLamp& l = v.lamps[0];
    check(l.place[0] == 0.0f && l.place[1] == 900.0f && l.place[2] == 500.0f && l.look[1] == -1.0f &&
              l.range == oj::scene::kLampShadowRange && l.strength == 0.5f,
          "the lamp's place, its look down, its map's range, its shadow's share");
    const auto clip = [&](float x, float y, float z, size_t row) {
        return l.clip[row][0] * x + l.clip[row][1] * y + l.clip[row][2] * z + l.clip[row][3];
    };
    const float footW = clip(0.0f, 0.0f, 500.0f, 2);
    check(footW > 0.0f && near(clip(0.0f, 0.0f, 500.0f, 0) / footW, 0.0f) &&
              near(clip(0.0f, 0.0f, 500.0f, 1) / footW, 0.0f),
          "the lamp's foot in the middle of its map");
    const float side = oj::scene::kLampShadowTan * 900.0f;
    check(near(clip(side, 0.0f, 500.0f, 0) / clip(side, 0.0f, 500.0f, 2), 1.0f) &&
              near(clip(0.0f, 0.0f, 500.0f + side, 1) / clip(0.0f, 0.0f, 500.0f + side, 2), 1.0f),
          "the look's edge at the map's edge: along the world's x across, along its z up");
    std::vector<oj::render::Camera> views(5);
    std::vector<oj::Vec4> lamps;
    std::vector<oj::render::Camera*> cameras;
    for (size_t i = 0; i < views.size(); ++i) {
        lamps.push_back({100.0f * static_cast<float>(i), 900.0f, 0.0f, 0.0f});
        lampCamera(views[i], lamps.back());
        cameras.push_back(&views[i]);
    }
    check(oj::scene::lampShadowView(camera, lamps, cameras).count == 4, "four lamps at most");
}

// The view's depth marks the cars drawn (drawingCars) depth + kCarMark, their cut out parts too; the road keeps its
// mark, a wall + 0 (the marked shader's depth at most the range's end: a far building read as no road, the user
// 2026-10-28); the headlights' maps (no ground pixel shader) mark nothing.
OJ_UNIT_TEST(LampShadows, CarsMarked) {
    using oj::render::Renderer3d;
    check(Renderer3d::kCarMark - Renderer3d::kGroundMark > 3.5f, "the cars' mark apart from the road's and its depth");
    MarkDevice device;
    oj::render::ProgramContext programs(device, 0);
    Renderer3d r(device, programs);
    r.shadowShaders.vertex = 1;
    r.shadowShaders.pixel = 2;
    r.shadowShaders.groundPixel = 3;
    r.shadowShaders.cutoutVertex = 4;
    r.shadowShaders.cutoutPixel = 5;
    r.shadowShaders.ground = true;
    PlainMaterial road(oj::kTarmac, r);
    PlainMaterial sheet(oj::kWater, r);
    PlainMaterial sea(oj::kSea, r);
    PuddleWater water(r);
    PlainMaterial wall(oj::kBuilding, r);
    PlainMaterial body(oj::kCarBody, r);
    PlainMaterial decal(oj::kStandardAlphaTest, r);
    r.beginMaterialPass(road, 0);
    check(device.pixel == 3 && device.mark[1] == Renderer3d::kGroundMark, "the road marked");
    r.beginMaterialPass(sea, 0);
    check(device.pixel == 3 && device.mark[1] == Renderer3d::kWaterMark, "the sea marked as the water");
    r.shadowShaders.puddlePixel = 6;
    r.beginMaterialPass(water, 0);
    check(device.pixel == 3 && device.mark[1] == Renderer3d::kGroundMark && device.depthWrite == 1,
          "the rain's water in the marked depth: no puddles (TrackScene draws none of it there)");
    using GroundPass = Renderer3d::GroundPass;
    r.groundPass = GroundPass::Puddles;
    r.beginMaterialPass(water, 0);
    check(device.pixel == 6 && device.vertex == 4 && device.depthWrite == 0 && device.puddles[0] == 3.0f &&
              device.puddles[1] == 5.0f && device.puddles[2] == Renderer3d::kPuddleMask,
          "the rain's puddles cut out by their mask (the tiling, the least of it), no depth written");
    check(r.shadowPassDraws(oj::kWater) && !r.shadowPassDraws(oj::kTarmac) && !r.shadowPassDraws(oj::kCarBody),
          "then the rain's water alone drawn");
    r.groundPass = GroundPass::Marked;
    r.groundMark = Renderer3d::kWaterMark;
    r.beginMaterialPass(road, 0);
    check(device.pixel == 3 && device.mark[1] == Renderer3d::kWaterMark && device.depthWrite == 1,
          "the road drawn again under a puddle: marked as the water");
    r.groundMark = 0.0f;
    r.beginMaterialPass(road, 0);
    check(device.mark[1] == 0.0f, "on sand and grass: no mark");
    check(r.shadowPassDraws(oj::kTarmac) && r.shadowPassDraws(oj::kSea) && !r.shadowPassDraws(oj::kWater) &&
              !r.shadowPassDraws(oj::kBuilding) && !r.shadowPassDraws(oj::kCarBody),
          "then the ground alone drawn, not the rain's water");
    r.groundPass = GroundPass::None;
    r.shadowShaders.puddlePixel = 0;
    r.beginMaterialPass(sheet, 0);
    check(device.pixel == 3 && device.mark[1] == Renderer3d::kGroundMark, "water without its textures: the road's");
    r.beginMaterialPass(wall, 0);
    check(device.pixel == 3 && device.mark[1] == 0.0f, "a wall not, its depth through the marked shader");
    r.drawingCars = true;
    r.beginMaterialPass(body, 0);
    check(device.pixel == 3 && device.mark[1] == Renderer3d::kCarMark, "a car's body marked");
    r.beginMaterialPass(decal, 0);
    check(device.pixel == 3 && device.mark[1] == Renderer3d::kCarMark, "a car's cut out part marked whole");
    r.shadowShaders.groundPixel = 0;
    r.beginMaterialPass(body, 0);
    check(device.pixel == 2, "the headlights' maps: no mark");
    r.drawingCars = false;
}
