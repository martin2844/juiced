#pragma once
// The track of a race (docs/spec/15-tracks.md): a route's geometry layers with their instances, materials, textures
// and baked-light tiles, the regions the instances are drawn from, and the world's main pass.

#include "Core/Formats/Cld.h"
#include "Core/Base/Random.h"
#include "Core/Tracks/SceneObjects.h"
#include "Game/Cars/CarModel.h"
#include "Game/Scene/LensFlares.h"
#include "Game/Scene/Projector.h"
#include "Game/Scene/SunLight.h"
#include "Game/Scene/Particles.h"
#include "Core/Tracks/SunMap.h"
#include "Core/Tracks/Track.h"
#include "Game/Scene/Scene3d.h"
#include "Game/Scene/WorldClock.h"
#include "Render/Sky.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace oj::audio {
class World;
}

namespace oj::scene {

// What RaceTrack (0x00424270) loads: the folder under tracks\, the route and the race's conditions.
struct TrackSetup {
    std::string track;       // "Engstrom District"
    std::string route;       // the route's name in the .tdf without ".wld": "Engstrom_R1"
    uint32_t timeOfDay = 2;  // the race's +0x6C: 1 morning, 3 evening, 4 night, else noon
    uint32_t weather = 0;    // the race's +0x70: 2 wet
    const EffectLibrary* effects = nullptr;  // the race's effects (the scene objects' emitters)
    bool sound = false;                      // [0x0075F860]
    float density = 0.0f;                    // [0x0075F794]: the particle density
    audio::World* soundWorld = nullptr;      // [G+0x50]: the race's 3D sounds, the effects manager's +0x80
};

class TrackScene {
public:
    explicit TrackScene(Engine& engine);
    ~TrackScene();

    // Reads the route. `random` is the loading thread's rand(): four values per instance.
    bool load(const TrackSetup& setup, CrtRandom& random, std::string* why);

    const Spl& lines() const { return m_lines; }
    // The route's collision (<track>_<route>.cld), none when it does not read.
    const Cld* collision() const { return m_collision ? &*m_collision : nullptr; }
    // The ground maps of the race's light set (<track>_<set>.sqh, .pqh; the race track's +0x80, +0x84), none when
    // they do not read.
    const SunMap* sunMap() const { return m_sunMap ? &*m_sunMap : nullptr; }
    const SunMap* puddleMap() const { return m_puddleMap ? &*m_puddleMap : nullptr; }
    // The light steps of the scene's barrels, bins and wheels (slot 3 at the end of the track's making, 0x00422AC0)
    // and their steps on the frames the objects step (0x00422BB0).
    void startObjectLights();
    // The objects' steps with the objects' step time; the game's frame time for a barrel's crash.
    void stepObjectLights(float dt, float frameTime);
    // A barrel's first hit (0x004B0280): the cruise's crash of 2000 by no car, with the game's frame time.
    std::function<void(float)> barrelCrash;
    // The race's particles (the effects manager), none without the race's effects.
    ParticleSystem* particles() { return m_particles.get(); }
    // The scene objects of the route's .dlh files.
    const SceneObjects& sceneObjects() const { return m_sceneObjects; }
    SceneObjects& sceneObjects() { return m_sceneObjects; }
    const render::AtmosphereSet* atmosphere() const { return m_atmosphere.get(); }
    render::AtmosphereSet* atmosphere() { return m_atmosphere.get(); }

    // The world's clock (+0x08 .. +0x20): the race's pause stops it.
    WorldClock& clock() { return m_clock; }

    // The camera's constants, the atmosphere, then the world's main pass (0x00547690); clock: the game's clock.
    void draw(render::Camera& camera, float clock);
    // The world's slot 10 (0x005485A0) as the race's load ends on the main thread (0x004255F0): the world's cube
    // (0x00548450).
    void loaded();
    // 0x00547D50 (the world's slot 14) as the race's view calls it before it begins (0x00511D00): the cars'
    // reflection cube (the renderer's +0x218) drawn around the camera's focus.
    void drawCarCube(render::Camera& camera, float clock);
    // A car standing on a place of the grid (facing along the place's heading); the model kept, none for a model
    // without nodes.
    CarModel* addCar(std::unique_ptr<CarModel> car, const Spl::GridPoint& place);
    // 0x00531260: a texture laid on the route under a node joins the world's list (+0x98); kept with the world.
    Projector* addProjector(render::Node& node, const Vec4& low, const Vec4& high, const render::GameTexture* shadow,
                            const render::GameTexture* glow);
    // 0x00531380: a car's silhouette shadow of a light joins the same list.
    Silhouette* addSilhouette(render::Node& node, int type, Silhouette::DrawCar drawCar);
    Engine& engine() { return m_engine; }

private:
    // An instance of a mesh (200 bytes): the regions it is drawn from (+0x00), the constants c0 .. c6 (+0x20: the
    // node's three rows, the column lengths, the columns over their lengths), the bounding radius (+0xA0), the light
    // tile (+0xA4), the random value of c40 (+0xA8), the bounding sphere's centre in the world (+0xB8).
    struct Instance {
        std::array<uint32_t, 8> regions{};
        std::array<float, 28> block{};
        float radius = 0.0f;
        int32_t tile = -1;
        oj::Vec4 random{};
        oj::Vec4 centre{};

        oj::Mat34 matrix() const;
    };
    // A mesh of a geometry layer (0x58 bytes): the distance ranges (+0x00), the bounding sphere (+0x18), drawn
    // after the cars (+0x2C), the buffers (+0x30 ..) and its instances (+0x50, +0x54).
    struct Mesh {
        std::array<float, 6> range{};
        oj::Vec4 centre{};
        bool deferred = false;
        uint32_t primitive = 0;
        render::Material* material = nullptr;
        uint32_t classId = 0;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        render::DeclarationId declaration = 0;
        uint32_t stride = 0;
        render::BufferId vertices = 0;
        render::BufferId indices = 0;
        std::vector<Instance> instances;
    };
    struct Entry {
        const Mesh* mesh;
        Instance* instance;
    };
    // A region as the world's +0x140 grid gives it (0x00551C00): its number, the word of the 256 bits and the bit in
    // it.
    struct Region {
        uint32_t number = 0;
        uint32_t word = 0;
        uint32_t bit = 1;
    };

    // A root of the scene objects that joined the world (the world's +0xD8 roots, 0x00550400): its node and its sphere
    // (+0x64 .. +0x74: a model node's mesh's, a spline node's around its knots; none for another node).
    struct SceneRoot {
        size_t node = 0;
        Vec4 centre{};
        float radius = 0.0f;
    };

    // The model nodes of the scene objects' .dlh files (0x00556AE0) with their meshes (0x00559780: the parts'
    // materials through the route's table, as the layers' meshes).
    void makeSceneNodes();
    // 0x00550500 with no lights near: each root in the order it joined, when its sphere (the mesh's centre through
    // the node's matrix) is in the view (0x00552BC0: a root that is not a model node never is): its model nodes,
    // the root first, then its children depth first, with the parts of opaque classes (1), then again with the others
    // (2). A physics object's node follows its body.
    void drawSceneObjects(render::Camera& camera);

    using Flare = LensFlares::Flare;
    // 0x0054D8D0 (identityObject in Game/Scene/Scene3d).
    void identityObject(render::Camera& camera);
    // The flares of the scene objects (the world's +0x120, 0x005500B0), then the race's sun (0x00425297: with the
    // atmosphere's SunFlare found, a flare of SunFlareSize x 2000, fall-off "cosines" 9 and 10, 200000 cm from the
    // camera, the SunAngle; its SunShape and SunReflection are kept by the stock flare and not drawn by the main view).
    void makeFlares();

    // 0x00546540
    void drawInstance(render::Camera& camera, const Mesh& mesh, const Instance& instance);

    // The reflections (docs/spec/15-tracks.md "The reflections").
    // 0x00548450 (from the world's slot 10 as the load ends, 0x005485A0, and from the main pass after the renderer's
    // start or a reset of the device): the world's cube (+0x21C) drawn once from the environment's place
    // (+0xAC) with every face, the reflections 3 and the renderer's +0x188 1.
    void drawWorldCube(float clock);
    // 0x00547D50 into a cube: the instances near its focus (0x005471E0) on the faces they reach, the sky dome, those
    // drawn after the cars, the flares' reflections; with the reflections below 3 half the faces a frame.
    void drawCube(render::Camera& camera, const render::GameTexture& cube, float clock);
    // 0x005471E0: the instances within the cube's reach (the cars' environment range, the radius of the world's
    // +0x190) with the faces they reach (0x00547040), from the region's list or the render tree.
    void collectCube(const Region& region, const Vec4& focus, float radius);
    // 0x00546BF0: the render tree turned upside down about the height (the wet road's reflection), near to far,
    // in the reflection range; those drawn after the cars go to the list.
    void drawReflectedTree(render::Camera& camera, const Region& region, float height);
    // 0x0053F2A0: the lowest and highest of the route's faces straight under and over a point (x, z); false when no
    // face is there.
    bool groundHeights(float x, float z, float& lowest, float& highest) const;
    Region regionAt(float x, float y, float z) const;
    // An instance turned upside down about a height (twice it given) or back: its y column negated and its height
    // the twice less it.
    static void mirror(Instance& instance, float twice);
    // 0x0054FE30 (a cube's face, ZENABLE on) and 0x0054FF00 (the main pass before the region's instances): each
    // flare's reflection on the cars (0x00542100) or on the road (0x00542530), then the batches drawn (0x00540EE0).
    void drawFlareReflections(render::Camera& camera);
    void drawFlareRoadReflections(render::Camera& camera);
    // 0x005469A0
    void drawRegion(render::Camera& camera, uint32_t region, float lodScale);
    void setLightTile(const render::GameTexture* tile);

    Engine& m_engine;
    std::vector<std::unique_ptr<LaidObject>> m_projectors;  // the world's +0x98
    Spl m_lines;
    std::optional<Cld> m_collision;
    std::optional<SunMap> m_sunMap;
    std::optional<SunMap> m_puddleMap;
    std::vector<LightStep> m_objectLights;
    std::unique_ptr<ParticleSystem> m_particles;
    audio::World* m_soundWorld = nullptr;  // [G+0x50]
    std::vector<std::shared_ptr<Emitter>> m_sceneEmitters;
    void barrelStep(LightStep& l, float frameTime);
    SceneObjects m_sceneObjects;
    std::optional<Ocl> m_regions;
    std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::vector<std::vector<Entry>> m_lists;  // +0x174: per region, by material class
    std::vector<Entry> m_deferred;            // +0x1A8
    // The render tree of the .d8w (the world's +0x194, its root +0x58) and each leaf's instances.
    std::vector<RenderTreeNode> m_tree;
    std::vector<std::vector<Entry>> m_treeEntries;
    // The cube's lists (0x005471E0): the instances drawn after the cars go to +0x1A8 with their faces (+0x1C8), the
    // others to +0x1B8 (+0x1D8).
    std::vector<uint32_t> m_deferredFaces;   // +0x1C8
    std::vector<Entry> m_cubeEntries;         // +0x1B8
    std::vector<uint32_t> m_cubeFaces;        // +0x1D8
    float m_cubeRange = 0.0f;                 // +0x190: the cars' environment range's greatest far (squared)
    float m_reflectionRange = 0.0f;           // +0x1A0: the reflection range's greatest far (squared)
    Vec4 m_environmentPlace{0.0f, 10000.0f, 0.0f, 0.0f};  // +0xAC: an env_camera* node's place
    const render::GameTexture* m_testEnvironment = nullptr;  // +0x1E8: testenv
    uint32_t m_cubeFrames = 0;                // +0x1A4: the cubes drawn (half the faces by turns)
    float m_lodScale = 1.0f;                  // [0x0075B2CC]: the scale of the distances
    Vec4 m_cameraPlace{};                     // [0x0079E22C]: the camera's place of the pass
    std::array<render::Camera, 6> m_faces;    // the cube's face cameras (0x005C8D10)
    std::vector<std::unique_ptr<render::Material>> m_materials;
    std::vector<std::unique_ptr<render::GameTexture>> m_textures;  // the route's .d8w textures, by index
    std::vector<std::unique_ptr<render::GameTexture>> m_tiles;     // +0x13C: the .shd's light tiles
    std::unique_ptr<render::AtmosphereSet> m_atmosphere;
    std::unique_ptr<render::SkyDome> m_sky;  // +0xA8
    std::vector<std::unique_ptr<CarModel>> m_cars;
    std::vector<std::unique_ptr<render::Mesh>> m_objectMeshes;
    std::vector<std::vector<render::Mesh*>> m_meshOf;        // per .dlh file, per object
    std::vector<std::vector<std::array<float, 5>>> m_sphereOf;  // per .dlh file, per object: the mesh's sphere
    std::vector<std::unique_ptr<render::ModelNode>> m_sceneNodes;  // per node of the scene objects
    std::vector<Mat34> m_baseLocal;  // their own matrices as the files left them (a turning object's +0x7C)
    std::vector<SceneRoot> m_sceneRoots;
    LensFlares m_lens;  // the world's flares (+0x120) and the shared batches
    std::unique_ptr<SceneFlare> m_sun;  // the race's sun flare (the race track's +0x28)
    const render::GameTexture* m_environment = nullptr;
    WorldClock m_clock;
};

// The splines' debug draw (slot 2 of the vtables 0x006EEE70 / 0x006EF040, 0x0041BB40; nothing calls it): from 0 by
// 0.1 while under the path's count (+0x20) a line from the point at the place to the point 0.1 further (0x0053FF50),
// both moved by the node's place (+0x1C's +0x10, +0x20, +0x30) and put on the screen of 640 x 480 (the camera's
// projection, +0x5C of the object [[[[+0x18] + 0x58] + 0x0C]], the depth the third value); drawn (0x00517A60) only
// when both are within 0 .. 640 and 0 .. 480 and deeper than 10, in green and red by turns (each step turns it).
void drawSplineDebug(const SceneSpline& spline, const Mat34& node, const render::Camera& camera,
                     render::IRenderDevice& device);

}  // namespace oj::scene
