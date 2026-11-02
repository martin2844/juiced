#include "Game/Scene/DepthShaders.h"

#include <windows.h>

#include <Shaders/HeadlightShadowShaders.h>

namespace oj::scene {

namespace {

render::ShaderId vertexShader(render::IRenderDevice& device, const BYTE* code) {
    return device.createVertexShader(reinterpret_cast<const uint32_t*>(code));
}

render::ShaderId pixelShader(render::IRenderDevice& device, const BYTE* code) {
    return device.createPixelShader(reinterpret_cast<const uint32_t*>(code));
}

}  // namespace

DepthShaders::DepthShaders(render::IRenderDevice& device) : m_device(device) {
    m_vertex = vertexShader(device, g_VsHeadlightShadow);
    m_pixel = pixelShader(device, g_PsHeadlightShadow);
    m_cutoutVertex = vertexShader(device, g_VsHeadlightShadowCutout);
    m_treeVertex = vertexShader(device, g_VsHeadlightShadowTree);
    m_treeSpriteVertex = vertexShader(device, g_VsHeadlightShadowTreeSprite);
    m_cutoutPixel = pixelShader(device, g_PsHeadlightShadowCutout);
    m_cutoutMarkedPixel = pixelShader(device, g_PsHeadlightShadowCutoutMarked);
    m_groundPixel = pixelShader(device, g_PsHeadlightShadowMarked);
    m_puddlePixel = pixelShader(device, g_PsHeadlightShadowPuddles);
}

DepthShaders::~DepthShaders() {
    for (const render::ShaderId id :
         {m_vertex, m_pixel, m_cutoutVertex, m_treeVertex, m_treeSpriteVertex, m_cutoutPixel, m_cutoutMarkedPixel,
          m_groundPixel, m_puddlePixel}) {
        if (id != 0) m_device.destroyShader(id);
    }
}

render::Renderer3d::ShadowShaders DepthShaders::shaders(uint32_t cull, bool ground) const {
    render::Renderer3d::ShadowShaders s{m_vertex, m_pixel, cull, ground};
    const render::ShaderId pixel = ground ? m_cutoutMarkedPixel : m_cutoutPixel;
    if (m_cutoutVertex != 0 && m_treeVertex != 0 && m_treeSpriteVertex != 0 && pixel != 0) {
        s.cutoutVertex = m_cutoutVertex;
        s.treeVertex = m_treeVertex;
        s.treeSpriteVertex = m_treeSpriteVertex;
        s.cutoutPixel = pixel;
    }
    if (ground) {
        s.groundPixel = m_groundPixel;
        s.puddlePixel = m_puddlePixel;
    }
    return s;
}

}  // namespace oj::scene
