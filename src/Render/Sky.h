#pragma once
// The atmosphere of a 3D world at draw time and its sky dome (docs/spec/13-atmosphere.md).

#include "Core/Tracks/Atmosphere.h"
#include "Render/Camera.h"
#include "Render/Materials.h"
#include "Render/Renderer3d.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace oj::render {

// The atmospheres of a world (0x74 bytes, 0x005337A0): those of the file, the one in use, the one it changes to and
// how far (the conditions of a race change over time).
struct AtmosphereSet {
    std::vector<Atmosphere> list;         // +0x08
    Atmosphere* current = nullptr;        // +0x58
    const Atmosphere* next = nullptr;     // +0x5C
    float blend = 0.0f;                   // +0x64
};

// 0x00531B60: each channel a × t + b × (1 − t), cut toward zero.
uint32_t mixColour(uint32_t a, uint32_t b, float t);

// 0x00533B80: the atmosphere's lights, fog and values for the frame; the fog colour goes from the back to the front
// colour with the camera's heading against the sky angle.
void applyAtmosphere(Renderer3d& renderer, const AtmosphereSet& atmosphere, Camera& camera);

// The light 1 direction of the atmospheres (0x005341F0), blended when they change.
Vec4 atmosphereLightDirection(const AtmosphereSet& atmosphere);

// The sky dome (0x50 bytes, 0x0054D030, vtable 0x006FA038): 8 bands of a half sphere of radius 5000 as one triangle
// strip, its colours from the horizon through the sky to the zenith, drawn around the camera with the Sky material.
class SkyDome {
public:
    struct Colours {
        uint32_t horizonFront, horizonBack, skyFront, skyBack, zenith;
        float angle, heightFactor, luminosity, heightManual;
    };
    SkyDome(Renderer3d& renderer, std::unique_ptr<Material> material, const Colours& colours);
    ~SkyDome();
    SkyDome(const SkyDome&) = delete;
    SkyDome& operator=(const SkyDome&) = delete;

    // 0x0054D510: the main pass, the dome's centre the camera's position lowered by the height.
    void draw(Camera& camera);
    // 0x0054D6C0 (slot 2): into the wet road's reflection, turned upside down about the camera's position 50 up (its
    // height of the reflection not read, nor the dome's height); the model through the world-to-clip matrix as the
    // object draw makes it (0x00511620).
    void drawReflected(Camera& camera);

    struct Vertex {
        float x, y, z;
        uint32_t argb;
    };
    const std::vector<Vertex>& vertices() const { return m_vertices; }

private:
    Renderer3d& m_r;
    std::unique_ptr<Material> m_material;  // +0x34
    DeclarationId m_declaration = 0;       // +0x38
    BufferId m_buffer = 0;                 // +0x4C
    std::vector<Vertex> m_vertices;        // +0x3C
    float m_luminosity;                    // +0x2C
    float m_heightManual;                  // +0x30
};

// The colours of the dome from an atmosphere (0x005315A0), or without one: horizon and sky 255 247 255 front,
// 218 227 233 back, zenith 55 126 190, angle 0, height factor 0, luminosity 0.25, height -50.
SkyDome::Colours skyColours(const AtmosphereSet* atmosphere);

}  // namespace oj::render
