#pragma once
// The 3D scenes of the interface (docs/spec/12-rendering.md): the engine's programs, textures, meshes and
// materials read from the game folder, the models of models\, and the view a Begin3d element shows.

#include "Core/Data/ExeTables.h"
#include "Core/Base/GameFiles.h"
#include "Core/Base/Random.h"
#include "Game/Scene/LensFlares.h"
#include "Game/UI/UiScreen.h"
#include "Render/Camera.h"
#include "Render/Materials.h"
#include "Render/Model.h"
#include "Render/Program.h"
#include "Render/Renderer3d.h"
#include "Render/Sky.h"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace oj::scene {

// The model nodes of a .boh, depth first; the first top node is the root.
struct Model {
    std::string name;  // the name it was loaded by (the element's list compares it)
    // Its top node hidden (+0xC0 | 4, 0x0045B720): only that node's own mesh goes undrawn (0x00559191), the nodes
    // below it are drawn.
    void setHidden(bool on);
    bool hidden() const;
    std::vector<std::unique_ptr<render::ModelNode>> meshNodes;
    std::vector<std::unique_ptr<render::Node>> otherNodes;
    // The meshes loaded for it alone while the mesh cache was off (Engine::setMeshCaching).
    std::vector<std::unique_ptr<render::Mesh>> ownMeshes;
    render::Node* root = nullptr;
    // The nodes below the top nodes with their names, depth first.
    std::vector<std::pair<std::string, render::Node*>> named;
    // 0x00534EC0: the first node whose name holds the text; none without one.
    render::Node* find(const char* text) const;
    // 0x00459070 on a copy: every mesh node mirrored (+0xC0 | 1), the top node's Y axis and height negated.
    void mirror();
};

class PostEffects;

class Engine {
public:
    // localEffects / debugEffects: -localfx / -debugfx ([0x0075F134], [0x0075F138]): the programs from the
    // developers' .fx sources (docs/spec/11-effects.md).
    Engine(render::IRenderDevice& device, const ExeTables& tables, uint32_t shadersSetting, bool wideScreen,
           bool localEffects = false, bool debugEffects = false);
    ~Engine();
    // The post effects of the race's view (the renderer's +0x1F8 .. +0x224, made at its start; here at first use).
    PostEffects& postEffects();
    // The lens flares' objects the stock keeps once for every world (Game/Scene/LensFlares); made at first use.
    FlareDevice& flareDevice();
    // The reflections' targets the renderer makes at its start (0x005147A3 .. 0x0051489F; here at first use): the
    // cars' cube (+0x218, the settings' +0x10 a side), the wet road's reflection (+0x214, +0x2C x +0x30) and the
    // world's cube (+0x21C, +0x14), their textures with linear filters and clamped addresses. The world's cube is due
    // (+0x220) after the start and after each reset of the device (0x00514248), which loses what they held.
    struct ReflectionTargets {
        render::GameTexture carCube;
        render::GameTexture road;
        render::GameTexture worldCube;
        bool worldCubeDue = true;
        uint32_t resets = 0;
    };
    ReflectionTargets& reflectionTargets();

    render::IRenderDevice& device() { return m_device; }
    render::Renderer3d& renderer() { return m_renderer; }
    // Each frame of the renderer before the views: after a reset of the device the programs made again (0x00519700,
    // from the reset 0x005142F3); the programs' passes ended (0x00514C56 → 0x00519F70) and their technique by the
    // detail (0x00514C61 → 0x00519E80); then (0x00514C88 → 0x00519DD0) the effects' shared vAmbUnit the renderer's
    // ambient (+0x28) with r, g and b over 255, and vOne (1, 1, 1, 1).
    void frameParameters();
    bool wideScreen() const { return m_wideScreen; }
    // The device's shader classes (the renderer's +0x684).
    uint32_t shaderClasses() const { return m_programs.classes(); }

    // A program made at the start (0x00519CB0, the name case aside); nullptr when it is not among them, its effect
    // did not load or it has no usable technique.
    render::Program* program(const std::string& name);
    // <textures>\<name>.ddt of the folders in use, loaded once.
    const render::GameTexture* texture(const std::string& name);
    // <folder>\<name>.ddt when there is one (a car's textures\), else <shared>\<name>.ddt when there is one (the
    // cars' cartextures\, 0x00498610), else textures\<name>.ddt; loaded once.
    const render::GameTexture* texture(const std::string& name, const std::filesystem::path& folder,
                                       const std::filesystem::path& shared = {});
    // A new material of a part from <mats>\<part>.pmd, its textures looked up in `textures` and `shared` first;
    // nullptr when the file or its class is missing (logged).
    std::unique_ptr<render::Material> loadMaterial(const std::filesystem::path& mats, const std::string& part,
                                                   const std::filesystem::path& textures,
                                                   const std::filesystem::path& shared = {});
    // textures\<name>.dds as a cube texture with D3DX's default mip levels (0x00510280), loaded once; its sampler
    // states are linear filters and clamped addresses (0x00511110).
    const render::GameTexture* cubeTexture(const std::string& name);
    // <mesh>\<name>.d8m with the materials of its parts (<mats>\<part>.pmd) of the folders in use, loaded once.
    render::Mesh* mesh(const std::string& name);
    // 0x005582B0 (nothing in the game calls it): the mesh's <name>.d8m of the meshes' folder read again into its
    // vertex buffers (render::Mesh::reloadVertices); false when the file does not open.
    bool reloadMesh(render::Mesh& mesh, const std::string& name);
    // The mesh manager's cache ([G+0x1C]+0x14, on): off, a model's meshes are read anew for it (0x00554D30 ->
    // 0x005596E0), each with materials of its own; the HUD's map turns it off for its markers (0x0044A100).
    void setMeshCaching(bool on) { m_meshCaching = on; }
    // <models>\<name>.boh of the folders in use: a model node for every mesh node (four rand() values each, as the
    // stock nodes draw).
    std::unique_ptr<Model> loadModel(const std::string& name, CrtRandom& random);
    // The folders the managers read from: the game's (models, mesh, mesh\mats, textures) or, while cars load, the
    // cars' (0x00498610: carmodels, carmesh, carmesh\mats, cartextures). 0x00498780 sets the cars' and keeps the
    // folders before ([0x0075EF54], [0x0075EF4C], [0x0075EF50]); 0x00498840 puts the kept ones back.
    void useCarFolders();
    void restoreFolders();
    // A new material of a class by name (the material manager's +4), nullptr for a class past the table.
    std::unique_ptr<render::Material> newMaterial(const std::string& className);
    // The same by the class number; `info` gets the class record (nullptr for a number the game does not have).
    std::unique_ptr<render::Material> newMaterial(uint32_t classId, const ExeTables::MaterialClass** info);
    // A texture of a .ddt image held in memory (a track's textures and light tiles); nullptr when it does not read.
    std::unique_ptr<render::GameTexture> makeTexture(const std::vector<std::byte>& ddt);

private:
    render::Material* material(const std::string& part);
    // 0x00519BC0: a program of effects\<name>.fxo added to the effects' manager (one without a usable technique,
    // or whose file does not load, as well).
    void makeProgram(const std::string& name);
    // 0x00519660: the effects' folder and a file's path in it.
    GameFiles effectFiles() const;
    std::filesystem::path effectFile(const std::string& file) const;
    // A texture parameter of a program (the end of 0x0051AAC0).
    render::TextureId effectTexture(const render::Program& program, const render::Program::TextureRequest& request);
    // 0x0051A930: a texture of the effects' folder; a file that does not open makes a 16 x 16 texture.
    render::TextureId loadEffectTexture(const std::string& file);
    // 0x0051B2C0: proc_<program>_<function>.dds, with -localfx in the developers' folder.
    std::string proceduralName(const render::Program& program, const render::Program::TextureRequest& request) const;
    // 0x0051B400: with -localfx the function compiled as a texture shader, a texture filled with it and saved.
    render::TextureId makeProceduralTexture(const render::Program& program,
                                            const render::Program::TextureRequest& request);

    render::IRenderDevice& m_device;
    const ExeTables& m_tables;

public:
    const ExeTables& tables() const { return m_tables; }

private:
    bool m_wideScreen;
    render::ProgramContext m_programs;
    render::Renderer3d m_renderer;
    std::vector<std::unique_ptr<render::Program>> m_programList;
    render::ProgramManager m_effects;  // [0x0075B294]
    uint32_t m_effectsResets = 0;      // the device's resets the programs were made again for
    bool m_localEffects = false;       // [0x0075F134]
    bool m_debugEffects = false;       // [0x0075F138]
    std::map<std::string, render::TextureId> m_effectTextures;
    struct Folders {
        std::filesystem::path models{"models"};
        std::filesystem::path mesh{"mesh"};
        std::filesystem::path mats{std::filesystem::path("mesh") / "mats"};
        std::filesystem::path textures{"textures"};
    };
    // A key of the caches: the name for the game's folder, <folder>|<name> for another.
    static std::string cacheKey(const std::filesystem::path& folder, const char* own, const std::string& name);
    Folders m_folders;
    Folders m_keptFolders;
    std::map<std::string, std::unique_ptr<render::GameTexture>> m_textures;
    std::map<std::string, std::unique_ptr<render::GameTexture>> m_cubeTextures;
    std::map<std::string, std::unique_ptr<render::Mesh>> m_meshes;
    bool m_meshCaching = true;
    std::unique_ptr<render::Mesh> readMesh(const std::string& name);
    std::vector<std::unique_ptr<render::Material>> m_materials;
    render::GameTexture m_halfVectorCube;
    render::GameTexture m_normalCube;
    std::unique_ptr<PostEffects> m_postEffects;
    std::unique_ptr<ReflectionTargets> m_reflections;
    std::unique_ptr<FlareDevice> m_flareDevice;
};

// The cars' folders in use for a scope (0x00498780 at its start, 0x00498840 at its end).
class CarFolders {
public:
    explicit CarFolders(Engine& engine) : m_engine(engine) { engine.useCarFolders(); }
    ~CarFolders() { m_engine.restoreFolders(); }
    CarFolders(const CarFolders&) = delete;
    CarFolders& operator=(const CarFolders&) = delete;

private:
    Engine& m_engine;
};

// 0x00523020 (the material's serialisable interface, slot 0 of vtable 0x006F8524 through 0x00523E40; nothing calls
// it): a material's description as the .pmd text the reader takes (0x00523760): "Material Program = %s" with the
// class's name, then each parameter "%s = " and its value (values in the class's order, as the description holds
// them) by its type: 1 "%d", 2 and 3 "%f", 4 TRUE or FALSE, 5 the colour's red, green and blue "%d %d %d", 6
// "%f %f %f", 7 the text, any other type nothing more; then "%s = %s" for each texture slot that has a texture; each
// line ends with CR LF.
std::string writeMaterialText(const ExeTables::MaterialClass& info, const std::vector<std::vector<uint8_t>>& values,
                              const std::vector<bool>& textured);

// The camera's constants of a 3D view (0x00511CA0): c7 .. c17 the view, the projection and both together, c32 the
// position, c33 the direction, c41 and c42 the axes of sprites facing the camera.
void setViewConstants(render::Renderer3d& r, render::Camera& camera);
// 0x0054D8D0: the object constants of no object: c0 .. c2 and c4 .. c6 the identity's rows, c3 (1, 1, 1, 1), c18 ..
// c21 the camera's world-to-clip.
void identityObject(render::Renderer3d& r, render::Camera& camera);
// The sprites' axes (c41, c42) of a view looking along (fx, fy, fz) (0x00511F5F): up (0, 1, 0, 0) x the direction,
// then the direction x that, each normalised as 0x005115C0 does (the four squares summed w first, 1 / (the root +
// 1.1920929e-07)).
void spriteAxes(float fx, float fy, float fz, float out[8]);

// The world of a 3D view without an atmosphere (0x00530D70, then the view's defaults): its lights.
struct World {
    oj::Vec4 ambient{50.0f, 50.0f, 50.0f, 0.0f};        // +0x58
    oj::Vec4 light1Colour{255.0f, 255.0f, 255.0f, 0.0f};  // +0x28
    oj::Vec4 light2Colour{};                             // +0x38
    oj::Vec4 light3Colour{};                             // +0x48
    oj::Vec4 direction1{0.0f, -1.0f, 1.0f, 0.0f};        // +0x68
    oj::Vec4 direction2{0.0f, -1.0f, 0.0f, 0.0f};        // +0x78
    oj::Vec4 direction3{0.0f, 1.0f, 0.0f, 0.0f};         // +0x88
    float time = 0.0f;                                       // +0x10
    float fade = 0.0f;                                       // +0x1C: the view's fade
    std::unique_ptr<render::AtmosphereSet> atmosphere;      // +0x0C
    std::unique_ptr<render::SkyDome> sky;                    // +0xA8
    const render::GameTexture* environment = nullptr;        // +0xD0
};

// How a 3D view moves (its +0x128): the .ui's MODEL_AUTO, MODEL_MANUAL, CAMERA_AUTO, CAMERA_MANUAL, or the map's (its
// arm hung on the pivot at the look-at point).
enum class MoveMode : uint32_t { ModelAuto = 0, ModelManual = 1, CameraAuto = 2, CameraManual = 3, Map = 4 };

// A Begin3d element (0x005B36D0): the camera, its rig for the map mode, the world and the model.
class View3d final : public ui::View3d {
public:
    View3d(Engine& engine, const ui::Element& element, CrtRandom& random);

    void draw(float x0, float y0, float x1, float y1, float seconds, float dt) override;
    void setRound(bool round) override { m_round = round; }

    render::Camera& camera() { return m_camera; }
    Engine& engine() { return m_engine; }
    // 0x005B4380: the point the camera turns to; the angles toward it from the camera's position (+0x130), and the
    // camera's own angles too when `now`.
    void lookAt(const oj::Vec4& point, bool now);
    // 0x005B4490: another move mode; the map mode hangs the arm on the pivot at the look-at point, the arm at
    // `arm` (default (0, 0, -500)), and sets the rig angles to (-0.12, -1.57).
    void setMode(MoveMode mode, const oj::Vec4* arm);
    // 0x005B4610: the world's atmosphere from ui\atmospheres\<name>.atm (0x00531510), its light 1 direction and
    // environment texture.
    void setAtmosphere(const std::string& name);
    // 0x005B4660: the sun and the sky dome of the world's atmosphere (0x005315A0). With the atmosphere's SunFlare found
    // as a texture (0x005A8D30: the interface's textures\), a lens flare as the race's sun (0x00425297) joins the
    // world's flares (0x005428D0, the world's +0x10: +0x120). Stock problem: SunShape's light object and SunReflection's
    // reflection on the cars both take the SunFlare's texture (the light object is never drawn, nor are the
    // reflections in a view).
    void buildSky();
    // The element's list of models (+0x118, 0x005B3E60): <models>\<name>.boh of the engine's folders in use at a
    // place (centimetres; its node's translation); -1 when it is not read.
    int addModel(const std::string& name, const oj::Vec4& place, CrtRandom& random);
    int modelCount() const { return static_cast<int>(m_models.size()); }
    // The element's own model (SetModel; the stock list's first); none without one.
    Model* ownModel() { return m_model.get(); }
    // The list's model at that place taken out (the list after the element's own model, which the stock list holds
    // first: index 0 here is the stock list's 1).
    void removeModel(int index);
    Model* listedModel(int index) { return m_models[static_cast<size_t>(index)].get(); }
    // 0x005B3520 after the place: a listed model's node as the scale, at the place, turned about y, x and z by the
    // angles.
    void placeModel(int index, const oj::Vec4& place, const oj::Vec4& angles, float scale);
    // The camera at a place and its angles, at once (the race's map, 0x0044A7A0: the element's +0x130 and its camera
    // node, the angles +0x140 and +0x150).
    void setCameraNow(const oj::Vec4& place, float pitch, float yaw);
    // The camera's goal (+0x130) and its node at that place (0x0045B050 and the car view's 0x0045AEF0).
    void setCameraPlace(const oj::Vec4& place);
    // The camera's goal alone (+0x130 .. +0x138).
    void setCameraGoal(const oj::Vec4& place) { m_target = place; }
    MoveMode mode() const { return m_mode; }
    void setZoom(float zoom) { m_zoom = zoom; }              // +0x188
    void setZoomTarget(float zoom) { m_zoomTarget = zoom; }  // +0x18C

    // 0x005B4150: a node of the element's world listed (+0x118, an entry of 0x80 bytes, 0x005B34A0) to turn by its
    // rates each frame (+0x60, +0x64, +0x68 about X, Y and Z by the frame time; `parent` +0x74: about the parent's
    // axes, 0x00534B70 .., else the node's own, 0x00534960 ..); its place in the list.
    int listNode(render::Node* node, bool parent);
    void setTurnRates(int index, float x, float y, float z);
    int listedCount() const { return static_cast<int>(m_listed.size()); }
    render::Node* listedNode(int index) { return m_listed[static_cast<size_t>(index)].node; }
    // Every listed node taken out of the list (the car view's 0x0045ACE0).
    void clearListed() { m_listed.clear(); }
    // What the element's world draws besides its models, in the order it was loaded into the world.
    std::vector<std::function<void(render::Renderer3d&, render::Camera&)>> worldDraws;

    // The rig's pitch and yaw (+0x160, +0x164) and its limits (+0x1A4 .. +0x1B0; 9999 none).
    float rig[4] = {};
    float yawLimits[2] = {9999.0f, 9999.0f};
    float pitchLimits[2] = {-0.628f, -0.1f};
    float autoSpeed = -0.1f;  // +0x190: the map mode turns the rig by this × the frame time
    float inputDelay = 0.0f;  // +0x198: the seconds before input moves the rig
    // The interface manager's copy of the first player's window event (0x005A6AB0 each frame, +0x114 ..): the stick
    // its AXIS2_X and AXIS2_Y (+0x124, +0x128; the keyboard's NUMPAD4 / 6 and NUMPAD2 / 8) and the pad direction its
    // scroll (+0x118: 1 up .. 4 right). One for the views of every window, as the manager holds it.
    static inline float stick[2] = {};
    static inline uint32_t pad = 0;

private:
    void moveCamera(float dt);  // the element's draw before the view (0x005B4870)
    void moveRig(float dt);     // the map mode's part of 0x005B4870 (0x005B4A0E)
    void turnCamera();
    void drawView(float x0, float y0, float x1, float y1);  // 0x00511CA0

    Engine& m_engine;
    render::Camera m_camera;
    render::Node m_pivot;  // +0xD0
    render::Node m_arm;    // +0xD4
    World m_world;
    std::unique_ptr<Model> m_model;
    std::vector<std::unique_ptr<Model>> m_models;  // +0x118
    // The world's lens flares (+0x120): the suns buildSky adds, one each time, and their records.
    LensFlares m_flares;
    std::vector<std::unique_ptr<SceneFlare>> m_suns;
    struct Listed {
        render::Node* node = nullptr;
        float rates[3] = {};
        bool parent = false;
    };
    std::vector<Listed> m_listed;  // +0x118, the nodes the code lists
    MoveMode m_mode = MoveMode::ModelAuto;  // +0x128
    bool m_snap = false;           // +0x12C: the next frame takes the angles at once
    bool m_round = false;          // the view's flag 0x10 (0x005B36D0): its corners masked off (0x00516450)
    void roundMask(const render::Viewport& viewport);  // 0x00516450
    oj::Vec4 m_target{};       // +0x130: where the camera goes
    float m_targetAngles[4] = {};  // +0x140: pitch, yaw, roll toward the look-at point
    float m_angles[4] = {};        // +0x150
    oj::Vec4 m_lookAt{};       // +0x170
    float m_speed[2] = {};         // +0x180, +0x184
    float m_zoom = 1.0f;           // +0x188
    float m_zoomTarget = 1.0f;     // +0x18C
    float m_pause = 0.0f;          // +0x194
    float m_fade = 0.0f;           // +0x19C
    float m_fadeTarget = 0.0f;     // +0x1A0
};

}  // namespace oj::scene
