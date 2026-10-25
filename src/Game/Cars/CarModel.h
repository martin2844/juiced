#pragma once
// The 3D model of a car (docs/spec/06-cars.md, "The car's model"): the node tree of cars\<car>\models\<car>_stock.boh
// with its slots filled by the meshes of the fitted parts, the wheels and the driver, and one body material that
// carries the car's paint.

#include "Core/Cars/Car.h"
#include "Core/Cars/CarDamage.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Base/Random.h"
#include "Core/Tracks/SceneObjects.h"
#include "Game/Scene/Scene3d.h"

#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace oj::scene {

// A lens flare of a light node of the car's model (0x00421D50 through the race's scene objects): it takes the light
// node's place under its parent, so it moves with the car; on (+0xA4) as it is made.
struct CarFlare {
    SceneFlare def;
    render::Node* node = nullptr;
    bool on = true;
};

class CarModel {
public:
    // The slots the parts' meshes go to (the table 0x00717538, 0x44 bytes each; the model object's +0x30 ..): each the
    // first node below the top node whose name contains the slot's (0x00534EC0, case sensitive: the 206's EXHAUST_S33,
    // the MR-S's MRS_DECAL), a slot only when that node is a mesh node.
    enum Slot : size_t {
        kFrontBumper,     // FBUMP
        kRearBumper,      // RBUMP
        kBonnet,          // BONNET
        kSpoiler,         // SPOILER
        kSkirtLeft,       // SSKIRT_L
        kSkirtRight,      // SSKIRT_R
        kExhaustCentre,   // EXHAUST_C
        kExhaustSide,     // EXHAUST_S
        kExhaustOpposite, // EXHAUST_O
        kDecal,           // DECAL
        kDriver,          // DRIVER
        kMirrorLeft,      // WMIR_L: the fixed build's (the stock has no slot for the wing mirrors)
        kMirrorRight,     // WMIR_R
        kSlotCount
    };
    // The folders a mesh is read from. Car: the car's own (the stock pushes them, 0x0040D5C0, for the model's nodes,
    // the body parts and the standard wheels): <car>\mesh\, then carmesh\, then mesh\, the car's mats\ and textures\
    // first. Shared: the cars' shared folders every load of cars has in use (0x00498780) and the stock reads the
    // exhaust, the catalogue's wheels and the driver from: carmesh\, carmesh\mats\, cartextures\. Most cars' own
    // mesh\ folders keep older exhausts under the same names (the NSX's exhaust_s8 has both pipes, 119 cm apart).
    enum class Folders { Car, Shared };

    // The race's model (cars\<car>\) or the interface's (cars\<car>_ui\). `random`: the generator of the thread that
    // makes it (four values per mesh node).
    static std::unique_ptr<CarModel> load(Engine& engine, const Car& car, const PartCatalogue& catalogue,
                                          CrtRandom& random, bool interface);
    ~CarModel();

    render::Node* root() { return m_model ? m_model->root : nullptr; }
    // The CarDef's name the model was loaded for.
    const std::string& name() const { return m_name; }
    // Every mesh node, in the tree's order.
    void draw(render::Renderer3d& r, render::Camera& camera, uint32_t mode);

    // 0x0048BF90: the body material's paint from the car: Car Colour the base colour, Metallic Colour, Metallic
    // Shininess 30 - the metallic amount x 25, Pearlescent Colour, Pearlescent Shininess the pearl amount x 6 + 0.01.
    void paint(const Car& car);
    // 0x0048C440: with the car's wheel colour set (+0x40) the wheel's materials (+0x11C .., none for the standard
    // wheels) take its colour (+0x3C) as their colour (parameter 0).
    void colourWheels(const Car& car);
    // 0x0048C530: the wheel's materials' colours back to those kept (+0x134 ..).
    void restoreWheelColours();
    // 0x0048C5D0: the car's wheel colour (+0x3C) kept for each of the wheel's materials (+0x134 ..).
    void keepWheelColours(const Car& car);
    // The wheel's materials (+0x11C ..: the wheel's, _BS, _B, _LOD, _BL; none for a name not found) and their
    // colours kept (+0x134 ..).
    const render::Material* wheelMaterial(size_t i) const { return i < 5 ? m_wheelMaterials[i] : nullptr; }
    uint32_t keptWheelColour(size_t i) const { return i < 5 ? m_wheelColours[i] : 0xFFFFFFFF; }
    // 0x0048C040: the glass material (<car>_GLASS, the model object's +0xF4) black (Colour, parameter 5) with the
    // car's tint (+0x44) as its parameter 2.
    void tint(const Car& car);
    // 0x0048BEF0: with a DECAL slot and a decal part on the car (+0x0C, not none or 0x7F) the material named from
    // "DECAL" on in the part's mesh name (the model object's +0x130) takes the car's decal colour (+0x38) as its
    // colour (parameter 0) when it is of Coloured Spec ABlend (28).
    void colourDecal(const Car& car);
    // 0x0048C210 (0x0048C090): the parts of the primer material (the CarDef's +0x19C, <car>_PRIMER, found by its
    // name) in the meshes of the model's slots (+0x30 .. +0x58) take the body material (+0xEC). Stock problem: a
    // mesh not in a slot keeps the primer (the S2000's chassis, the Golf's farthest logo: the primer's grey). The fixed
    // build turns the primer's parts of every mesh of the model.
    void primerToBody();
    // 0x0048B440 and 0x0048B690 (the model's parts and lights again): with a SPONS node the materials decal_spons_1
    // .. 9 (the model object's +0xF8 ..) take alpha2 of cartextures; with the car's sponsor decals shown (+0x48) each
    // one whose kind of catalogue part the car has (+0x0E ..) takes decal_<the part's supplier> of cartextures and, of
    // Coloured Spec ABlend (28), the car's sponsor colour (+0x4C).
    void sponsorDecals(const Car& car, const PartCatalogue& catalogue);
    // The body parts, the wheels and the exhaust of the car in the model's slots (as the model is made); the body
    // parts' indices fitted written back to the record (0x0048BC30).
    void fitParts(Car& car, const PartCatalogue& catalogue);
    // The same after the car's parts changed (0x0045A8A0): each body part as 0x0048BC30 takes the record's value, a
    // category of no parts passed over: -1, 0x7F or one past the list name no part (0x00447690), its slots emptied
    // (a decal taken off goes); then the wheels and the exhaust.
    void refitParts(Car& car, const PartCatalogue& catalogue);

    // The wheels' mesh in the four wheel slots (0x0048ABC0, 0x0048B3C0); its blurred mesh <mesh>_BLUR, else
    // RX7_WHEEL_BLUR, kept for the race's fast wheels (the model object's +0x28, +0x2C). The CarDef's standard wheels
    // are read from the car's folders, a catalogue wheel from the shared ones. The wheel's materials let go
    // (0x0048AE42); a catalogue wheel's found by name (+0x11C ..: the mesh's name, with _BS, _B and _LOD, and its
    // "_BL" one: the name up to its first '_', "_BL", the rest from that '_', cut at the last '_'; WHEEL_BBS_VS1_17
    // gives WHEEL_BL_BBS_VS1), each with its file's colour (its description's first value, 0x0048AED8) kept
    // (+0x134 ..; -1 for a name not found) and put back on it. The standard wheels have none: the wheel colour does
    // not reach them.
    void fillWheels(const std::string& meshName, Folders folders);
    // The wheel slot of a race wheel (0 WHEEL_FL, 1 WHEEL_FR, 2 WHEEL_RL, 3 WHEEL_RR).
    render::ModelNode* wheelNode(size_t i);
    // The mesh in a slot (none for a slot the model has not or an empty one).
    const render::Mesh* slotMesh(Slot slot) const { return m_slots[slot] != nullptr ? m_slots[slot]->mesh : nullptr; }
    // 0x00555290 on a wheel slot: the wheel's mesh, or its blurred one.
    void blurWheel(size_t i, bool blurred);
    // A body part's mesh in its category's slots (0x0048BC30): the first four categories theirs, the skirt SSKIRT_L and
    // SSKIRT_R as <mesh>_L and <mesh>_R, the decal DECAL; the others none (the fixed build's wing mirrors, 7, WMIR_L
    // and WMIR_R as the skirt).
    void fillBody(int category, const std::string& meshName);
    // The wheels and the exhaust of fitParts and refitParts.
    void fitWheelsAndExhaust(const Car& car, const PartCatalogue& catalogue);

    // The exhaust's mesh, read from the shared folders, in the slots the car has of EXHAUST_S, EXHAUST_C and EXHAUST_O
    // (0x0048BB00 with the slots 7, 6 and 8; the model object's +0x4C, +0x48, +0x50); none empties them.
    void fillExhaust(const std::string& meshName);

    // The mirror image of the interface (0x00459070, 0x00458EB0): every mesh node mirrored (+0xC0 | 1), the top
    // node's Y axis and height negated.
    void mirror();
    // The meshes as they were read, the dents gone (the model's meshes made again, 0x00489440).
    void undent();
    // 0x00556AE0 then 0x005348E0: a mesh node of a mesh (its four values as it drew them) made a child of a node of
    // the model, drawn with the model after that node's own (the tree's order).
    render::ModelNode* addMeshNode(render::Node& parent, render::Mesh* mesh, const Vec4& random);
    // The mesh nodes at or below a node drawn (a world whose root is that node, 0x0054C4D0).
    void drawBelow(const render::Node* top, render::Renderer3d& r, render::Camera& camera, uint32_t mode);

    // The race car's node hidden (+0x90, 0x00489370): the player's car under a view on it.
    bool hidden = false;

    // The vertices of a node for the damage grid (0x00555E30): the node found as the game finds one by name
    // (0x00534EC0: depth first from the top node's children, the first whose name contains the text), each vertex's
    // position (the first three floats of every part's vertices) plus the node's offset, with its place in the
    // mesh's vertices that the dents move. None without such a node, none listed when it has no mesh.
    std::optional<std::vector<DamagePoint>> damagePoints(const char* text);
    // Whether a node is found by its name as damagePoints finds it.
    bool hasNode(const char* text) const;
    // The sphere radius of the node found so (its mesh's; 0 without a mesh or the node).
    float nodeRadius(const char* text) const;
    // The mesh node of the node found so (none without a mesh or the node).
    render::ModelNode* meshNode(const char* text);
    // The node found so (none without it).
    render::Node* node(const char* text);
    // A node's own name (+0x7C), none for a node not of the model.
    const std::string* nameOf(const render::Node* node) const;
    // The flares of the light nodes with LIGHTFLARE, in the tree's order.
    const std::vector<std::unique_ptr<CarFlare>>& flares() const { return m_flares; }
    // The flare of the node found so (none when that node has no flare).
    CarFlare* flare(const char* text);

private:
    CarModel(Engine& engine) : m_engine(engine) {}
    // A mesh of the folders with its materials; the parts of the body's material take the car's body material (the
    // material manager's one of that name, 0x0048A404).
    render::Mesh* mesh(const std::string& name, Folders folders, bool quiet = false);
    // The slot found by a name as the stock finds one (0x00534EC0 through meshNode, a mesh node alone).
    render::ModelNode* slotNode(const char* name);
    // A slot's mesh (none empties it).
    void fill(Slot slot, const std::string& meshName, Folders folders);

    Engine& m_engine;
    std::string m_name;               // the CarDef's name ("3000gt")
    std::filesystem::path m_folder;   // cars\<car> or cars\<car>_ui
    std::unique_ptr<Model> m_model;
    // Every node below a top node, depth first, with its name; the mesh nodes also as such.
    std::vector<std::tuple<std::string, render::Node*, render::ModelNode*>> m_tree;
    std::array<render::ModelNode*, kSlotCount> m_slots{};  // +0x30 .. +0x58
    std::array<render::ModelNode*, 4> m_wheels{};          // +0x10 .. +0x1C: WHEEL_FL, WHEEL_FR, WHEEL_RL, WHEEL_RR
    // The vertices of each mesh's parts as the dents move them (Mesh::parts' order); a part that changed goes to its
    // vertex buffer as the model is drawn (the stock writes the locked buffer itself, 0x005556C0).
    std::map<const render::Mesh*, std::vector<DamageVertices>> m_vertices;
    std::vector<std::unique_ptr<render::Mesh>> m_meshes;
    std::vector<std::unique_ptr<render::Material>> m_materials;
    std::vector<std::unique_ptr<CarFlare>> m_flares;
    render::Material* m_body = nullptr;  // +0xEC of the car's model object
    render::Mesh* m_wheelMesh = nullptr;      // +0x20, +0x24: front and rear (the same here)
    render::Mesh* m_wheelBlurMesh = nullptr;  // +0x28, +0x2C
    std::map<std::string, render::Material*> m_partMaterials;  // the materials by their parts' names (lower case)
    std::array<render::Material*, 5> m_wheelMaterials{};  // +0x11C .. +0x12C: the wheel's, _BS, _B, _LOD and _BL
    std::array<uint32_t, 5> m_wheelColours{};             // +0x134 .. +0x144: their colours kept
};

}  // namespace oj::scene
