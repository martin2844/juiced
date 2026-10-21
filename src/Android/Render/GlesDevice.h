#pragma once
#include "Render/RenderDevice.h"
namespace oj::render {
class GlesDevice final : public IRenderDevice {
  public:
    explicit GlesDevice(const DeviceDesc &);
    ~GlesDevice() override;
    Backend backend() const override;
    std::string adapterName() const override;
    std::string description() const override;
    bool beginFrame(const Color &clear) override;
    void endFrame() override;
    void resize(uint32_t width, uint32_t height) override;
    void drawDebugTriangles(const DebugVertex *vertices, uint32_t count) override;
    uint32_t width() const override;
    uint32_t height() const override;
    uint32_t vertexShaderVersion() const override;
    uint32_t pixelShaderVersion() const override;
    AdapterInfo adapterInfo() const override;
    uint32_t backBufferFormat() const override;
    uint32_t sourceBlendCaps() const override;
    bool fullScreenGamma() const override;
    void setGammaRamp(uint32_t flags, const uint16_t *red, const uint16_t *green, const uint16_t *blue) override;
    TextureId createTexture(const TextureData &data) override;
    TextureId createCubeTexture(const TextureData &data) override;
    void destroyTexture(TextureId texture) override;
    TextureId createRenderTarget(uint32_t width, uint32_t height, bool cube) override;
    void setRenderTarget(TextureId target, uint32_t face) override;
    uint32_t targetWidth(TextureId target) const override;
    uint32_t targetHeight(TextureId target) const override;
    uint32_t resets() const override;
    bool readTarget(TextureId target, std::vector<uint8_t> &pixels, uint32_t *pitch) override;
    uint32_t displayFormat() const override;
    uint32_t textureCaps() const override;
    bool textureFormatOk(uint32_t display, uint32_t format) const override;
    TextureId createDynamicTexture(uint32_t width, uint32_t height, uint32_t format) override;
    bool lockTexture(TextureId texture, void **bits, int32_t *pitch) override;
    void unlockTexture(TextureId texture) override;
    void drawUp(uint32_t primitive, uint32_t primitiveCount, const void *vertices, uint32_t stride) override;
    void drawQuad(const QuadVertex (&corners)[4], TextureId texture, Blend blend, bool samplerSet = false) override;
    void drawLine(float x0, float y0, float x1, float y1, uint32_t argb) override;
    void setClip(const ClipRect *rect) override;
    bool capture(const std::wstring &file) override;
    ShaderId createVertexShader(const uint32_t *code) override;
    ShaderId createPixelShader(const uint32_t *code) override;
    DeclarationId createDeclaration(const VertexElement *elements) override;
    BufferId createVertexBuffer(const void *data, uint32_t bytes) override;
    BufferId createIndexBuffer(const uint16_t *indices, uint32_t count) override;
    void destroyBuffer(BufferId buffer) override;
    void updateVertexBuffer(BufferId buffer, const void *data, uint32_t bytes) override;
    bool occlusionQueries() const override;
    QueryId createOcclusionQuery() override;
    void issueQuery(QueryId query, bool begin) override;
    int queryResult(QueryId query, bool flush, uint32_t *pixels) override;
    void setRenderState(RenderState state, uint32_t value) override;
    void setTextureStageState(uint32_t stage, TextureStageState type, uint32_t value) override;
    void setSamplerState(uint32_t sampler, SamplerState type, uint32_t value) override;
    void setTexture(uint32_t sampler, TextureId texture) override;
    void setTransform(TransformState state, const float *matrix) override;
    void setMaterial(const FixedMaterial &material) override;
    void setLight(uint32_t index, const FixedLight &light) override;
    void lightEnable(uint32_t index, bool enable) override;
    void setFvf(uint32_t fvf) override;
    void setNPatchMode(float segments) override;
    void setVertexShader(ShaderId shader) override;
    void setPixelShader(ShaderId shader) override;
    void setVertexShaderConstantF(uint32_t start, const float *data, uint32_t count) override;
    void setVertexShaderConstantI(uint32_t start, const int32_t *data, uint32_t count) override;
    void setVertexShaderConstantB(uint32_t start, const int32_t *data, uint32_t count) override;
    void setPixelShaderConstantF(uint32_t start, const float *data, uint32_t count) override;
    void setPixelShaderConstantI(uint32_t start, const int32_t *data, uint32_t count) override;
    void setPixelShaderConstantB(uint32_t start, const int32_t *data, uint32_t count) override;
    void setDeclaration(DeclarationId declaration) override;
    void setStream(BufferId buffer, uint32_t stride) override;
    void setIndices(BufferId buffer) override;
    void drawIndexed(uint32_t primitive, uint32_t vertexCount, uint32_t startIndex, uint32_t primitiveCount) override;
    void draw(uint32_t primitive, uint32_t startVertex, uint32_t primitiveCount) override;
    void setViewport(const Viewport &viewport) override;
    void clear(uint32_t flags, uint32_t argb, float z, uint32_t stencil) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> p;
};
} // namespace oj::render
