#pragma once
// The physics definition of a car, scripts\dynamic_<car>.txt (docs/spec/07-dynamics.md): the body, the engine, the
// gears and the wheels as the car definition (0x188 bytes, 0x004F3A20) holds them.

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oj {

struct Ocd;

// [BeginWheel] .. [EndWheel]: the 13-dword record the game copies into the car definition (0x004F4100).
struct WheelDef {
    int32_t id = 0;                  // +0x00 WheelId
    float radius = 0.0f;             // +0x04 WheelRadius
    float finalDrive = 0.0f;         // +0x08 FinalDrive
    float finalDriveInverse = 0.0f;  // +0x0C 1 / FinalDrive, 0 for 0
    float inertia = 0.0f;            // +0x10 WheelInertia
    float inertiaInverse = 0.0f;     // +0x14
    float width = 0.0f;              // +0x18 Width
    float maxSteerAngle = 0.0f;      // +0x1C MaxSteerAngle
    float brakeMu = 0.0f;            // +0x20 BrakeMu
    float handbrakeMu = 0.0f;        // +0x24 HandbrakeMu
    float axleMu = 0.0f;             // +0x28 AxleMu
    uint32_t drive = 0;              // +0x2C Drive: 1 when driven
    float baseHeight = 0.0f;         // +0x30 BaseHeight
};

// [BeginAirfoil] .. [EndAirfoil] (0x004EF2C0): a force that grows with the square of the body's speed.
struct Airfoil {
    std::array<float, 4> position{};  // +0x00: Position - CentreOfGravity at the [EndAirfoil]
    std::array<float, 4> force{};     // +0x10: Force, the force at a speed of 1 m/s
    float drag = 0.0f;                // +0x20: DragCoefficient of the block, added to the body's
};

// [EndLineCollider] (0x00536DA0, 0x0049F9B0): a segment of the model (centimetres) that finds the ground, a wheel's.
struct LineCollider {
    std::array<float, 4> point{};      // +0x00 Point
    std::array<float, 4> direction{};  // +0x10: End - Point, divided by its length + 1.1920929e-7
    float length = 0.0f;               // +0x20: |End - Point|
    int32_t id = 0;                    // +0x24 ID
};

// [EndClippingPlane] (0x00536CA0, pushed by 0x004C7DF0; 0x20 bytes): a plane of the model (centimetres) by which the
// faces are cut before the sphere colliders test them (0x00538310): what lies beyond its normal is cut off.
struct ClippingPlane {
    std::array<float, 4> point{};   // +0x00 Point
    std::array<float, 4> normal{};  // +0x10 Normal
};

// [EndSphereCollider] (0x00536D00, 0x24 bytes; the list grows by 0x00537CF0 / 0x00537FF0 / 0x00538030): a sphere of
// the model (centimetres) that hits the world and the other bodies.
struct SphereCollider {
    std::array<float, 4> point{};  // +0x00 Point
    float radius = 0.0f;           // +0x10 Radius
    int32_t id = 0;                // +0x20 ID
};

// [EndSphereCollidee] (0x00536DE0 -> 0x00537CA0, 0x2C bytes; grows by 0x00537EF0 / 0x00538130 / 0x00538180): a
// sphere of the model that the other bodies hit.
struct SphereCollidee {
    std::array<float, 4> point{};  // +0x00 Point
    float radius = 0.0f;           // +0x10 Radius
    int32_t surface = 0;           // +0x20 SurfaceType
    float mu = 0.0f;               // +0x24 Mu
    float restitution = 0.0f;      // +0x28 Restitution
};

// The collision object part of a definition (0x00535F20, 0xAC bytes): the shapes the body collides with.
struct CollisionDef {
    std::string name;                     // +0x08 Name (63 characters)
    int32_t maxCollidees = 0;             // +0xA0 MaxCollidees
    bool primary = false;                 // +0xA4 Primary
    std::vector<ClippingPlane> clippingPlanes;     // +0x48
    std::vector<SphereCollider> sphereColliders;  // +0x58
    std::vector<LineCollider> lines;      // +0x68
    std::vector<SphereCollidee> sphereCollidees;  // +0x78
    int32_t meshColliderId = 0;           // +0x8C: [EndMeshCollider]'s ID
    std::string meshCollider;             // +0x88: its File (.ocd), the shape that hits the world
    int32_t meshCollideeSurface = 0;      // +0x94: [EndMeshCollidee]'s SurfaceType
    float meshCollideeMu = 0.0f;          // +0x98 Mu
    float meshCollideeRestitution = 0.0f; // +0x9C Restitution
    std::string meshCollidee;             // +0x90: its File (.ocd), the shape the world hits
    std::shared_ptr<const Ocd> colliderShape;  // the meshes of the two files, once read
    std::shared_ptr<const Ocd> collideeShape;
};

// [End..Contact]: how a collider's touch with the world pushes the body (0x3C / 0x4C / 0x74 bytes, vtable
// 0x006F6CF0; made by the definition's 0x004EF350 / 0x004F41B0).
struct ContactDef {
    enum Type : int32_t { Rigid = 0, Spring = 1, Tyre = 2 };
    int32_t id = 0;                          // +0x04 ContactId: the collider's ID
    bool clipped = false;                    // +0x08: ClippingPlane given (NoClippingPlane clears it)
    std::array<float, 4> clipPoint{};        // +0x0C
    std::array<float, 4> clipNormal{};       // +0x1C
    float maxCompression = 0.0f;             // +0x2C MaxCompression
    float mu = 0.0f;                         // +0x30 Mu
    float restitution = 0.0f;                // +0x34 Restitution
    Type type = Rigid;                       // +0x38
    float stiffness = 0.0f;                  // +0x3C: LocalGravity x Stiffness
    float dampingRatio = 0.0f;               // +0x40 DampingRatio
    float mass = 0.0f;                       // +0x44: Mass x WeightDistribution; a tyre's from the front weight
    float damping = 0.0f;                    // +0x48: 2 sqrt(mass x stiffness) x DampingRatio
    float lateralStiffness = 0.0f;           // +0x4C LateralStiffness
    float longitudinalStiffness = 0.0f;      // +0x50 LongitudinalStiffness
    float lateralMu = 0.0f;                  // +0x54 LateralMu
    float longitudinalMu = 0.0f;             // +0x58 LongitudinalMu
    std::array<float, 4> magic{};            // +0x5C MagicFormulaB, C, D, E
    float eOverB = 0.0f;                     // +0x6C: E / B, 0 for B = 0
    float muRatio = 0.0f;                    // +0x70: LongitudinalMu x LateralStiffness / (LongitudinalStiffness x
                                             //        LateralMu), 0 for a divisor 0
};

// The dynamic object part of a definition (0x004EEE20, its first 0x104 bytes).
struct BodyDef {
    CollisionDef collision;                   // +0x00 .. +0xAC
    float mass = 0.0f;                        // +0xAC Mass (kg)
    std::array<float, 4> inertia{};           // +0xB0 Inertia: about x, y, z, per kilogram
    std::array<float, 4> centreOfGravity{};   // +0xC0 CentreOfGravity: in the model, in metres
    float localGravity = 1.0f;                // +0xD0 LocalGravity: the factor of the gravity
    float drag = 1.55f;                       // +0xD4 DragCoefficient
    bool sleepSet = false;                    // +0xD8: Sleep read
    bool sleep = false;                       // +0xDC Sleep <on> <value>
    float sleepValue = 0.0f;                  // +0xE0
    std::vector<Airfoil> airfoils;            // +0xE4
    std::vector<ContactDef> contacts;         // +0xF4
};

// 0x004EF2C0: an airfoil at a point of the model (metres) with its force and drag.
void addAirfoil(BodyDef& body, const std::array<float, 4>& position, const std::array<float, 4>& force, float drag);

struct CarDynamics {
    BodyDef body;                      // +0x00 .. +0x104
    std::vector<float> torqueSpeeds;   // +0x104 TorqueCurveX, in rad/s (rpm x 0.10471976)
    std::vector<float> torqueFactors;  // +0x114 TorqueCurveY
    float maxTorque = 0.0f;            // +0x124
    float reverseRatio = 0.0f;         // +0x128 ReverseGearRatio x 0.6
    std::vector<float> forwardRatios;  // +0x12C ForwardGearRatios
    std::vector<WheelDef> wheels;      // +0x13C, in the order of their [EndWheel]
    float engineInertia = 0.0f;        // +0x14C
    float clutchSpeed = 0.0f;          // +0x150
    float steeringFactor = 0.0f;       // +0x154
    float steeringSpeed = 1.0f;        // +0x158
    float powerRangeMin = 0.0f;        // +0x15C
    float powerRangeMax = 0.0f;        // +0x160
    float turbo = 1.0f;                // +0x164
    float nitro = 1.0f;                // +0x168
    float nitroTime = 0.0f;            // +0x16C
    bool abs = false;                  // +0x170
    bool asr = false;                  // +0x174
    float asrFactor = 1.0f;            // +0x178
    float timeFactor = 1.0f;           // +0x17C
    float topSpeed = 0.0f;             // +0x180: the last TorqueCurveX point (rad/s)
    float frontWeightDistribution = 0.5f;  // +0x184
};

// The car of the file (the last one when it holds several); none without a [BeginCar].
std::optional<CarDynamics> parseDynamics(std::string_view text);

// What a dynamics file's object is (the reader's +0xA8): a collision object ([BeginCollisionObject], 0x00535F20: the
// collision part only), a dynamic object ([BeginDynamicObject], 0x004EEE20; a rigid body, 0x004F2060), a car
// ([BeginCar], 0x004F3A20: its body part here).
enum class ObjectKind : int32_t { Collision = 0, Dynamic = 1, Car = 2 };

// The definition a file's reader makes last (0x004EFF30: the reader's +0x68) with its kind; none without any.
struct ObjectDynamics {
    ObjectKind kind = ObjectKind::Dynamic;
    BodyDef body;
};
std::optional<ObjectDynamics> parseObjectDynamics(std::string_view text);

// x87 at single precision: a double times a float, rounded once to a float in the current rounding mode.
float productToFloat(double a, float b);

}  // namespace oj
