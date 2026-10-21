#pragma once
// A render device that draws nothing, for the tests of what runs over the interface (the windows): textures, targets
// and buffers get numbers of their own, a back buffer of 640 x 480.

#include "Render/RenderDevice.h"

#include <cstdint>
#include <string>
#include <vector>

namespace oj::test {

class NullRenderDevice : public render::IRenderDevice {
public:
    render::Backend backend() const override { return render::Backend::D3D9; }
    std::string adapterName() const override { return "null"; }
    std::string description() const override { return "null"; }
    bool beginFrame() override { return true; }
    void endFrame() override {}
    void resize(uint32_t, uint32_t) override {}
    void drawDebugTriangles(const render::DebugVertex*, uint32_t) override {}
    uint32_t width() const override { return 640; }
    uint32_t height() const override { return 480; }
    uint32_t vertexShaderVersion() const override { return 0xFFFE0300; }
    uint32_t pixelShaderVersion() const override { return 0xFFFF0300; }
    render::AdapterInfo adapterInfo() const override { return {}; }
    uint32_t backBufferFormat() const override { return 22; }  // D3DFMT_X8R8G8B8
    uint32_t sourceBlendCaps() const override { return 0; }
    bool fullScreenGamma() const override { return false; }
    void setGammaRamp(uint32_t, const uint16_t*, const uint16_t*, const uint16_t*) override {}

    render::TextureId createTexture(const render::TextureData&) override { return ++m_next; }
    render::TextureId createCubeTexture(const render::TextureData&) override { return ++m_next; }
    void destroyTexture(render::TextureId) override {}
    render::TextureId createRenderTarget(uint32_t, uint32_t, bool) override { return ++m_next; }
    void setRenderTarget(render::TextureId, uint32_t) override {}
    uint32_t targetWidth(render::TextureId) const override { return 640; }
    uint32_t targetHeight(render::TextureId) const override { return 480; }
    uint32_t resets() const override { return 0; }
    bool readTarget(render::TextureId, std::vector<uint8_t>&, uint32_t*) override { return false; }
    uint32_t displayFormat() const override { return 22; }
    uint32_t textureCaps() const override { return 0; }
    bool textureFormatOk(uint32_t, uint32_t) const override { return true; }
    render::TextureId createDynamicTexture(uint32_t, uint32_t, uint32_t) override { return ++m_next; }
    bool lockTexture(render::TextureId, void**, int32_t*) override { return false; }
    void unlockTexture(render::TextureId) override {}
    void drawUp(uint32_t, uint32_t, const void*, uint32_t) override {}
    void drawQuad(const render::QuadVertex (&)[4], render::TextureId, render::Blend, bool) override {}
    void drawLine(float, float, float, float, uint32_t) override {}
    void setClip(const render::ClipRect*) override {}
    bool capture(const std::wstring&) override { return false; }

    render::ShaderId createVertexShader(const uint32_t*) override { return ++m_next; }
    render::ShaderId createPixelShader(const uint32_t*) override { return ++m_next; }
    render::DeclarationId createDeclaration(const render::VertexElement*) override { return ++m_next; }
    render::BufferId createVertexBuffer(const void*, uint32_t) override { return ++m_next; }
    render::BufferId createIndexBuffer(const uint16_t*, uint32_t) override { return ++m_next; }
    void destroyBuffer(render::BufferId) override {}
    void updateVertexBuffer(render::BufferId, const void*, uint32_t) override {}
    bool occlusionQueries() const override { return false; }
    render::QueryId createOcclusionQuery() override { return 0; }
    void issueQuery(render::QueryId, bool) override {}
    int queryResult(render::QueryId, bool, uint32_t*) override { return -1; }

    void setRenderState(render::RenderState, uint32_t) override {}
    void setTextureStageState(uint32_t, render::TextureStageState, uint32_t) override {}
    void setSamplerState(uint32_t, render::SamplerState, uint32_t) override {}
    void setTexture(uint32_t, render::TextureId) override {}
    void setTransform(render::TransformState, const float*) override {}
    void setMaterial(const render::FixedMaterial&) override {}
    void setLight(uint32_t, const render::FixedLight&) override {}
    void lightEnable(uint32_t, bool) override {}
    void setFvf(uint32_t) override {}
    void setNPatchMode(float) override {}
    void setVertexShader(render::ShaderId) override {}
    void setPixelShader(render::ShaderId) override {}
    void setVertexShaderConstantF(uint32_t, const float*, uint32_t) override {}
    void setVertexShaderConstantI(uint32_t, const int32_t*, uint32_t) override {}
    void setVertexShaderConstantB(uint32_t, const int32_t*, uint32_t) override {}
    void setPixelShaderConstantF(uint32_t, const float*, uint32_t) override {}
    void setPixelShaderConstantI(uint32_t, const int32_t*, uint32_t) override {}
    void setPixelShaderConstantB(uint32_t, const int32_t*, uint32_t) override {}
    void setDeclaration(render::DeclarationId) override {}
    void setStream(render::BufferId, uint32_t) override {}
    void setIndices(render::BufferId) override {}
    void drawIndexed(uint32_t, uint32_t, uint32_t, uint32_t) override {}
    void draw(uint32_t, uint32_t, uint32_t) override {}
    void setViewport(const render::Viewport&) override {}
    void clear(uint32_t, uint32_t, float, uint32_t) override {}

private:
    uint32_t m_next = 0;
};

}  // namespace oj::test
