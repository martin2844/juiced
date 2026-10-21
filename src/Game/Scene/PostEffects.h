#pragma once
// The post effects of the race's 3D view (docs/spec/12-rendering.md, "The post effects"): with an effect asked for the
// view's scene goes into a render target (0x00516880), and the view's end (0x00516D20) draws the motion blur, the
// depth of field, the copy into the back buffer with the exposure, the glow and the director's cut over it, then
// the frame's target becomes the previous one.

#include "Render/Materials.h"
#include "Render/Renderer3d.h"

#include <array>
#include <functional>
#include <cstdint>
#include <memory>
#include <vector>

namespace oj::render {
class Camera;
class Program;
}  // namespace oj::render

namespace oj::scene {

class Engine;

// 0x00401F9D (the camera leaving the car's node, 0x00401F90): the exposure 1 and its speed 0.
void resetExposure();

// A material of the class TexShape2D (0x10002, 0x0052CE30): Material_TexShape2D, a texture, parameters 0 (added) and
// 1 (opaque).
std::unique_ptr<render::Material> makeTexShape2d(Engine& engine);

class PostEffects {
public:
    // What the view keeps (the view object of 0x00516210): its glow (+0x20, 1; 0 for a split screen race,
    // 0x0041DF28), the glow's first frame (+0x18, 1 until the view's first end) and its clear colour (+0x0C).
    struct View {
        bool glow = true;
        bool glowFirst = true;
        uint32_t clear = 0xFF000000u;
    };

    // The renderer's start (0x005146D6, 0x00514745, 0x005142B3, 0x005142D8): with a size for them (the settings'
    // +0x34, 0 without shaders) the frame's two scene targets, the effects' object (0x0051FA00) and the exposure's
    // measure (0x00512210).
    explicit PostEffects(Engine& engine);
    ~PostEffects();

    // 0x00516880 for the race's view (flags 0x0E): the effects asked for put the scene into the frame's target
    // (+0x200, 0x00515B70) and load the exposure's program (0x00521170); the scene's target or the back buffer
    // then takes the view's viewport (the target's size), its clear (colour and depth by the target) and the 2D
    // constants c22, c23 (the view's size).
    void begin(const render::Camera& camera, View& view, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
               bool cutBlend, bool alwaysDepthOfField);
    // 0x00516D20: the effects into the back buffer (cutAmount the director's transition, [0x0075ABC4]), the targets
    // swapped, the interface's states.
    // `smears`: with the camera's screen smears (+0x234) the effects manager's SMEAR emitters (0x005224E0), given the
    // view's corners.
    void end(const render::Camera& camera, View& view, float cutAmount, bool alwaysDepthOfField,
             const std::function<void(float, float, float, float)>& smears = {});

    // The target taken last ([0x0075F168]): a texture drawn into taken through it (0x0051F660: not again when it is
    // the one taken last; taking one empties the texture stages 0 .. 3), or forgotten when a cube's face is set past
    // it (0x005C9120 sets it to -1).
    void takeTarget(render::TextureId target);
    void forgetTarget() { m_taken = -1; }
    // 0x00517390 in the world's main pass after the wet road's reflection: the view's target taken again (the
    // scene's target with the effects, its whole size; else the back buffer with the view's viewport).
    void resume();

private:
    struct Target {
        render::GameTexture texture;  // its texture (the RT object's +0x18) with linear filters, clamped (0x005C9730)
        uint32_t width = 0;
        uint32_t height = 0;
    };

    bool shaders() const;           // no [0x0079E664] and [0x0079E66C]
    bool motionBlurOn() const;      // 0x005A3370: the settings' +0x54 with shaders
    bool depthOfFieldOn(const render::Camera& camera, bool always) const;
    void makeTarget(Target& target, uint32_t width, uint32_t height);
    // 0x0051F660 and the like: the target taken unless it is the one taken last ([0x0075F168]; 0 the back buffer);
    // taking one empties the texture stages 0 .. 3 (0x0051F3E0).
    void take(const Target* target);
    // The engine's state of the 2D draws (0x005240E0's start, the 0x0051BDA0 of the open program, [0x00719540]
    // 0x10009, [0x0075F16C] 0, the samplers' cache forgotten).
    void freshState();
    // 0x0051C5E0: the vertices written into the buffer (+0x00) from its start.
    void writeVertices(const void* data, uint32_t bytes);

    void motionBlur(float edge, float middle, float x0, float y0, float x1, float y1);  // 0x0051FD50
    void copyToBack(float width, float height, bool glow);                              // 0x005211B0
    // 0x00520E30 → 0x00520AB0: the texture as a quad into the back buffer with TexShape2D (0x10002, opaque) and,
    // with a program, that program's pass over it.
    void drawToBack(const render::GameTexture& texture, float x, float y, float width, float height,
                    bool alphaOnly, render::Program* program);
    void measureExposure(float width, float height);  // 0x00520900, 0x00512290
    void cutBlend(float amount, float x0, float y0, float x1, float y1);  // 0x00520410
    void depthOfField(const render::Camera& camera, float x0, float y0);  // 0x005214C0
    void glow(float x0, float y0, float x1, float y1, bool first);      // 0x00521C80
    // 0x00520EF0: 2DBlur (0x10005) with -2 and the texture, its pass, the engine's c52 .. c55 0.
    void blurPass(const render::GameTexture& texture);
    // 0x00520FC0: a quad of transformed vertices (XYZRHW, a colour, four u v with their offsets of 1 / 256) from the
    // second buffer (+0x04) at the depth z.
    void blurQuad(uint32_t colour, float x0, float y0, float x1, float y1, float z);

    Engine& m_engine;
    render::IRenderDevice& m_device;
    render::Renderer3d& m_r;
    // The renderer's +0x1F8 (the frame's) and +0x1FC (the previous frame's), +0x200 (the scene drawn into the
    // frame's), +0x1E8 (the 2D declaration: place float2, colour, uv float2).
    Target m_targets[2];
    int m_frame = 0;
    bool m_drawing = false;
    render::DeclarationId m_declaration2d = 0;
    // The effects' object (+0x224): the vertex buffer (+0x00), the samplers (+0x08 linear clamped, +0x0C linear
    // wrapped, point between levels), the blur targets (+0x14 settings +0x24 x +0x28, +0x1C +0x1C x +0x20) cleared
    // white with no alpha, the exposure's program (+0x20).
    render::BufferId m_buffer = 0;
    render::BufferId m_buffer2 = 0;
    render::SamplerParams m_clamped{2, 2, 1, 3, 3, 0.0f};
    render::SamplerParams m_wrapped{2, 2, 1, 1, 1, 0.0f};
    Target m_blurSmall;
    Target m_blurWide;
    render::Program* m_exposureProgram = nullptr;
    bool m_exposureLoaded = false;
    // The materials of the 2D classes (the material manager's +4 by class id).
    std::unique_ptr<render::Material> m_texShape;    // 0x10002
    std::unique_ptr<render::Material> m_motionBlur;  // 0x10003
    std::unique_ptr<render::Material> m_blur;        // 0x10005
    // The exposure's measure ([0x00722A88]): its target (+0x04, 128 x 64), its pixel shader (+0x08).
    render::TextureId m_measure = 0;
    render::ShaderId m_luminance = 0;
    // [0x0075F168]: the target taken last; -1 none known (0x00515B70).
    int64_t m_taken = -1;
    uint32_t m_viewWidth = 0, m_viewHeight = 0;
    float m_x0 = 0.0f, m_y0 = 0.0f, m_x1 = 0.0f, m_y1 = 0.0f;
    float m_c22[4] = {};  // [0x0079BCF0]
    float m_c23[4] = {};  // [0x0079BCE0]
};

}  // namespace oj::scene
