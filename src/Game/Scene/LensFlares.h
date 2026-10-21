#pragma once
// The lens flares of a world (its +0x120 list, drawn by 0x0054FC60 after its nodes) and the device's part the stock
// keeps once for every world: the race's track and each interface view's world (docs/spec/21-scene-objects.md).

#include "Core/Base/Matrix.h"
#include "Core/Tracks/SceneObjects.h"
#include "Render/Camera.h"
#include "Render/Materials.h"
#include "Render/RenderDevice.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace oj::scene {

class Engine;

// A batch of a flare texture (0x005089B0: 0x800 vertices of 0x20 bytes): its Light Flare material (0x00541210) and
// the quads of the frame.
struct FlareBatch {
    std::string texture;
    std::unique_ptr<render::Material> material;
    render::BufferId vertices = 0;
    std::vector<float> data;
};

// What the stock keeps once for every world's flares (globals, made with the first flare by 0x00541030): the batches
// of the textures, the flares' declaration, the index buffer, the visibility test's point, declaration and material,
// and the occlusion queries.
struct FlareDevice {
    bool made = false;
    std::vector<FlareBatch> batches;                 // [0x0079D9FC]
    render::DeclarationId declaration = 0;           // [0x0075F18C]: position, two texture coordinates, colour
    render::DeclarationId pointDeclaration = 0;      // [0x0075F184]: position
    render::BufferId indices = 0;                    // [0x0075F198]: two triangles a quad
    render::BufferId point = 0;                      // [0x0075F180]: the visibility test's point
    std::unique_ptr<render::Material> visibility;    // [0x0075F188]: <visibilty test>, Constant Colour
    bool queries = false;                            // [0x0075F1A0]
    std::array<render::QueryId, 256> ring{};         // 0x0079DA10: the queries and their polls left before a flush
    std::array<uint32_t, 256> polls{};
    uint32_t next = 0;                               // [0x0075F19C]
};

class LensFlares {
public:
    // A lens flare (0x00541030, 0x200 bytes): its scene objects' record, the flare node's matrix (the node's as the
    // load left it), its texture's batch, the visibility of the main view (+0xEC) and the query it waits for (+0xE4).
    struct Flare {
        const SceneFlare* def = nullptr;
        Mat34 world = identity34();
        size_t batch = 0;
        float visibility = 0.0f;
        int32_t query = -1;
        render::Node* node = nullptr;  // a car's flare: its node in the car's tree (the world matrix each frame)
        const bool* on = nullptr;      // +0xA4: off, the test leaves the visibility at 1 and no quad is made
        // The batches of the reflection on the cars (+0xB4, LIGHTFLARE_REFLECTION's texture) and on the road (+0xB8,
        // the y reflection's): those of their textures; SIZE_MAX without the texture.
        size_t reflectionBatch = SIZE_MAX;
        size_t roadBatch = SIZE_MAX;
        // The road's height under it (+0x1F4, 0 without a face) for its place across (+0x1EC, +0x1F0).
        float groundX = 3.4028235e38f;
        float groundZ = 3.4028235e38f;
        float ground = 0.0f;
    };

    explicit LensFlares(Engine& engine);

    std::vector<Flare>& flares() { return m_flares; }
    bool empty() const { return m_flares.empty(); }
    void clear() { m_flares.clear(); }
    // The batch of a flare texture, made with its Light Flare material (0x00541210) the first time.
    size_t batch(const std::string& texture);
    FlareBatch& batchAt(size_t b);
    size_t batchCount() const;
    bool queries() const;
    // A flare's reflection batches (0x005416DB: with the texture found, the batch of its material, 0x005089B0).
    void makeReflectionBatches(Flare& flare);
    // The flares' declarations, index buffer, visibility test and queries, made with the first flare.
    void makeDevice();
    // A flare's world matrix now: a car's flare's node's (car_flare01 turned about its own x).
    void flareWorld(Flare& flare);
    // The sun's place from a camera (0x00540A70, 0x00541C10, 0x00542100): toward the sun the flare's distance from it.
    void placeSun(render::Camera& camera, Flare& flare);
    // 0x0054FC60 (after the world's nodes): each sun put in its place (0x00540A70); with the occlusion queries each
    // flare's visibility test (0x00540C70), without them every flare fully visible; each flare's quad (0x00541C10);
    // the batches drawn (0x00540EE0).
    void draw(render::Camera& camera);
    // 0x00540EE0: each batch with quads, its material's first pass; the depth test only without the queries.
    void drawBatches();

private:
    // 0x00540C70
    void test(render::Camera& camera, Flare& flare);
    // 0x00541C10
    void quad(render::Camera& camera, Flare& flare);

    Engine& m_engine;
    std::vector<Flare> m_flares;  // the world's +0x120
};

}  // namespace oj::scene
