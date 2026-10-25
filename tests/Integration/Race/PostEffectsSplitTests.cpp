// Integration tests: the fixed build's split screen in the post effects (Game/Scene/PostEffectsSplit.cpp), through a
// device of the tests that records the targets made and taken and the viewports.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Saves/GraphicsConfig.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"
#include "Render/Camera.h"

#include <map>
#include <utility>
#include <vector>

namespace {

using oj::test::check;

struct RecordingDevice : oj::test::NullRenderDevice {
    std::map<oj::render::TextureId, std::pair<uint32_t, uint32_t>> targets;  // made: their sizes
    oj::render::TextureId taken = 0;
    std::vector<oj::render::TextureId> takenList;
    std::vector<oj::render::Viewport> backViewports;  // set while the back buffer is taken
    oj::render::TextureId createRenderTarget(uint32_t width, uint32_t height, bool depth) override {
        const oj::render::TextureId id = NullRenderDevice::createRenderTarget(width, height, depth);
        targets[id] = {width, height};
        return id;
    }
    void setRenderTarget(oj::render::TextureId target, uint32_t) override {
        taken = target;
        takenList.push_back(target);
    }
    void setViewport(const oj::render::Viewport& port) override {
        if (taken == 0) backViewports.push_back(port);
    }
};

}  // namespace

// Two views of a split screen (640 x 480: the upper and the lower halves) with the motion blur: each view draws into
// its own targets of its size (two each, swapped every frame, not the other view's), and the back buffer takes the
// view's place as its viewport for the copy (the stock shared the screen's pair and drew at the screen's places).
OJ_INTEGRATION_TEST(PostEffects, SplitViewsOwnTargets) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    oj::GraphicsConfig& c = oj::graphicsConfig();
    const oj::GraphicsConfig kept = c;
    c.noShaders = 0;
    c.shaders = 1;
    c.sceneWidth = 0x400;
    c.sceneHeight = 0x200;
    c.motionBlur = 1;
    c.glow = 0;
    c.depthOfField = 0;
    {
        RecordingDevice device;
        oj::scene::Engine engine(device, shared.tables, 3, false);
        oj::scene::PostEffects& post = engine.postEffects();
        oj::render::Camera camera;
        camera.blurEdge = 0.5f;
        oj::scene::PostEffects::View upper;
        oj::scene::PostEffects::View lower;
        upper.glow = lower.glow = false;
        std::vector<oj::render::TextureId> drawn;  // the target each view's begin took, frame by frame
        bool lowerPlaced = true;
        for (int frame = 0; frame < 2; ++frame) {
            for (int v = 0; v < 2; ++v) {
                oj::scene::PostEffects::View& view = v == 0 ? upper : lower;
                const uint32_t y = v == 0 ? 0u : 240u;
                device.backViewports.clear();
                post.begin(camera, view, 0, y, 640, 240, false, false);
                drawn.push_back(device.taken);
                post.end(camera, view, 0.0f, false);
                if (v == 1) {
                    bool placed = false;
                    for (const oj::render::Viewport& p : device.backViewports) {
                        placed = placed || (p.x == 0 && p.y == 240 && p.width == 640 && p.height == 240);
                    }
                    lowerPlaced = lowerPlaced && placed;
                }
            }
        }
        size_t halves = 0;
        for (const auto& [id, size] : device.targets) halves += size.first == 640 && size.second == 240 ? 1 : 0;
        const auto half = [&](oj::render::TextureId id) {
            const auto it = device.targets.find(id);
            return it != device.targets.end() && it->second.first == 640 && it->second.second == 240;
        };
        // drawn: upper 0, lower 0, upper 1, lower 1: four targets of the view's size, each view's two its own.
        const bool own = drawn.size() == 4 && half(drawn[0]) && half(drawn[1]) && half(drawn[2]) && half(drawn[3]) &&
                         drawn[0] != drawn[1] && drawn[2] != drawn[3] && drawn[0] != drawn[2] && drawn[1] != drawn[3] &&
                         drawn[0] != drawn[3] && drawn[1] != drawn[2];
        check(halves == 4 && own, "split screen (fixed): each view its own two targets of its size");
        check(lowerPlaced, "split screen (fixed): the lower view's copy at its place on the back buffer");
        // The next race's players are made again, their views new objects at the same places: the same pairs, no
        // new targets (keyed by the view's address the pairs grew by two a race).
        const size_t made = device.targets.size();
        oj::scene::PostEffects::View upperAgain;
        oj::scene::PostEffects::View lowerAgain;
        upperAgain.glow = lowerAgain.glow = false;
        post.begin(camera, upperAgain, 0, 0, 640, 240, false, false);
        const bool upperSame = device.taken == drawn[0] || device.taken == drawn[2];
        post.end(camera, upperAgain, 0.0f, false);
        post.begin(camera, lowerAgain, 0, 240, 640, 240, false, false);
        const bool lowerSame = device.taken == drawn[1] || device.taken == drawn[3];
        post.end(camera, lowerAgain, 0.0f, false);
        check(device.targets.size() == made && upperSame && lowerSame,
              "split screen (fixed): the next race's views take the pairs of their places, none made");
        // The whole screen's view goes back to the screen's pair and lets the views' go.
        oj::scene::PostEffects::View whole;
        whole.glow = false;
        post.begin(camera, whole, 0, 0, 640, 480, false, false);
        const auto it = device.targets.find(device.taken);
        check(it != device.targets.end() && it->second.first == 640 && it->second.second == 480,
              "the whole screen's view: the screen's targets");
        post.end(camera, whole, 0.0f, false);
    }
    c = kept;
}
