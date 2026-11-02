#pragma once
// The shaders of the fixed build's depth passes (the headlights' shadow maps, the ambient occlusion's view depth):
// JuicedConfigExt's Shared/Shaders/HeadlightShadow.hlsl, the plain pair and the alpha tested classes' cutout shaders
// (Renderer3d::ShadowShaders; the leaves of the trees cut out by their textures, the swaying trees placed as their
// shaders place them).

#include "Render/RenderDevice.h"
#include "Render/Renderer3d.h"

#include <cstdint>

namespace oj::scene {

class DepthShaders {
public:
    explicit DepthShaders(render::IRenderDevice& device);
    ~DepthShaders();
    DepthShaders(const DepthShaders&) = delete;
    DepthShaders& operator=(const DepthShaders&) = delete;

    // The plain pair was made (without the cutout shaders the alpha tested classes are left out).
    bool usable() const { return m_vertex != 0 && m_pixel != 0; }
    // The renderer's shadow shaders of a pass: these, the cull mode and the ground's flag given; with `ground` (the
    // view's depth of the ambient occlusion) the cut out pixels' depth negative (they occlude, none is occluded) and
    // the road's and the water's marked (Renderer3d::kGroundMark; kWaterMark the sea's and the rain's puddles': their
    // lights' reflections), the cars' too
    // (Renderer3d::kCarMark while drawingCars: no shadow of the street lamps on them).
    render::Renderer3d::ShadowShaders shaders(uint32_t cull, bool ground) const;

private:
    render::IRenderDevice& m_device;
    render::ShaderId m_vertex = 0;
    render::ShaderId m_pixel = 0;
    render::ShaderId m_cutoutVertex = 0;
    render::ShaderId m_treeVertex = 0;
    render::ShaderId m_treeSpriteVertex = 0;
    render::ShaderId m_cutoutPixel = 0;
    render::ShaderId m_cutoutMarkedPixel = 0;
    render::ShaderId m_groundPixel = 0;
    render::ShaderId m_puddlePixel = 0;
};

}  // namespace oj::scene
