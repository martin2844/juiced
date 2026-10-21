#include "Core/Physics/Dynamics.h"

#include "Core/Base/Script.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

namespace oj {

namespace {

bool same(const std::string& a, const char* b) {
    return _stricmp(a.c_str(), b) == 0;
}

std::vector<float> realList(const ScriptLine& line, float scale = 0.0f) {
    std::vector<float> values;
    for (const std::string& word : line.words) {
        const double v = std::atof(word.c_str());
        values.push_back(scale != 0.0f ? productToFloat(v, scale) : static_cast<float>(v));
    }
    return values;
}

float inverse(float v) {
    return v != 0.0f ? 1.0f / v : 0.0f;
}

// 0x00536610: three numbers, each 0 when missing.
std::array<float, 4> vectorOf(const ScriptLine& line) {
    return {line.real(0), line.real(1), line.real(2), 0.0f};
}

}  // namespace

void addAirfoil(BodyDef& body, const std::array<float, 4>& position, const std::array<float, 4>& force, float drag) {
    Airfoil a;
    a.position = {position[0] - body.centreOfGravity[0], position[1] - body.centreOfGravity[1],
                  position[2] - body.centreOfGravity[2], position[3]};
    a.force = force;
    a.drag = drag;
    body.airfoils.push_back(a);
}

float productToFloat(double a, float b) {
    double p = a * static_cast<double>(b);
    const double e = std::fma(a, static_cast<double>(b), -p);
    if (e != 0.0 && std::isfinite(p)) {
        // Round to odd: a double that is not exact keeps an odd last bit on the side of the exact product, so the
        // float rounding of it is that of the exact product.
        uint64_t bits;
        std::memcpy(&bits, &p, 8);
        if ((bits & 1) == 0) {
            bits = (e > 0.0) == (p > 0.0) ? bits + 1 : bits - 1;
            std::memcpy(&p, &bits, 8);
        }
    }
    return static_cast<float>(p);
}

namespace {

// Handlers 0x004EF550 .. 0x004EFEF0 (the body), 0x005366B0 .. 0x00536DE0 (the collision object; of them the sphere
// colliders' and collidees' and the clipping planes' keys, Radius 0x00536A60, Normal 0x005369A0, Centre 0x005369E0,
// [EndClippingPlane] 0x00536CA0, [EndSphereCollider] 0x00536D00 and [EndSphereCollidee] 0x00536DE0, are not read
// here) and 0x004F4470 .. 0x004F58D0 (the car); a car key needs an open car, a body key an open body ([BeginCar] or
// [BeginDynamicObject]),
// Name an open definition (any of the three), a wheel or airfoil key its open block, and a key used out of its block
// stops the file. The wheel and airfoil records are the parser's and are not cleared between the blocks; an airfoil
// joins the last body opened, also after its end. last: the definition made last, with its kind.
std::optional<CarDynamics> parseFile(std::string_view text, std::optional<ObjectDynamics>* last) {
    std::optional<CarDynamics> result;
    CarDynamics* car = nullptr;
    bool wheelOpen = false;
    WheelDef wheel;
    BodyDef* body = nullptr;  // the parser's +0x68
    ObjectKind kind = ObjectKind::Collision;  // its +0xA8
    bool bodyOpen = false;    // +0x70
    bool collisionOpen = false;  // +0x6C of a [BeginCollisionObject]
    std::optional<BodyDef> object;     // a [BeginDynamicObject] that is not a car
    std::optional<BodyDef> collision;  // a [BeginCollisionObject]
    bool airfoilOpen = false;       // +0x80
    std::array<float, 4> airfoilPosition{}, airfoilForce{};
    float airfoilDrag = 0.0f;
    // The collider and contact records of the parser (+0x0C .. +0x138), kept between the blocks.
    int32_t id = 0;                           // +0x0C ID / ContactId
    int32_t surface = 0;                      // +0x10 SurfaceType
    float mu = 0.0f, restitution = 0.0f;      // +0x14, +0x18
    std::string file;                         // +0x1C File
    std::array<float, 4> point{}, end{};      // +0x20 Point, +0x50 End
    std::array<float, 4> normal{};            // +0x30 Normal
    std::array<float, 4> centre{};            // +0x40 Centre (0x005369E0): no block reads it
    float radius = 0.0f;                      // +0x60 Radius
    bool contactOpen = false, rigidOpen = false, springOpen = false, tyreOpen = false;  // +0x74, +0x78, +0x7C, +0x114
    ContactDef contact;                       // +0x84 .. +0xA8 (the clipping plane, MaxCompression) and the tyre's
    float stiffness = 0.0f, dampingRatio = 0.0f, weightDistribution = 0.0f;  // +0xAC, +0xB0, +0xB4
    // 0x004EFD50 / 0x004EFDF0 / 0x004F4830: the contact made from the records; a definition that is not a car's makes
    // a rigid contact for a tyre.
    auto addContact = [&](ContactDef::Type type) {
        if (body == nullptr) return;
        const bool ofCar = result && body == &result->body;
        // Stock problem: a tyre of a body that is not a car's is made a rigid contact and the tyre's values are
        // written past it; the rigid contact alone here.
        if (type == ContactDef::Tyre && !ofCar) type = ContactDef::Rigid;
        ContactDef c;
        c.type = type;
        c.id = id;
        c.clipped = contact.clipped;
        c.clipPoint = contact.clipPoint;
        c.clipNormal = contact.clipNormal;
        c.maxCompression = contact.maxCompression;
        c.mu = mu;
        c.restitution = restitution;
        if (type != ContactDef::Rigid) {
            c.stiffness = body->localGravity * stiffness;
            c.dampingRatio = dampingRatio;
            if (type == ContactDef::Spring) {
                c.mass = body->mass * weightDistribution;
            } else {
                // The tyres share the car's weight by FrontWeightDistribution: contacts 0 and 1 are the front ones.
                const float share = id == 0 || id == 1 ? result->frontWeightDistribution
                                                       : 1.0f - result->frontWeightDistribution;
                c.mass = share * 0.5f * body->mass;
            }
            const float d = std::sqrt(c.mass * c.stiffness) * c.dampingRatio;
            c.damping = d + d;
        }
        if (type == ContactDef::Tyre) {
            c.lateralStiffness = contact.lateralStiffness;
            c.longitudinalStiffness = contact.longitudinalStiffness;
            c.lateralMu = contact.lateralMu;
            c.longitudinalMu = contact.longitudinalMu;
            c.magic = contact.magic;
            const float divisor = c.longitudinalStiffness * c.lateralMu;
            c.muRatio = divisor != 0.0f ? (c.longitudinalMu * c.lateralStiffness) / divisor : 0.0f;
            c.eOverB = c.magic[0] != 0.0f ? c.magic[3] / c.magic[0] : 0.0f;
        }
        body->contacts.push_back(c);
    };

    for (const ScriptLine& line : parseScript(text)) {
        const std::string& k = line.keyword;
        bool handled = true;
        if (same(k, "[BeginCar]")) {
            handled = car == nullptr;
            if (handled) {
                result.emplace();
                car = &*result;
                body = &car->body;
                kind = ObjectKind::Car;
                bodyOpen = true;
            }
        } else if (same(k, "[EndCar]")) {
            handled = car != nullptr;
            if (handled) bodyOpen = false;
            car = nullptr;
        } else if (same(k, "[BeginDynamicObject]")) {
            handled = !bodyOpen;
            if (handled) {
                object.emplace();
                body = &*object;
                kind = ObjectKind::Dynamic;
                bodyOpen = true;
            }
        } else if (same(k, "[EndDynamicObject]")) {
            handled = bodyOpen;
            bodyOpen = false;
        } else if (same(k, "[BeginCollisionObject]")) {
            handled = !collisionOpen;
            if (handled) {
                collision.emplace();
                body = &*collision;
                kind = ObjectKind::Collision;
                collisionOpen = true;
            }
        } else if (same(k, "[EndCollisionObject]")) {
            handled = collisionOpen;
            collisionOpen = false;
        } else if (same(k, "Mass") || same(k, "Inertia") || same(k, "CentreOfGravity") || same(k, "LocalGravity") ||
                   same(k, "Sleep") || (same(k, "DragCoefficient") && !airfoilOpen)) {
            handled = bodyOpen;
            if (!handled) {
            } else if (same(k, "Mass")) {
                body->mass = line.real(0);
            } else if (same(k, "Inertia")) {
                body->inertia = vectorOf(line);
            } else if (same(k, "CentreOfGravity")) {
                body->centreOfGravity = vectorOf(line);
            } else if (same(k, "LocalGravity")) {
                body->localGravity = line.real(0);
            } else if (same(k, "Sleep")) {
                body->sleep = line.integer(0) != 0;
                body->sleepValue = line.real(1);
                body->sleepSet = true;
            } else {
                body->drag = line.real(0);
            }
        } else if (same(k, "[BeginAirfoil]")) {
            handled = !airfoilOpen;
            airfoilOpen = true;
        } else if (same(k, "[EndAirfoil]")) {
            handled = airfoilOpen;
            airfoilOpen = false;
            // Stock problem: an airfoil before any body writes through a null definition; left out here.
            if (handled && body != nullptr) addAirfoil(*body, airfoilPosition, airfoilForce, airfoilDrag);
        } else if (same(k, "Position") || same(k, "Force") || same(k, "DragCoefficient")) {
            handled = airfoilOpen;
            if (!handled) {
            } else if (same(k, "Position")) {
                airfoilPosition = vectorOf(line);
            } else if (same(k, "Force")) {
                airfoilForce = vectorOf(line);
            } else {
                airfoilDrag = line.real(0);
            }
        } else if (same(k, "Name") || same(k, "MaxCollidees") || same(k, "Primary")) {
            // Name needs an open collision object (any open body here); MaxCollidees and Primary write to the
            // definition unchecked.
            // Stock problem: before any definition they write through a null pointer; left out here.
            handled = !same(k, "Name") || bodyOpen || collisionOpen;
            if (handled && body != nullptr) {
                if (same(k, "Name")) {
                    body->collision.name = line.words.empty() ? std::string() : line.words[0].substr(0, 63);
                } else if (same(k, "MaxCollidees")) {
                    body->collision.maxCollidees = line.integer(0);
                } else {
                    body->collision.primary = true;
                }
            }
        } else if (same(k, "ID")) {
            id = line.integer(0);
        } else if (same(k, "SurfaceType")) {
            static const char* const kSurfaces[] = {"TARMAC_DRY", "SAND_DRY",   "GRASS_DRY", "TARMAC_WET", "SAND_WET",
                                                    "GRASS_WET",  "CONCRETE",   "METAL",     "WOOD",       "GLASS"};
            surface = 0;
            for (int32_t i = 0; i < 10; ++i) {
                if (!line.words.empty() && same(line.words[0], kSurfaces[i])) {
                    surface = i;
                    break;
                }
            }
        } else if (same(k, "Mu")) {
            mu = line.real(0);
        } else if (same(k, "Restitution")) {
            restitution = line.real(0);
        } else if (same(k, "File")) {
            file = line.words.empty() ? std::string() : line.words[0];
        } else if (same(k, "Point")) {
            point = vectorOf(line);
        } else if (same(k, "End")) {
            end = vectorOf(line);
        } else if (same(k, "Normal")) {
            normal = vectorOf(line);
        } else if (same(k, "Centre")) {
            centre = vectorOf(line);
        } else if (same(k, "Radius")) {
            radius = line.real(0);
        } else if (same(k, "[BeginMeshCollider]") || same(k, "[BeginMeshCollidee]") ||
                   same(k, "[BeginLineCollider]") || same(k, "[BeginClippingPlane]") ||
                   same(k, "[BeginSphereCollider]") || same(k, "[BeginSphereCollidee]")) {
        } else if (same(k, "[EndClippingPlane]") || same(k, "[EndSphereCollider]") ||
                   same(k, "[EndSphereCollidee]")) {
            // 0x00536CA0 / 0x00536D00 / 0x00536DE0: the block's records added to the definition, open or not.
            // Stock problem: before any definition they write through a null pointer; left out here.
            if (body == nullptr) {
            } else if (same(k, "[EndClippingPlane]")) {
                body->collision.clippingPlanes.push_back({point, normal});
            } else if (same(k, "[EndSphereCollider]")) {
                SphereCollider s;
                s.point = point;
                s.radius = radius;
                s.id = id;
                body->collision.sphereColliders.push_back(s);
            } else {
                SphereCollidee s;
                s.point = point;
                s.radius = radius;
                s.surface = surface;
                s.mu = mu;
                s.restitution = restitution;
                body->collision.sphereCollidees.push_back(s);
            }
        } else if (same(k, "[EndLineCollider]")) {
            if (body != nullptr) {
                LineCollider l;
                l.point = point;
                const float dx = end[0] - point[0], dy = end[1] - point[1], dz = end[2] - point[2];
                l.length = std::sqrt((dx * dx + dy * dy) + dz * dz);
                const float unit = 1.0f / (l.length + 1.1920929e-7f);
                l.direction = {unit * dx, unit * dy, unit * dz, end[3] - point[3]};
                l.id = id;
                body->collision.lines.push_back(l);
            }
        } else if (same(k, "[EndMeshCollider]")) {
            if (body != nullptr) {
                body->collision.meshColliderId = id;
                if (!file.empty()) body->collision.meshCollider = file;
            }
        } else if (same(k, "[EndMeshCollidee]")) {
            if (body != nullptr) {
                body->collision.meshCollideeSurface = surface;
                body->collision.meshCollideeMu = mu;
                body->collision.meshCollideeRestitution = restitution;
                if (!file.empty()) body->collision.meshCollidee = file;
            }
        } else if (same(k, "[BeginRigidContact]") || same(k, "[BeginSpringContact]") ||
                   same(k, "[BeginTyreContact]")) {
            handled = !contactOpen;
            if (handled) {
                contactOpen = true;
                rigidOpen = same(k, "[BeginRigidContact]");
                springOpen = !rigidOpen;
                tyreOpen = same(k, "[BeginTyreContact]");
            }
        } else if (same(k, "[EndRigidContact]")) {
            handled = rigidOpen;
            if (handled) {
                contactOpen = rigidOpen = false;
                addContact(ContactDef::Rigid);
            }
        } else if (same(k, "[EndSpringContact]")) {
            handled = springOpen;
            if (handled) {
                contactOpen = springOpen = false;
                addContact(ContactDef::Spring);
            }
        } else if (same(k, "[EndTyreContact]")) {
            handled = tyreOpen;
            if (handled) {
                contactOpen = springOpen = tyreOpen = false;
                addContact(ContactDef::Tyre);
            }
        } else if (same(k, "ContactId") || same(k, "ClippingPlane") || same(k, "NoClippingPlane") ||
                   same(k, "MaxCompression")) {
            handled = contactOpen;
            if (!handled) {
            } else if (same(k, "ContactId")) {
                id = line.integer(0);
            } else if (same(k, "ClippingPlane")) {
                contact.clipPoint = {line.real(0), line.real(1), line.real(2), 0.0f};
                contact.clipNormal = {line.real(3), line.real(4), line.real(5), 0.0f};
                contact.clipped = true;
            } else if (same(k, "NoClippingPlane")) {
                contact.clipPoint = {};
                contact.clipNormal = {};
                contact.clipped = false;
            } else {
                contact.maxCompression = line.real(0);
            }
        } else if (same(k, "Stiffness") || same(k, "DampingRatio") || same(k, "WeightDistribution")) {
            handled = springOpen;
            if (!handled) {
            } else if (same(k, "Stiffness")) {
                stiffness = line.real(0);
            } else if (same(k, "DampingRatio")) {
                dampingRatio = line.real(0);
            } else {
                weightDistribution = line.real(0);
            }
        } else if (same(k, "LateralStiffness") || same(k, "LongitudinalStiffness") || same(k, "LateralMu") ||
                   same(k, "LongitudinalMu") || same(k, "MagicFormulaB") || same(k, "MagicFormulaC") ||
                   same(k, "MagicFormulaD") || same(k, "MagicFormulaE")) {
            handled = tyreOpen;
            if (!handled) {
            } else if (same(k, "LateralStiffness")) {
                contact.lateralStiffness = line.real(0);
            } else if (same(k, "LongitudinalStiffness")) {
                contact.longitudinalStiffness = line.real(0);
            } else if (same(k, "LateralMu")) {
                contact.lateralMu = line.real(0);
            } else if (same(k, "LongitudinalMu")) {
                contact.longitudinalMu = line.real(0);
            } else {
                contact.magic[static_cast<size_t>(std::toupper(static_cast<unsigned char>(k.back())) - 'B')] =
                    line.real(0);
            }
        } else if (same(k, "[BeginWheel]")) {
            handled = !wheelOpen;
            wheelOpen = true;
        } else if (same(k, "[EndWheel]")) {
            // The wheel joins the last car made, also after its [EndCar].
            handled = wheelOpen;
            if (handled && result) {
                result->wheels.push_back(wheel);
            }
            wheelOpen = false;
        } else if (same(k, "WheelId") || same(k, "WheelRadius") || same(k, "FinalDrive") ||
                   same(k, "WheelInertia") || same(k, "Width") || same(k, "MaxSteerAngle") || same(k, "BrakeMu") ||
                   same(k, "HandbrakeMu") || same(k, "AxleMu") || same(k, "Drive") || same(k, "BaseHeight")) {
            handled = wheelOpen;
            if (!handled) {
            } else if (same(k, "WheelId")) {
                wheel.id = line.integer(0);
            } else if (same(k, "WheelRadius")) {
                wheel.radius = line.real(0);
            } else if (same(k, "FinalDrive")) {
                wheel.finalDrive = line.real(0);
                wheel.finalDriveInverse = inverse(wheel.finalDrive);
            } else if (same(k, "WheelInertia")) {
                wheel.inertia = line.real(0);
                wheel.inertiaInverse = inverse(wheel.inertia);
            } else if (same(k, "Width")) {
                wheel.width = line.real(0);
            } else if (same(k, "MaxSteerAngle")) {
                wheel.maxSteerAngle = line.real(0);
            } else if (same(k, "BrakeMu")) {
                wheel.brakeMu = line.real(0);
            } else if (same(k, "HandbrakeMu")) {
                // A value between 100 and 10000 becomes 10000 (0x004F5700); a missing one is 0. The bounds are
                // compared with atof's double, rounded to a float only as it is stored.
                const double v = line.words.empty() ? 0.0 : std::atof(line.words[0].c_str());
                wheel.handbrakeMu = 100.0 < v && v < 10000.0 ? 10000.0f : static_cast<float>(v);
            } else if (same(k, "AxleMu")) {
                wheel.axleMu = line.real(0);
            } else if (same(k, "Drive")) {
                wheel.drive = line.integer(0) != 0 ? 1 : 0;
            } else {
                wheel.baseHeight = line.real(0);
            }
        } else if (same(k, "TorqueCurveX") || same(k, "TorqueCurveY") || same(k, "MaxTorque") ||
                   same(k, "ReverseGearRatio") || same(k, "ForwardGearRatios") || same(k, "EngineInertia") ||
                   same(k, "ClutchSpeed") || same(k, "SteeringFactor") || same(k, "SteeringSpeed") ||
                   same(k, "PowerRangeMin") || same(k, "PowerRangeMax") || same(k, "Turbo") || same(k, "Nitro") ||
                   same(k, "ABS") || same(k, "ASR") || same(k, "ASRFactor") || same(k, "TimeFactor") ||
                   same(k, "FrontWeightDistribution")) {
            handled = car != nullptr;
            if (!handled) {
            } else if (same(k, "TorqueCurveX")) {
                car->torqueSpeeds = realList(line, 0.10471976f);
                // Stock problem: an empty list takes the value before it in memory as the last point; kept here.
                if (!car->torqueSpeeds.empty()) {
                    car->topSpeed = car->torqueSpeeds.back();
                }
            } else if (same(k, "TorqueCurveY")) {
                car->torqueFactors = realList(line);
            } else if (same(k, "MaxTorque")) {
                car->maxTorque = line.real(0);
            } else if (same(k, "ReverseGearRatio")) {
                car->reverseRatio = productToFloat(line.words.empty() ? 0.0 : std::atof(line.words[0].c_str()), 0.6f);
            } else if (same(k, "ForwardGearRatios")) {
                car->forwardRatios = realList(line);
            } else if (same(k, "EngineInertia")) {
                car->engineInertia = line.real(0);
            } else if (same(k, "ClutchSpeed")) {
                car->clutchSpeed = line.real(0);
            } else if (same(k, "SteeringFactor")) {
                car->steeringFactor = line.real(0);
            } else if (same(k, "SteeringSpeed")) {
                car->steeringSpeed = line.real(0);
            } else if (same(k, "PowerRangeMin")) {
                car->powerRangeMin = line.real(0);
            } else if (same(k, "PowerRangeMax")) {
                car->powerRangeMax = line.real(0);
            } else if (same(k, "Turbo")) {
                car->turbo = line.real(0);
            } else if (same(k, "Nitro")) {
                car->nitro = line.real(0);
                car->nitroTime = line.real(1);
            } else if (same(k, "ABS")) {
                car->abs = true;
            } else if (same(k, "ASR")) {
                car->asr = true;
            } else if (same(k, "ASRFactor")) {
                car->asrFactor = line.real(0);
            } else if (same(k, "TimeFactor")) {
                car->timeFactor = line.real(0);
            } else {
                car->frontWeightDistribution = line.real(0);
            }
        }
        if (!handled) {
            break;
        }
    }
    if (last != nullptr && body != nullptr) *last = ObjectDynamics{kind, *body};
    return result;
}

}  // namespace

std::optional<CarDynamics> parseDynamics(std::string_view text) {
    return parseFile(text, nullptr);
}

std::optional<ObjectDynamics> parseObjectDynamics(std::string_view text) {
    std::optional<ObjectDynamics> last;
    parseFile(text, &last);
    return last;
}

}  // namespace oj
