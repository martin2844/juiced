#pragma once
// TRACKS: the tyres' marks on the road.

#include "Audio/Voices.h"
#include "Core/Data/EffectDefs.h"
#include "Core/Tracks/SunMap.h"
#include "Game/Scene/Particles/Emitter.h"
#include "Game/Scene/Particles/ParticleSystem.h"
#include "Render/Camera.h"
#include "Render/Materials.h"
#include "Render/RenderDevice.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace oj::scene {

// TRACKS (0x00506520 on 0x0050CE70, vtable 0x006F70F0): the tyre tracks of a car. A ring of entries (+0x6C ..
// +0x78, 0x10 + 8 bytes a wheel each) holds the car's origin and, per wheel, its touch (2.2 cm steps from the origin),
// two alpha bytes and the strip's side; a track per wheel (0x0050D3E0 on the strip base 0x0050C390, vtable
// 0x006F75CC) keeps strips of up to 26 entries and draws them as triangle strips 10 cm over the road.
class TracksEmitter final : public Emitter {
public:
    TracksEmitter(ParticleSystem& system, const EffectDef& def);
    ~TracksEmitter() override;
    // 0x00506560 (0x0048FED2, the race's cars): the wheels' tracks; the ring of NUM_POINTS / cars entries, each track
    // 240 / cars strips.
    void setup(uint32_t wheels, uint32_t cars);
    // 0x005068A0: each track drawn; it is never done.
    bool frame(render::Camera& camera) override;
    // 0x004907D0's parts: a track asks for its surface's TRACKS (0x00503800; a new one taken at its next advance), the
    // ring advanced when a track asked it last frame (0x00506800), the car's origin into the current entry
    // (0x00490A07), a wheel's mark (0x0050C7D0: no touch puts it off).
    void askDefinition(size_t wheel, const EffectDef* def);
    void advance();
    void setOrigin(const Vec4& carOrigin);
    void mark(size_t wheel, const Vec4* touch, float a, float b);
    // 0x0050C440 for every track (0x00492710).
    void resetTracks();

private:
    // A strip (0x3C bytes): its bounds (0x0050D040, 0x0050D0C0: the middle +0x00, the radius +0x10 = half the box's
    // diagonal + the pad +0x2C, the box's top +0x14 and bottom +0x20), its first entry (+0x30), its entries (+0x34)
    // and its definition (+0x38).
    struct Strip {
        Vec4 middle{};
        float radius = 0.0f;
        Vec4 top{};
        Vec4 bottom{};
        float pad = 0.0f;
        int32_t first = -1;
        uint32_t count = 0;
        const EffectDef* def = nullptr;
    };
    struct Vertex {
        float x, y, z;
        uint32_t colour;
        float u, v;
    };
    struct Track {
        const EffectDef* def = nullptr;      // +0x40
        float lift = 10.0f;                  // +0x5C
        size_t wheel = 0;                    // +0x64
        std::vector<Strip> strips;           // +0x68 .. +0x70 (the last place never used)
        size_t current = 0;                  // +0x6C
        size_t oldest = 0;                   // +0x74
        uint32_t count = 0;                  // +0x78
        int32_t switchIn = -1;               // +0x80: frames to the asked definition
        const EffectDef* asked = nullptr;    // +0x84
        Vec4 last{};                         // +0x88: the touch where the strip last changed
        bool wasOn = false;                  // +0x98
        bool open = false;                   // +0x9C
        int32_t entry = -1;                  // +0xA0: the ring's entry it marks
        int32_t pending = -1;                // +0xA4: the entry its mark began on
        Vec4 origin{};                       // +0xA8: that entry's origin
        bool on = false;                     // +0xB8
        uint32_t kept = 0;                   // +0xBC
        uint32_t drawn = 0;                  // +0xC0
        render::BufferId buffer = 0;         // +0xC8: 0x36 vertices
    };

    uint8_t* entryAt(int32_t index) { return m_ring.data() + static_cast<size_t>(index) * m_stride; }
    int32_t nextEntry(int32_t index) const;
    void startStrip(Track& t, int32_t next);  // 0x0050C5E0
    void closeStrip(Track& t);                // 0x0050C670
    void advanceTrack(Track& t, int32_t next);  // 0x0050C6C0
    bool drawTrack(Track& t, render::Camera& camera);  // 0x0050D490
    bool visible(const Track& t, const Strip& s, render::Camera& camera) const;  // 0x0050C560

    std::vector<uint8_t> m_ring;
    uint32_t m_stride = 0;     // +0x7C
    int32_t m_last = 0;        // +0x78: the ring's last entry (wrapping before it)
    int32_t m_current = 0;     // +0x70
    uint32_t m_entries = 0;    // +0x80
    uint32_t m_strips = 0;     // +0x84
    bool m_moved = false;      // +0x8C
    std::vector<Track> m_tracks;  // +0x88, +0x90
    std::vector<Vertex> m_vertices;
};

}  // namespace oj::scene
