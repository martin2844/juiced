#pragma once
#include "Render/RenderDevice.h"
#include "Render/ShaderReplacements.h"
#include <functional>
#include <map>

namespace oj::render {
// Shared native implementation of upstream's post-processing passes. All
// work uses the engine API and the original compiled shader programs.
class NativeEffects;
class NativeRenderDevice : public IRenderDevice {
public:
    struct Target { TextureId id; uint32_t face; Viewport viewport; };
    struct SavedState {
        std::function<void()> restore;
        SavedState(std::function<void()> fn) : restore(std::move(fn)) {}
        SavedState(const SavedState&) = delete;
        SavedState(SavedState&& other) noexcept : restore(std::move(other.restore)) { other.restore = {}; }
        ~SavedState() { if (restore) restore(); }
    };
    NativeRenderDevice();
    ~NativeRenderDevice() override;
    virtual Target nativeTarget() const = 0;
    virtual SavedState saveNativeState() = 0;
    virtual bool copyNativeTarget(TextureId from, uint32_t face, const Viewport& region, TextureId to) = 0;
    enum class TargetFormat { Colour, Scalar, Hdr };
    virtual TextureId createNativeTarget(uint32_t width, uint32_t height, TargetFormat format) = 0;
    virtual TextureId createNativeScalarTexture(uint32_t width, uint32_t height, const float* values) = 0;
    virtual TextureId createNativeVolume(uint32_t width, uint32_t height, uint32_t depth, const float* rgba) = 0;
    virtual bool copyNativeDepth(TextureId from, TextureId to) = 0;
    TextureId createFloatTarget(uint32_t width, uint32_t height) override;
    void setShaderFeatures(unsigned features,float worldLightStrength) override;
    void setLightCase(LightCase lightCase) override;
    ShaderId resolveNativePixelShader(ShaderId id);
    uint32_t nativeShaderReplacements() const { return m_replacedShaders; }
    bool ambientOcclusion(TextureId depth, const AmbientOcclusionView& view) override;
    bool lightReflections(TextureId depth, const LightReflectionView& view) override;
    bool lampShadows(TextureId depth, const std::array<TextureId, 4>& maps, const LampShadowView& view) override;
    bool volumetricLight(TextureId depth, const VolumetricLightView& view) override;
    bool pointLights(TextureId depth, const PointLightView& view) override;
    bool cloudMap(TextureId map, const CloudView& view) override;
    bool cloudView(const CloudView& view) override;
    bool cloudSky(uint32_t primitives, const CloudView& view) override;
    bool dilateDepth(TextureId target, const Viewport& region, float gap) override;
    void setAntialiasing(uint32_t msaa, bool smaa) override;
    void antialias() override;
    void setBrightness(float brightness) override;
    uint32_t beginHeatHaze() override;
    void drawHeatHaze(const void* vertices, uint32_t quads, float boost) override;
    void endHeatHaze() override;
    void setHeatHazeStrength(float strength) override;
    bool captureAmbientOcclusion(const std::wstring& file) override;
    bool captureLightReflections(const std::wstring& file) override;
    bool capturePointLights(const std::wstring& file) override;
    bool captureVolumetricLight(const std::wstring& file) override;
    bool captureLampShadows(const std::wstring& file) override;
protected:
    void configureNativeShaders(const DeviceDesc& desc);
    std::vector<uint32_t> nativeShaderCode(const uint32_t* code,bool vertex);
    void rememberNativeShader(ShaderId id,const uint32_t* code,bool vertex);
    void forgetNativeShader(ShaderId id);
    void finishNativeFrame();
    void shutdownNativeEffects();
    std::unique_ptr<NativeEffects> m_nativeEffects;
private:
    ShaderReplacements m_replacements;
    LightCase m_lightCase=kBaseLightCase;
    bool m_creatingVariant=false;
    uint32_t m_replacedShaders=0;
    std::map<ShaderId,std::vector<uint32_t>> m_originalPixels;
    std::map<std::pair<ShaderId,LightCase>,ShaderId> m_pixelVariants;
};
}
