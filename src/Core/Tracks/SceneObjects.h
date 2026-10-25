#pragma once
// The scene objects of a race (docs/spec/21-scene-objects.md): the objects of the route's .dlh files as the race's
// load makes them (the scene objects' manager, RaceTrack +0x14 = [0x0075ECDC], 0x004208E0) — the nodes, the objects
// the names make, the replay cameras, the splines' riders — with the loading thread's draws in the stock order.

#include "Core/Cars/CarDamage.h"
#include "Core/Formats/Dlh.h"
#include "Core/Physics/Dynamics.h"
#include "Core/Data/EffectDefs.h"
#include "Core/Base/Random.h"
#include "Core/Physics/RigidBody.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oj {

class GameFiles;

// The properties of an object (0x0042A070): "KEY = value" pairs of its 3ds Max text; the keys by the table of 42
// names (0x00715960, an unknown key 0), a value of digits, '.' and '-' only a number, else a text.
class ObjectProperties {
public:
    enum Key : int32_t {
        Unknown = 0, Physics = 1, RotXMin = 2, RotXMax = 3, RotYMin = 4, RotYMax = 5, RotZMin = 6, RotZMax = 7,
        SpeedMin = 8, SpeedMax = 9, SpeedMatch = 10, CsplineOn = 11, CsplineOff = 12, CamZoomClose = 13,
        CamZoomWide = 14, CamZoomAuto = 15, LightFlare = 16, LightFlareYReflection = 17, LightFlareSize = 18,
        LightFlareBrightness = 19, LightFlareFalloffMin = 20, LightFlareFalloffMax = 21, LightFlareOffset = 22,
        LightFlareYReflectSizeX = 23, LightFlareYReflectSizeY = 24, LightFlareYReflectAlphaTop = 25,
        LightFlareYReflectAlphaBottom = 26, LightFlareReflection = 27, LightFlareReflectionSize = 28,
        LightFlareDryYReflection = 29, LightFlareDryYReflectSizeX = 30, LightFlareDryYReflectSizeY = 31,
        LightFlareDryYReflectAlphaTop = 32, LightFlareDryYReflectAlphaBottom = 33,
        CruiseMessage = 34, CruiseMove = 35, CruiseName = 36,
        CruiseDifficulty = 37, CruiseCrowd = 38, NodAng = 39, EvenSpacing = 40, TimeOfDay = 41,
    };
    struct Entry {
        int32_t key = 0;
        bool isText = false;
        std::string text;
        float number = 0.0f;
    };

    explicit ObjectProperties(std::string_view text);

    // 0x0042A510, 0x0042A550: the first entry of the key.
    bool has(int32_t key) const { return entry(key) != nullptr; }
    const Entry* entry(int32_t key) const;
    // 0x0042A5B0: its number, 0 without the entry; 0x0042A590: its text, none without it.
    float number(int32_t key) const;
    const std::string* text(int32_t key) const;
    // 0x0042A5E0: its number cut toward zero.
    int32_t integer(int32_t key) const;

    const std::vector<Entry>& entries() const { return m_entries; }

private:
    std::vector<Entry> m_entries;
};

// The node types of the scene's nodes (+0x8C): a mesh node 0, a light 2, a dummy or an effect 4, a helper 5, a
// spline 6.
enum class SceneNodeType : int32_t { Mesh = 0, Light = 2, Dummy = 4, Helper = 5, Spline = 6 };

// What the race's load keeps of a scene object made from a node.
struct SceneObject {
    enum class Kind { Generic, Wheel, Crowd, Bin, Barrel, CarPart };
    Kind kind = Kind::Generic;
    std::string name;
    std::array<float, 16> matrix{};
    // 0x0041A9A0: the turning animation (ROTX/Y/Z_MIN/MAX, NOD_ANG): which axes (+0x5C), their rates (+0x6C ..
    // +0x74) and the nod (+0x78); the angles turned so far (+0x60 .. +0x68).
    uint32_t axes = 0;
    std::array<float, 3> rates{};
    float nod = 0.0f;
    std::array<float, 3> angles{};
    size_t node = SIZE_MAX;  // +0x08
    // A crowd member's (0x0041D870): the most its turn moves in a step (+0xBC) and the steps to its next look for the
    // nearest car (+0xC0, then every 20 steps; 0x0041DC00). Its facing in sixteenths of a turn, read by nothing found:
    // its own along the centre line (+0xB0, 0x0041D9D0), the turn (+0xB4) eased toward the target (+0xB8).
    float crowdTurnStep = 0.0f;
    int32_t crowdCountdown = 0;
    float crowdHeading = 0.0f;
    float crowdTurn = 0.0f;
    float crowdTarget = 0.0f;
    std::string physics;  // PHYSICS: dynamic_<name>
};

// A node of the scene's .dlh files as the load made it (0x0054F260): its object, the file's world matrix, a mesh
// node's four values (0x005550D0) and whether it is hidden (+0xC0 bit 4), its children.
struct SceneNode {
    uint32_t file = 0;        // the .dlh's number in the load's order
    uint32_t object = 0;      // its object in the file
    bool mesh = false;        // a model node (type 0)
    std::array<float, 16> matrix{};
    Vec4 random{};
    bool hidden = false;
    std::vector<size_t> children;
    // A spline node's sphere around its knots (+0x64 .. +0x74, 0x00540320); a model node's is its mesh's.
    bool hasSphere = false;
    Vec4 sphereCentre{};
    float sphereRadius = 0.0f;
    // A rider's matrix in its spline's space, set where it starts (0x00423B80), in place of the file's.
    bool placed = false;
    Mat34 local = identity34();
    // A turning object's turn (0x0041B050): the node's own matrix (as the file left it) times it.
    bool turning = false;
    Mat34 turn = identity34();
};

// A lens flare of a node with LIGHTFLARE (0x00421D50 → 0x005428D0, 0x00541030; docs/spec/21-scene-objects.md): made
// at the node's place as the load left it, its textures by name and what the main view draws it by.
struct SceneFlare {
    size_t node = 0;          // the node it was made from
    bool tilted = false;      // a car_flare01: turned about its x by -0.0872665 (0x00534A10)
    std::string texture;      // LIGHTFLARE
    std::string yReflection;  // LIGHTFLARE_YREFLECTION, or in the dry LIGHTFLARE_DRYYREFLECTION when given
    std::string reflection;   // LIGHTFLARE_REFLECTION
    float size = 100.0f;      // +0x1CC: LIGHTFLARE_SIZE
    float cosMin = 0.0f;      // +0x1D4: cos(LIGHTFLARE_FALLOFF_MIN, 2, x 0.017452778)
    float cosMax = 0.0f;      // +0x1D8: cos(LIGHTFLARE_FALLOFF_MAX, 8)
    float offset = 20.0f;     // +0x1E8: LIGHTFLARE_OFFSET, the visibility test's point along -y
    // The road reflection's size across and up (+0x1B4, +0x1B8) and its alphas at the top and the bottom (+0x1BC,
    // +0x1C0): LIGHTFLARE_YREFLECT_SIZE_X, _SIZE_Y, _ALPHA_TOP, _ALPHA_BOTTOM, in the dry each DRYYREFLECT key when
    // given; the reflection's size on the cars (+0x1C4, LIGHTFLARE_REFLECTION_SIZE). Stock problem: a key not given
    // leaves a value of the stack (0x00421D50's record); 0 here.
    float roadWidth = 0.0f;
    float roadHeight = 0.0f;
    float roadAlphaTop = 0.0f;
    float roadAlphaBottom = 0.0f;
    float reflectionSize = 0.0f;
    // The race's sun (0x00425425): placed each frame this far from the camera toward the sun (+0x1DC, 0 for a node's
    // flare), the sun's height (+0x1E0, the atmosphere's SunAngle).
    float screen = 0.0f;
    float sunAngle = 0.0f;
};

// 0x00421D50's LIGHTFLARE (a node of the scene or of a car's model): the flare of the node's keys (its node left to
// the caller); none without LIGHTFLARE.
std::optional<SceneFlare> lightFlare(const ObjectProperties& props, bool wet);

// A physics object of a node with PHYSICS (0x00421960): the body of its profile's definition, a copy made for it
// (0x004EEF00, 0x00536040), at the place of the node.
struct ScenePhysics {
    std::string profile;
    ObjectKind kind = ObjectKind::Dynamic;  // the definition's
    size_t object = SIZE_MAX;  // the object made for the node (+0x58 its body), SIZE_MAX for none
    size_t node = SIZE_MAX;    // the node the body moves (nodes())
    std::unique_ptr<BodyDef> def;
    std::unique_ptr<RigidBody> body;
};

// The body of a car's part (0x004191C0 -> 0x00421960 with its last argument 0): made as a scene object's is from its
// profile, but it never joins the world; the part keeps it (+0x58) until the part goes (0x0041ADA0). Nothing in the PC
// game reads it.
struct PartPhysics {
    std::string profile;
    ObjectKind kind = ObjectKind::Dynamic;
    std::unique_ptr<BodyDef> def;
    std::unique_ptr<RigidBody> body;
};

// The scene object of a car's chassis or part (0x004191C0, 0xF8 bytes; made by 0x00420FF0 for a model node whose name
// holds FBUMP, RBUMP, BONNET, SPOILER, SSKIRT, HEADLIGHT, REARLIGHT, WMIR, DECAL, EXHAUST or CHASS; the parts' vtable
// 0x006EEDE8, the chassis's 0x006EF0F0, both stepping with 0x00419780): how it wobbles on the car as the dents of its
// damage zone add up, and the bit of the model it would clear once put back on (nothing in the PC game takes one off).
struct CarPartObject {
    // 0x004191C0 by the node's name (+0x7C, compared without case): "fbump" and "rbump" wobble about every axis (7),
    // "bonnet" and "spoiler" about x (1), a name starting "sskirt" about x and z (5), one starting "exhaust" about x
    // and y (3) and named "exhaust" for its files; any other name not at all, and has no files and no body.
    // Stock problem: the skirts are told apart by _strnicmp with "sskirt_l" and "sskirt_r" over 6 characters, so the
    // right skirt takes the left's bit (0x10, never 0x20); the fixed build's 0x20.
    static CarPartObject make(const std::string& nodeName);

    uint32_t axes = 0;              // +0x5C: 1 about x, 2 about y, 4 about z
    std::array<float, 3> angles{};  // +0x60 .. +0x68: the wobble's turn about x, y and z
    float softness = 1.2f;          // +0xE0: the spoiler's and the exhausts' 0.5
    float phase = 0.0f;             // +0xE4
    float rate = 4.0f;              // +0xE8
    float amount = 0.0003f;         // +0xEC: 0x399D4952, the rear bumper's 0x3951B717
    uint8_t bit = 0;                // +0xF4: its bit of the model's +0x170 (fbump 1, rbump 2, bonnet 4, spoiler 8, skirts
                                    // 0x10)
    bool attached = true;           // +0xD8: 1 from the constructor and 0x00419730; nothing clears it
    std::string files;              // the name its files take: the node's, "exhaust" for an exhaust

    // 0x00419780 on an attached part with axes, before its light step (0x0041B270), with that step's last values (the
    // speed +0x24, the change of the velocity across the ground +0x20 and the change of the velocity +0x38): the
    // damage's level is its zone's dents (+0xCC, the zone's +0x10) x 0.05, 0 without a zone; the phase moves on by
    // dt x the rate / (level / softness + 0.0125) (within -pi .. pi); every axis's turn is
    // -(|sin(phase) + sin(phase x 0.3)| x (1 / softness) x level x dt) x amount x speed; with a change across the
    // ground over 20 ([0x00725298]) the change x 0.001 x (1 / softness) x level adds its y to the turn about x, its z
    // about y and its x about z; the turns about its axes are held within +-100 ([0x00725290]) x amount.
    void wobble(float dt, const DamageZone* zone, float speed, float sideways, const Vec4& change);
};

// 0x0041B050: the turn of an object with axes (+0x5C): each axis's angle (+0x60 .. +0x68) turns on by dt x its rate
// (+0x6C .. +0x74), held within -pi .. pi; with a nod (bit 8) the angle used is sin(the angle) x the nod (+0x78) x 0.02;
// the turns about x (0x0041B680), y (0x0041B4E0) and z (0x0041B4A0) in that order. The node's matrix is then its own
// (+0x7C) times this.
Mat34 turnObject(uint32_t axes, std::array<float, 3>& angles, const std::array<float, 3>& rates, float nod, float dt);

struct Atmosphere;

// An atmosphere zone (0x00418B50, 0x3C bytes, the manager's +0x28): an atmos* helper among the roots (0x00422750).
// Its place and radius are its node's slot 1 as it was made: a sphere's (0x005543E0) its centre in the world and its
// radius, a box's (0x00554620) the box's middle in the world (its w the low corner's) and half its diagonal. Its name
// is the helper's from its seventh character, at most 16 (strncpy), cut at the first digit after the first character.
// At the end of the track's making (0x00422AC0) it takes the track's atmosphere of that name (0x00533A70), none
// without one.
struct SceneAtmosZone {
    Vec4 place{};                            // +0x28
    float radius = 0.0f;                     // +0x38
    std::string name;                        // +0x18
    const Atmosphere* atmosphere = nullptr;  // +0x14
};

// The distance of a point from a zone as 0x00422C40 and 0x00418BF0 take it: x's difference a float, the sum
// (z^2 + y^2) + x^2 a float, sqrtf.
float zoneDistance(const SceneAtmosZone& zone, const Vec4& point);

// A Move* helper (0x00421D50 -> 0x004B01D0): a crowd zone of the cruise manager. Its shape is the helper's node
// (0x00553E50 a box, 0x00553F10 a sphere) at the node's world matrix; its properties' text; its children the crowd
// members (0x004B0330).
struct SceneMoveZone {
    std::array<float, 16> matrix{};
    uint32_t shape = 0;  // 1 a box, 2 a sphere
    Vec4 boxMin{};
    Vec4 boxMax{};
    Vec4 centre{};
    float radius = 0.0f;
    std::string properties;
    size_t children = 0;
};

// A helper's shape (its node's slot 0): whether a point (centimetres) is inside: 0x00553E50 a box (the point in the
// node's frame by the inverse of its world matrix, 0x004F36C0, within the box's corners), 0x00553F10 a sphere (the
// point's distance from the centre moved by the world matrix not over the radius); another shape makes no node.
bool helperContains(const Mat34& matrix, uint32_t shape, const Vec4& boxMin, const Vec4& boxMax, const Vec4& centre,
                    float radius, const Vec4& point);

// A replay camera of a camera* node (0x00418EA0).
struct SceneCamera {
    std::string name;
    std::array<float, 16> matrix{};
    int32_t splineOn = 0;   // +0x18: CSPLINE_ON - 1
    int32_t splineOff = 0;  // +0x1C
    float speed = -1.0f;    // +0x20
    float speedMatch = 1.0f;  // +0x24
    int32_t zoomMode = 2;   // +0x28
    float zoom = 1.0f;      // +0x2C
};

// A spline whose children ride along it (0x004236A0) and its riders (0x00423A80).
struct SceneRider {
    std::string name;
    size_t node = 0;          // +0x08: the child node it moves
    int32_t next = 0;         // +0x0C: the path's point it heads for
    float progress = 0.0f;    // +0x14: along the path from the point before
    float speed = 1500.0f;    // +0x18
    Vec4 velocity{};          // +0x1C: the last frame's move (x, y, z)
    float pitch = 0.0f;       // +0x2C
    float yaw = 0.0f;         // +0x30
    float roll = 0.0f;        // +0x34
    bool moving = true;       // +0x3C: an open path's rider stops at its end
    float turnRate = 0.1f;    // +0x40
    float lookAhead = 0.6f;   // +0x44
    Vec4 target{};            // +0x48: the curve's point it steers for
    float place = 0.0f;       // the segment and its fraction it started at (0x00423B80)
    bool banks = true;        // +0x58: cablecar* does not
};
// A segment of a spline node (0x40 bytes, 0x00540630): a knot, its out handle, the next knot's in handle, that knot.
struct SplineSegment {
    Vec4 p0{}, p1{}, p2{}, p3{};
};
// A point of a spline's path (0x0041B720, 0x28 bytes): its place in the world (the knot moved by the node's place),
// the unit direction from the point before it, the length to the next point (the last one's back to the first),
// the length of the path up to it.
struct SplinePathPoint {
    Vec4 position{};
    Vec4 direction{};
    float length = 0.0f;    // +0x20
    float distance = 0.0f;  // +0x24
};
// 0x0041BAA0: the path's point nearest a place (the first of the nearest), 0 with none.
size_t nearestPathPoint(const std::vector<SplinePathPoint>& path, float x, float y, float z);

struct SceneSpline {
    std::string name;
    std::vector<DlhSplinePoint> points;
    bool closed = false;
    std::vector<SplineSegment> segments;   // the node's +0xA4
    std::vector<SplinePathPoint> path;     // +0x08: as many as the segments, one more when open (+0x20)
    bool evenSpacing = false;  // +0x34
    float length = 0.0f;       // +0x38: the sum of the path's lengths
    float speedMin = 0.0f;     // +0x3C
    float speedMax = 0.0f;     // +0x40
    std::vector<SceneRider> riders;  // +0x24
    size_t node = 0;                 // the spline's node (nodes())

    // 0x0053FF50 / 0x00540130: the point at a place (the segment and its fraction) of the node's curve, in the node's
    // space.
    Vec4 pointAt(float place) const;
};

// 0x0041B720: the spline's path (its segments made), the points at the knots moved by the node's place (x, y, z):
// each one's direction from the point before (the first from the last) and that point's length to it, the length of
// the path up to it; the spline's length the points' lengths summed (the fixed build's open spline without the last
// one's way back to the first).
void makeSplinePath(SceneSpline& s, float x, float y, float z);

// The scene objects' manager of a race.
class SceneObjects {
public:
    struct Load {
        CrtRandom* random = nullptr;           // the loading thread's
        const EffectLibrary* effects = nullptr;
        bool sound = false;                    // [0x0075F860]
        uint32_t timeOfDay = 2;                // the race's +0x6C
        const GameFiles* files = nullptr;      // the physics profiles' scripts\<name>.txt (none: no physics)
        bool wet = false;                      // the atmosphere's Wet (+0x174): the flares' reflections
    };

    // 0x0054F520 with the manager as its handler: the file's objects and nodes. For each root, depth first: each
    // node made (a mesh node draws four values, an effect makes its emitter) and handed to the manager (0x00421D50);
    // then the root through the manager's filter (0x00422750: a spline's children ride it).
    void loadDlh(const Dlh& dlh, Load& load);

    // 0x00422AC0 at the end of the race track's making (0x00424270), on the loading thread: each object's slot 3 (the
    // first time: its node's matrix kept as its own, +0x7C); a crowd member (0x0041D9D0) on a route with a centre
    // line ([0x0075ECD0], the first centre_spline* made) faces the line's nearest point with a turn drawn from rand().
    void finishTrack(CrtRandom& random);

    // 0x004B0330, after the race track is made (the cars set): a zone for each child of every Move* helper
    // (0x0041A430: a value drawn for its sound's delay).
    // The values drawn (one a child of each zone, in order), for the sounders' first delays.
    std::vector<int32_t> makeMoveZones(CrtRandom& random) const;

    // 0x00422BB0, each frame of the race after the cars (0x00425D40), with the physics' step time: for its first 50
    // frames with none; each spline's riders move (0x00423D00); on the frames of the game's parity (objectsStep) the
    // objects step: a plain object turns (0x0041B050; a barrel, bin or wheel by 1 / the time, 0x0041B270), a crowd
    // member looks for the nearest car (0x0041DC00; cars: the places of the cars' chassis in the order the cars were
    // made, the centre line's trackers, 0x00419E10).
    void update(float stepTime, bool objectsStep, const std::vector<Vec4>& cars);

    const std::vector<SceneObject>& objects() const { return m_objects; }
    const std::vector<SceneCamera>& cameras() const { return m_cameras; }
    // An env_camera* dummy's place (0x00422183 → 0x00531AE0: the world's +0xAC, the place its reflection cube is
    // drawn from), the last one handed; none without one.
    const std::optional<Vec4>& environmentPlace() const { return m_environmentPlace; }
    const std::vector<SceneSpline>& splines() const { return m_splines; }
    // The nodes of the .dlh files and the roots that joined the world, in the order they were made.
    const std::vector<SceneNode>& nodes() const { return m_nodes; }
    const std::vector<SceneFlare>& flares() const { return m_flares; }
    const std::vector<SceneMoveZone>& moveZones() const { return m_moveZones; }
    const std::vector<SceneAtmosZone>& atmosZones() const { return m_atmosZones; }
    // The race's centre line's path, none without one.
    const std::vector<SplinePathPoint>* centreLine() const { return m_centreLine ? &m_centrePath : nullptr; }
    std::vector<SceneAtmosZone>& atmosZones() { return m_atmosZones; }
    // 0x00422D20: a point inside a tunnel* helper that joined the world (the manager's +0x38).
    bool inTunnel(const Vec4& point) const;
    // 0x00422D60: a point inside a reverb* helper that joined the world (the manager's +0x48).
    bool inReverb(const Vec4& point) const;
    // The time the objects' steps took in the last update (0 for its first 50 frames).
    float stepTime() const { return m_stepTime; }
    const std::vector<size_t>& roots() const { return m_roots; }
    uint32_t files() const { return m_files; }
    // The physics objects in the order they were made; the world takes their bodies (0x0053E450).
    const std::vector<ScenePhysics>& physics() const { return m_physics; }
    std::vector<ScenePhysics>& physics() { return m_physics; }
    // 0x004191C0 for a part with axes: its hulls <files>_collider.ocd and <files>_collidee.ocd of the meshes' folder
    // (0x00535830; either none when it is not there), then 0x00421960 with the profile dynamic_<files> and the last
    // argument 0: the profile read once into the manager's profiles (its first object giving it its hulls), the body
    // made at the node's place in the world, kept by the part and never given to the world. None when the profile is
    // not read (no dynamic_exhaust.txt in the game's files) or is a car's.
    std::optional<PartPhysics> makePartPhysics(const std::string& files, const Mat34& nodeWorld,
                                               const GameFiles* gameFiles);

    size_t meshNodes() const { return m_meshNodes; }
    size_t emitters() const { return m_emitters; }
    // The effects' emitters made, each with its node (0x0054EF60: the emitter's [2] the effect's node).
    struct Emitter {
        size_t node = 0;
        std::string name;
        int32_t soundDraw = -1;  // the value drawn for its looping sound (0x0050CE70), -1 none
    };
    const std::vector<Emitter>& emitterNodes() const { return m_emitterNodes; }

private:
    size_t makeNode(const Dlh& dlh, const DlhNode& node, Load& load);
    void handleNode(const DlhObject& object, const DlhNode& node, SceneNodeType type, size_t record, Load& load);
    // False when the root does not join the world.
    bool filterRoot(const Dlh& dlh, const DlhNode& root, size_t record, Load& load);
    void crowdStep(SceneObject& o, const std::vector<Vec4>& cars) const;
    // 0x00421960: false when the profile is not read.
    bool makePhysics(const std::string& profile, const DlhObject& object, const DlhNode& node, size_t made,
                     size_t record, Load& load);
    // 0x00421960's profile: read once (0x004EFF30) into the manager's profiles by "<profile>.txt", the first object's
    // hulls given to it (each may be none); none when it is not read.
    const ObjectDynamics* profileOf(const std::string& profile, const Ocd* collider, const Ocd* collidee,
                                    const GameFiles* files);

    std::vector<SceneObject> m_objects;  // +0x70
    std::vector<SceneCamera> m_cameras;  // +0x18
    std::optional<Vec4> m_environmentPlace;
    std::vector<SceneSpline> m_splines;  // +0x08, with the node they were made for
    std::map<std::string, std::unique_ptr<ObjectDynamics>> m_profiles;  // +0x60: by "<profile>.txt"
    std::vector<ScenePhysics> m_physics;
    std::vector<SceneNode> m_nodes;
    std::vector<SceneFlare> m_flares;
    std::vector<size_t> m_roots;
    uint32_t m_files = 0;
    uint32_t m_framesBeforeTime = 50;  // +0x5C
    bool m_centreLine = false;         // [0x0075ECD0] set
    std::vector<const DlhNode*> m_splineNodes;
    std::vector<SceneMoveZone> m_moveZones;  // the cruise manager's +0x00, in the order they were made
    std::vector<SceneMoveZone> m_tunnels;    // +0x38
    std::vector<SceneMoveZone> m_reverbs;    // +0x48
    std::vector<SceneAtmosZone> m_atmosZones;  // +0x28
    // The race's centre line ([0x0075ECD0], the first centre_spline* made, 0x00419CC0): its path as a spline's
    // (0x0041B720).
    std::vector<SplinePathPoint> m_centrePath;
    float m_stepTime = 0.0f;
    size_t m_meshNodes = 0;
    size_t m_emitters = 0;
    std::vector<Emitter> m_emitterNodes;
};

}  // namespace oj
