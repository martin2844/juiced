#pragma once
// The fixed build's heat haze pass on Direct3D 9 (docs/fixes.md Added): JuicedConfigExt's HeatHaze, its shaders
// JuicedConfigExt's Shared/Shaders/HeatHaze.hlsl (vs_1_1, ps_2_0) as the plugin has them. The particles' side is
// Game/Scene/HeatHaze.

#include <windows.h>
#include <d3d9.h>

#include <cstdint>
#include <vector>

namespace oj::render::d3d9 {

class HeatHaze {
public:
    // strength: the registry's HeatHazeStrength / 100 (the shift's scale).
    explicit HeatHaze(float strength);
    // The strength changed in the game (the graphics options' ADVANCED page).
    void setStrength(float strength);
    ~HeatHaze();
    HeatHaze(const HeatHaze&) = delete;
    HeatHaze& operator=(const HeatHaze&) = delete;

    // A camera view's pass on `target` (the game's target 0, its depth `depth`) over `viewport`: the frame's viewport
    // copied, the particles' target bound (with the game's samples when it has them, so its depth tests them) and
    // cleared, the device's states kept in a state block. The view's width in pixels; 0 when it cannot be begun.
    uint32_t begin(IDirect3DDevice9* device, IDirect3DSurface9* target, IDirect3DSurface9* depth,
                   const D3DVIEWPORT9& viewport);
    // The particles' quads (24 bytes a vertex: place, colour, u v, the current declaration) adding their shift into
    // the particles' target with the distortion map on stage 0 (Vertex + Offset), the depth tested but not written;
    // the shift scaled by `boost`.
    void draw(IDirect3DDevice9* device, const void* vertices, uint32_t quads, float boost);
    // The copy drawn over the view moved by the shifts (Composite), the game's target, depth, viewport and states again.
    void end(IDirect3DDevice9* device);
    // Before a reset: the default pool's textures, targets and the state block go.
    void releaseTargets();

private:
    bool ensure(IDirect3DDevice9* device, const D3DSURFACE_DESC& target);

    float m_shiftScale = 0.0f;  // the shift at 1 cm, at the strength
    IDirect3DDevice9* m_device = nullptr;  // the one the shaders were made for
    UINT m_width = 0, m_height = 0;
    D3DFORMAT m_format = D3DFMT_UNKNOWN;
    D3DMULTISAMPLE_TYPE m_samples = D3DMULTISAMPLE_NONE;
    DWORD m_quality = 0;
    bool m_failed = false;
    IDirect3DTexture9* m_copy = nullptr;
    IDirect3DSurface9* m_copySurface = nullptr;
    IDirect3DTexture9* m_shiftTexture = nullptr;  // the particles' shifts, read by the composite
    IDirect3DSurface9* m_shiftSurface = nullptr;
    IDirect3DSurface9* m_shiftSamples = nullptr;  // with the game's samples, resolved into the shift; none without
    IDirect3DVertexShader9* m_vs = nullptr;
    IDirect3DPixelShader9* m_offset = nullptr;
    IDirect3DPixelShader9* m_composite = nullptr;
    IDirect3DStateBlock9* m_states = nullptr;
    std::vector<uint16_t> m_indices;  // 0 1 2 2 1 3 a quad

    // The pass in progress.
    bool m_running = false;
    IDirect3DSurface9* m_target = nullptr;
    IDirect3DSurface9* m_depth = nullptr;
    D3DVIEWPORT9 m_viewport{};
    RECT m_area{};
    bool m_depthTest = false;
    float m_vsParams[4] = {};
    float m_psParams[3][4] = {};
    int m_draws = 0;
    bool m_logged = false;
};

}  // namespace oj::render::d3d9
