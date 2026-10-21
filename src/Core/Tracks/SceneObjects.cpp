#include "Core/Tracks/SceneObjects.h"

#include "Core/Base/GameFiles.h"
#include "Core/Formats/Ocd.h"
#include "Core/Base/X87.h"

#include <bit>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace oj {

namespace {

// 0x00715960: the properties' keys by their numbers.
const char* const kKeys[42] = {
    "UNKNOWN", "PHYSICS", "ROTX_MIN", "ROTX_MAX", "ROTY_MIN", "ROTY_MAX", "ROTZ_MIN", "ROTZ_MAX", "SPEED_MIN",
    "SPEED_MAX", "SPEED_MATCH", "CSPLINE_ON", "CSPLINE_OFF", "CAM_ZOOM_CLOSE", "CAM_ZOOM_WIDE", "CAM_ZOOM_AUTO",
    "LIGHTFLARE", "LIGHTFLARE_YREFLECTION", "LIGHTFLARE_SIZE", "LIGHTFLARE_BRIGHTNESS", "LIGHTFLARE_FALLOFF_MIN",
    "LIGHTFLARE_FALLOFF_MAX", "LIGHTFLARE_OFFSET", "LIGHTFLARE_YREFLECT_SIZE_X", "LIGHTFLARE_YREFLECT_SIZE_Y",
    "LIGHTFLARE_YREFLECT_ALPHA_TOP", "LIGHTFLARE_YREFLECT_ALPHA_BOTTOM", "LIGHTFLARE_REFLECTION",
    "LIGHTFLARE_REFLECTION_SIZE", "LIGHTFLARE_DRYYREFLECTION", "LIGHTFLARE_DRYYREFLECT_SIZE_X",
    "LIGHTFLARE_DRYYREFLECT_SIZE_Y", "LIGHTFLARE_DRYYREFLECT_ALPHA_TOP", "LIGHTFLARE_DRYYREFLECT_ALPHA_BOTTOM",
    "CRUISE_MESSAGE", "CRUISE_MOVE", "CRUISE_NAME", "CRUISE_DIFFICULTY", "CRUISE_CROWD", "NOD_ANG", "EVEN_SPACING",
    "TIME_OF_DAY"};

bool space(char c) {
    return c == '\n' || c == ' ' || c == '\r';
}

bool startsWith(std::string_view s, const char* prefix) {
    const size_t n = std::strlen(prefix);
    return s.size() >= n && _strnicmp(s.data(), prefix, n) == 0;
}

bool contains(const std::string& s, const char* text) {
    return std::strstr(s.c_str(), text) != nullptr;
}

// The loading thread's x87 unit keeps 53 bits: a lerp by a draw, stored as a float.
float lerpDrawn(float from, double to, int r) {
    const double k = static_cast<double>(std::bit_cast<float>(0x38000100u));  // 1 / 32767 (0x006FF7EC)
    return static_cast<float>((to - static_cast<double>(from)) * static_cast<double>(r) * k + from);
}

}  // namespace

ObjectProperties::ObjectProperties(std::string_view text) {
    // The text ends at its first NUL; the stock parser stops there too (it would read on past a NUL in the middle of
    // a line, which no file has).
    const std::string s(text.substr(0, text.find('\0')));
    size_t p = 0;
    bool end = false;
    while (!end) {
        while (p < s.size() && space(s[p])) ++p;
        if (p >= s.size()) break;
        std::string key;
        while (p < s.size() && !space(s[p]) && s[p] != '=') key += s[p++];
        if (p >= s.size()) end = true;
        Entry e;
        for (int32_t k = 0; k < 42; ++k) {
            if (_stricmp(key.c_str(), kKeys[k]) == 0) {
                e.key = k;
                break;
            }
        }
        if (p < s.size() && s[p] != '=') {
            while (p < s.size() && space(s[p])) ++p;
        }
        ++p;  // the '=' (or whatever stands there)
        while (p < s.size() && space(s[p])) ++p;
        if (p >= s.size()) end = true;
        std::string value;
        while (p < s.size() && !space(s[p])) {
            const char c = s[p];
            if ((c < '0' || c > '9') && c != '.' && c != '-') e.isText = true;
            value += c;
            ++p;
        }
        if (p >= s.size()) end = true;
        if (e.isText) {
            e.text = value;
        } else {
            e.number = static_cast<float>(std::atof(value.c_str()));
        }
        m_entries.push_back(std::move(e));
    }
}

const ObjectProperties::Entry* ObjectProperties::entry(int32_t key) const {
    for (const Entry& e : m_entries) {
        if (e.key == key) return &e;
    }
    return nullptr;
}

float ObjectProperties::number(int32_t key) const {
    const Entry* e = entry(key);
    return e != nullptr ? e->number : 0.0f;
}

const std::string* ObjectProperties::text(int32_t key) const {
    const Entry* e = entry(key);
    return e != nullptr && e->isText ? &e->text : nullptr;
}

int32_t ObjectProperties::integer(int32_t key) const {
    const Entry* e = entry(key);
    return e != nullptr ? x87::chop(e->number) : 0;
}

namespace {

// 0x00540320: the sphere of a spline node grows to take a point in: by half of the point's excess over the radius,
// its centre the other half towards the point (the loading thread's double precision, the values stored as floats).
void growSphere(Vec4& centre, float& radius, const Vec4& p) {
    const float dx = p[0] - centre[0];
    const float dy = p[1] - centre[1];
    const float dz = p[2] - centre[2];
    const auto sum = static_cast<float>((static_cast<double>(dz) * dz + static_cast<double>(dy) * dy) +
                                        static_cast<double>(dx) * dx);
    const double d = std::sqrt(static_cast<double>(sum));
    const double excess = d - static_cast<double>(radius);
    if (!(excess > 0.0)) return;
    const auto stored = static_cast<float>(excess);
    const double k = static_cast<double>(stored) / d;
    centre[0] = static_cast<float>(static_cast<double>(centre[0]) + (dx * k) * 0.5);
    const auto y = static_cast<float>(dy * k);
    centre[1] = static_cast<float>(static_cast<double>(centre[1]) + static_cast<double>(y) * 0.5);
    const auto z = static_cast<float>(static_cast<double>(static_cast<float>(dz * k)) * 0.5);
    centre[2] = static_cast<float>(static_cast<double>(centre[2]) + z);
    radius = static_cast<float>(static_cast<double>(radius) + static_cast<double>(stored) * 0.5);
}

// 0x00540580 / 0x0054F030: a spline node's segments from knot to knot (a closed one back to the first), the sphere
// grown by each segment's two knots, the first set to the first knot with a radius of 0.
void buildSpline(const DlhObject& object, std::vector<SplineSegment>& segments, Vec4& centre, float& radius) {
    auto add = [&](const Vec4& a, const Vec4& out, const Vec4& in, const Vec4& b) {
        if (segments.empty()) {
            centre = a;
            radius = 0.0f;
        }
        growSphere(centre, radius, a);
        growSphere(centre, radius, b);
        segments.push_back({a, out, in, b});
    };
    const std::vector<DlhSplinePoint>& p = object.points;
    for (size_t i = 0; i + 1 < p.size(); ++i) add(p[i].knot, p[i].out, p[i + 1].in, p[i + 1].knot);
    if ((object.closed & 1) != 0 && !p.empty()) add(p.back().knot, p.back().out, p.front().in, p.front().knot);
}

// The identity turned about y (0x00534960), then, when asked, about x (0x00534A10); no place.
Mat34 turned(float aboutY, bool turnX, float aboutX) {
    const double cy = std::cos(static_cast<double>(aboutY)), sy = std::sin(static_cast<double>(aboutY));
    Mat34 m = identity34();
    m[0] = static_cast<float>(cy);
    m[2] = static_cast<float>(-sy);
    m[8] = static_cast<float>(sy);
    m[10] = static_cast<float>(cy);
    if (turnX) {
        const double cx = std::cos(static_cast<double>(aboutX)), sx = std::sin(static_cast<double>(aboutX));
        for (size_t row = 0; row < 3; ++row) {
            const float c1 = m[row * 4 + 1], c2 = m[row * 4 + 2];
            m[row * 4 + 1] = static_cast<float>(cx * c1 - sx * c2);
            m[row * 4 + 2] = static_cast<float>(sx * c1 + cx * c2);
        }
    }
    return m;
}

// Within -pi .. pi by whole turns (0x00407910).
float wrapAngle(float a) {
    constexpr float kPi = 3.14159274f, kTurn = 6.28318548f;
    while (a > kPi) a -= kTurn;
    while (a < -kPi) a += kTurn;
    return a;
}

}  // namespace

Vec4 SceneSpline::pointAt(float place) const {
    const size_t count = segments.size();
    if (count == 0) return {0.0f, 0.0f, 0.0f, 1.0f};
    const auto whole = static_cast<int32_t>(std::floor(static_cast<double>(place)));
    float t = place - static_cast<float>(whole);
    int32_t i = whole;
    if (closed) {
        i %= static_cast<int32_t>(count);
        if (i < 0) i = 0;
    } else {
        if (i < 0) {
            i = 0;
            t = 0.0f;
        }
        if (i >= static_cast<int32_t>(count)) {
            t = 1.0f;
            i = static_cast<int32_t>(count) - 1;
        }
    }
    const SplineSegment& s = segments[static_cast<size_t>(i)];
    const double u = t, v = 1.0 - u;
    const double a = v * v * v, b = v * v * u * 3.0, c = v * u * u * 3.0, d = u * u * u;
    Vec4 r{};
    for (size_t k = 0; k < 3; ++k) {
        r[k] = static_cast<float>(((s.p3[k] * d + s.p0[k] * a) + s.p1[k] * b) + s.p2[k] * c);
    }
    r[3] = 1.0f;
    return r;
}

void SceneObjects::loadDlh(const Dlh& dlh, Load& load) {
    ++m_files;
    for (const DlhNode& root : dlh.roots) {
        const size_t made = makeNode(dlh, root, load);
        if (filterRoot(dlh, root, made, load)) m_roots.push_back(made);
    }
}

// 0x0054F260: the node of its object (0x0054EB80 a mesh node, 0x0054EF60 an effect's emitter, ...), handed to the
// manager, then its children.
size_t SceneObjects::makeNode(const Dlh& dlh, const DlhNode& node, Load& load) {
    const DlhObject& object = dlh.objects[node.object];
    const size_t record = m_nodes.size();
    m_nodes.emplace_back();
    m_nodes[record].file = m_files - 1;
    m_nodes[record].object = node.object;
    m_nodes[record].matrix = node.matrix;
    SceneNodeType type = SceneNodeType::Dummy;
    switch (object.type) {
    case DlhObject::Mesh:
        // 0x00556AE0 → 0x005550D0: four values for the node's look (rand() x 1/32767 each).
        m_nodes[record].mesh = true;
        for (int k = 0; k < 4; ++k) {
            m_nodes[record].random[k] =
                static_cast<float>(load.random->next()) * std::bit_cast<float>(0x38000100u);
        }
        ++m_meshNodes;
        type = SceneNodeType::Mesh;
        break;
    case DlhObject::Effect: {
        // The effect of the object's name, less a leading "effect_".
        std::string_view name = object.name;
        if (startsWith(name, "effect_")) name.remove_prefix(7);
        int32_t draw = -1;
        if (makeEmitter(*load.effects, name, *load.random, load.sound, &draw) != nullptr) {
            ++m_emitters;
            m_emitterNodes.push_back({record, std::string(name), draw});
        }
        type = SceneNodeType::Dummy;
        break;
    }
    case DlhObject::Light:
        type = SceneNodeType::Light;
        break;
    case DlhObject::Spline: {
        type = SceneNodeType::Spline;
        std::vector<SplineSegment> segments;
        buildSpline(object, segments, m_nodes[record].sphereCentre, m_nodes[record].sphereRadius);
        m_nodes[record].hasSphere = !segments.empty();
        break;
    }
    case DlhObject::Helper:
        type = SceneNodeType::Helper;
        break;
    default:
        type = SceneNodeType::Dummy;
        break;
    }
    handleNode(object, node, type, record, load);
    for (const DlhNode& child : node.children) {
        const size_t made = makeNode(dlh, child, load);
        m_nodes[record].children.push_back(made);
    }
    return record;
}

// 0x00421D50: what the manager makes of a node, by its type and name, then its properties.
void SceneObjects::handleNode(const DlhObject& object, const DlhNode& node, SceneNodeType type, size_t record,
                              Load& load) {
    std::optional<ObjectProperties> props;
    if (!object.text.empty()) props.emplace(object.text);
    const std::string& name = object.name;
    SceneObject* made = nullptr;
    switch (type) {
    case SceneNodeType::Mesh: {
        // A mesh of another time of day is hidden (+0xC0 |= 4) and makes nothing.
        if (props && props->has(ObjectProperties::TimeOfDay)) {
            const std::string* when = props->text(ObjectProperties::TimeOfDay);
            const char* w = when != nullptr ? when->c_str() : "";
            const uint32_t t = load.timeOfDay;
            const bool other = (t == 1 && _stricmp(w, "morn") != 0) || (t == 2 && _stricmp(w, "noon") != 0) ||
                               (t == 3 && _stricmp(w, "eve") != 0) || (t == 4 && _stricmp(w, "night") != 0);
            if (other) {
                m_nodes[record].hidden = true;
                break;
            }
        }
        // 0x00420FF0: the object of the name.
        SceneObject o;
        o.name = name;
        o.matrix = node.matrix;
        if (startsWith(name, "barrel")) {
            o.kind = SceneObject::Kind::Barrel;
        } else if (startsWith(name, "bin")) {
            o.kind = SceneObject::Kind::Bin;
        } else if (startsWith(name, "crowd")) {
            // 0x0041D870: the turn's most a step drawn (under 0.03 none) and the steps before its first look.
            o.kind = SceneObject::Kind::Crowd;
            const int r = load.random->next();
            o.crowdTurnStep = static_cast<float>(static_cast<double>(r) *
                                              static_cast<double>(std::bit_cast<float>(0x36999ACDu)));
            if (o.crowdTurnStep < std::bit_cast<float>(0x3CF5C28Fu)) o.crowdTurnStep = 0.0f;
            o.crowdCountdown = static_cast<int32_t>(static_cast<uint32_t>(load.random->next()) % 20);
        } else if (contains(name, "FBUMP") || contains(name, "RBUMP") || contains(name, "BONNET") ||
                   contains(name, "SPOILER") || contains(name, "SSKIRT") || contains(name, "HEADLIGHT") ||
                   contains(name, "REARLIGHT") || contains(name, "WMIR") || contains(name, "DECAL") ||
                   contains(name, "EXHAUST") || contains(name, "CHASS")) {
            o.kind = SceneObject::Kind::CarPart;
        } else if (startsWith(name, "WHEEL")) {
            o.kind = SceneObject::Kind::Wheel;
        } else if (contains(name, "DRIVER")) {
            break;
        } else {
            o.kind = SceneObject::Kind::Generic;
        }
        o.node = record;
        m_objects.push_back(std::move(o));
        made = &m_objects.back();
        break;
    }
    case SceneNodeType::Dummy:
        if (startsWith(name, "camera")) {
            // 0x00418EA0: a replay camera; its speed drawn between SPEED_MIN and SPEED_MAX when both are given.
            SceneCamera c;
            c.name = name;
            c.matrix = node.matrix;
            if (props) {
                if (props->has(ObjectProperties::CsplineOn)) c.splineOn = props->integer(ObjectProperties::CsplineOn) - 1;
                if (props->has(ObjectProperties::CsplineOff)) {
                    c.splineOff = props->integer(ObjectProperties::CsplineOff) - 1;
                }
                if (props->has(ObjectProperties::CamZoomClose)) {
                    c.zoomMode = 0;
                    c.zoom = props->number(ObjectProperties::CamZoomClose);
                }
                if (props->has(ObjectProperties::CamZoomWide)) {
                    c.zoomMode = 1;
                    c.zoom = props->number(ObjectProperties::CamZoomWide);
                }
                if (props->has(ObjectProperties::CamZoomAuto)) {
                    c.zoomMode = 2;
                    c.zoom = props->number(ObjectProperties::CamZoomAuto);
                }
                if (props->has(ObjectProperties::SpeedMatch)) c.speedMatch = props->number(ObjectProperties::SpeedMatch);
                float low = -1.0f;
                float high = -1.0f;
                if (props->has(ObjectProperties::SpeedMin)) {
                    low = props->number(ObjectProperties::SpeedMin);
                    c.speedMatch = 0.0f;
                }
                if (props->has(ObjectProperties::SpeedMax)) {
                    high = props->number(ObjectProperties::SpeedMax);
                    c.speedMatch = 0.0f;
                }
                if (low < 0.0f) {
                    if (high >= 0.0f) c.speed = high;
                } else if (high < 0.0f) {
                    c.speed = low;
                } else {
                    c.speed = lerpDrawn(low, high, load.random->next());
                }
            }
            m_cameras.push_back(std::move(c));
        } else if (startsWith(name, "env_camera")) {
            // 0x00407B00: the node's place in the world.
            m_environmentPlace = Vec4{node.matrix[3], node.matrix[7], node.matrix[11], 0.0f};
        }
        break;
    case SceneNodeType::Helper:
        // 0x004B01D0: a Move* helper is a zone of the cruise manager ([[game+0x80]+0x60]).
        if (startsWith(name, "Move")) {
            SceneMoveZone z;
            z.matrix = node.matrix;
            z.shape = object.shape;
            z.boxMin = object.boxMin;
            z.boxMax = object.boxMax;
            z.centre = object.centre;
            z.radius = object.radius;
            z.properties = object.text;
            z.children = node.children.size();
            m_moveZones.push_back(std::move(z));
        }
        break;
    case SceneNodeType::Spline: {
        // 0x00419CC0 for a centre line (the first made is the race's, [0x0075ECD0]), else 0x004236A0: a spline whose
        // children will ride it; both on the path of 0x0041B720.
        const bool centreLine = startsWith(name, "centre_spline");
        {
            SceneSpline s;
            s.name = name;
            s.points = object.points;
            s.closed = (object.closed & 1) != 0;
            Vec4 centre{};
            float radius = 0.0f;
            buildSpline(object, s.segments, centre, radius);
            // 0x0041B720: the path's points at the knots, moved by the node's place; each one's direction from the
            // point before (the first from the last) and that point's length to it.
            const size_t count = s.closed ? s.segments.size() : s.segments.size() + 1;
            if (!s.segments.empty()) {
                for (size_t i = 0; i < count; ++i) {
                    SplinePathPoint p;
                    p.position = s.pointAt(static_cast<float>(i));
                    p.position[0] = node.matrix[3] + p.position[0];
                    p.position[1] = node.matrix[7] + p.position[1];
                    p.position[2] = node.matrix[11] + p.position[2];
                    s.path.push_back(p);
                }
                for (size_t i = 0; i < count; ++i) {
                    SplinePathPoint& here = s.path[i];
                    SplinePathPoint& before = s.path[i == 0 ? count - 1 : i - 1];
                    const float dx = here.position[0] - before.position[0];
                    const float dy = here.position[1] - before.position[1];
                    const float dz = here.position[2] - before.position[2];
                    here.direction = {dx, dy, dz, here.position[3]};
                    const float length = std::sqrt((dz * dz + dy * dy) + dx * dx);
                    before.length = length;
                    const float unit = 1.0f / (length + 1.1920929e-7f);
                    here.direction = {unit * dx, unit * dy, unit * dz, here.position[3]};
                    here.distance = i == 0 ? 0.0f : before.distance + before.length;
                }
                for (const SplinePathPoint& p : s.path) s.length = p.length + s.length;
            }
            if (centreLine) {
                if (!m_centreLine) m_centrePath = std::move(s.path);
                m_centreLine = true;
                break;
            }
            if (props) {
                s.evenSpacing = props->has(ObjectProperties::EvenSpacing);
                if (props->has(ObjectProperties::SpeedMin)) s.speedMin = props->number(ObjectProperties::SpeedMin);
                if (props->has(ObjectProperties::SpeedMax)) s.speedMax = props->number(ObjectProperties::SpeedMax);
            }
            s.node = record;
            m_splines.push_back(std::move(s));
            m_splineNodes.push_back(&node);
        }
        break;
    }
    default:
        break;
    }
    if (!props) return;
    if (made != nullptr) {
        // 0x0041A9A0: the turning animation; each *_MAX given draws a rate between the minimum and it.
        static const int32_t kAxes[3][2] = {{ObjectProperties::RotXMin, ObjectProperties::RotXMax},
                                            {ObjectProperties::RotYMin, ObjectProperties::RotYMax},
                                            {ObjectProperties::RotZMin, ObjectProperties::RotZMax}};
        for (size_t a = 0; a < 3; ++a) {
            if (props->has(kAxes[a][0])) {
                made->axes |= 1u << a;
                made->rates[a] = props->number(kAxes[a][0]) * 10.0f;
            }
            if (props->has(kAxes[a][1])) {
                const double to = static_cast<double>(props->number(kAxes[a][1])) * 10.0;
                made->axes |= 1u << a;
                made->rates[a] = lerpDrawn(made->rates[a], to, load.random->next());
            }
        }
        if (props->has(ObjectProperties::NodAng)) {
            made->axes |= 8;
            made->nod = props->number(ObjectProperties::NodAng) * 200.0f;
        }
    }
    // LIGHTFLARE: the node's lens flare.
    if (std::optional<SceneFlare> f = lightFlare(*props, load.wet)) {
        f->node = record;
        m_flares.push_back(std::move(*f));
    }
    // PHYSICS: a physics object of the profile (with or without an object made).
    if (props->has(ObjectProperties::Physics)) {
        const std::string* physics = props->text(ObjectProperties::Physics);
        const std::string profile = physics != nullptr ? *physics : std::string();
        if (made != nullptr) made->physics = profile;
        makePhysics(profile, object, node, made != nullptr ? m_objects.size() - 1 : SIZE_MAX, record, load);
    }
}

std::optional<SceneFlare> lightFlare(const ObjectProperties& props, bool wet) {
    if (!props.has(ObjectProperties::LightFlare)) return std::nullopt;
    auto textOf = [&](int32_t key) {
        const std::string* t = props.text(key);
        return t != nullptr ? *t : std::string();
    };
    auto numberOr = [&](int32_t key, float fallback) { return props.has(key) ? props.number(key) : fallback; };
    SceneFlare f;
    f.texture = textOf(ObjectProperties::LightFlare);
    f.tilted = _stricmp(f.texture.c_str(), "car_flare01") == 0;
    f.reflection = textOf(ObjectProperties::LightFlareReflection);
    const bool dry = !wet;
    f.yReflection = textOf(dry && props.has(ObjectProperties::LightFlareDryYReflection)
                               ? ObjectProperties::LightFlareDryYReflection
                               : ObjectProperties::LightFlareYReflection);
    f.size = numberOr(ObjectProperties::LightFlareSize, 100.0f);
    // The angles in degrees x 0.017452778 (0x006FFCD0), their cosines on the loading thread.
    const float k = std::bit_cast<float>(0x3C8EF921u);
    f.cosMin = static_cast<float>(std::cos(static_cast<double>(numberOr(ObjectProperties::LightFlareFalloffMin, 2.0f)) *
                                           static_cast<double>(k)));
    f.cosMax = static_cast<float>(std::cos(static_cast<double>(numberOr(ObjectProperties::LightFlareFalloffMax, 8.0f)) *
                                           static_cast<double>(k)));
    f.offset = numberOr(ObjectProperties::LightFlareOffset, 20.0f);
    auto road = [&](int32_t wetKey, int32_t dryKey) {
        const int32_t key = dry && props.has(dryKey) ? dryKey : wetKey;
        return numberOr(key, 0.0f);
    };
    f.roadWidth = road(ObjectProperties::LightFlareYReflectSizeX, ObjectProperties::LightFlareDryYReflectSizeX);
    f.roadHeight = road(ObjectProperties::LightFlareYReflectSizeY, ObjectProperties::LightFlareDryYReflectSizeY);
    f.roadAlphaTop =
        road(ObjectProperties::LightFlareYReflectAlphaTop, ObjectProperties::LightFlareDryYReflectAlphaTop);
    f.roadAlphaBottom =
        road(ObjectProperties::LightFlareYReflectAlphaBottom, ObjectProperties::LightFlareDryYReflectAlphaBottom);
    f.reflectionSize = numberOr(ObjectProperties::LightFlareReflectionSize, 0.0f);
    return f;
}

const ObjectDynamics* SceneObjects::profileOf(const std::string& profile, const Ocd* collider, const Ocd* collidee,
                                              const GameFiles* files) {
    if (files == nullptr) return nullptr;
    const std::string file = profile + ".txt";
    auto found = m_profiles.find(file);
    if (found == m_profiles.end()) {
        // The profile read once (0x004EFF30); its first object gives it its hulls (0x00536590, 0x005365D0: a scene
        // object's +0x34 hull the collider +0x88 and its +0x30 the collidee +0x90, a car part's its two files; each
        // replaces the file's, none too), and a rigid body's centre of gravity: the middle of the collider's box
        // (0x00535BC0) x 0.01. Every later object of the profile takes these.
        const auto bytes = files->read(std::filesystem::path("scripts") / file);
        if (!bytes) return nullptr;
        auto parsed = parseObjectDynamics(
            std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
        if (!parsed) return nullptr;
        auto def = std::make_unique<ObjectDynamics>(std::move(*parsed));
        CollisionDef& c = def->body.collision;
        c.colliderShape = collider != nullptr ? std::make_shared<const Ocd>(*collider) : nullptr;
        c.collideeShape = collidee != nullptr ? std::make_shared<const Ocd>(*collidee) : nullptr;
        // Stock problem: a rigid body's profile whose first object has no collider hull reads its box through a
        // null pointer; its centre of gravity is the file's here.
        if (def->kind == ObjectKind::Dynamic && c.colliderShape != nullptr) {
            // A value below the least so far is not compared with the greatest (both start at FLT_MAX, -FLT_MAX).
            Vec4 low{FLT_MAX, FLT_MAX, FLT_MAX, 0.0f};
            Vec4 high{-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0f};
            for (const Vec4& v : c.colliderShape->vertices) {
                for (size_t a = 0; a < 3; ++a) {
                    if (v[a] < low[a]) {
                        low[a] = v[a];
                    } else if (v[a] > high[a]) {
                        high[a] = v[a];
                    }
                }
            }
            for (size_t a = 0; a < 3; ++a) def->body.centreOfGravity[a] = (high[a] + low[a]) * 0.5f * 0.01f;
            def->body.centreOfGravity[3] = 0.0f;
        }
        found = m_profiles.emplace(file, std::move(def)).first;
    }
    return found->second.get();
}

bool SceneObjects::makePhysics(const std::string& profile, const DlhObject& object, const DlhNode& node, size_t made,
                               size_t record, Load& load) {
    const ObjectDynamics* read = profileOf(profile, object.hull ? &*object.hull : nullptr,
                                           object.hull2 ? &*object.hull2 : nullptr, load.files);
    if (read == nullptr) return false;
    const ObjectDynamics& d = *read;
    if (d.kind != ObjectKind::Collision && d.kind != ObjectKind::Dynamic) return true;
    ScenePhysics p;
    p.profile = profile;
    p.kind = d.kind;
    p.object = made;
    p.node = record;
    p.def = std::make_unique<BodyDef>(d.body);
    // The model's origin at the node's place (metres); a rigid body starts upright (0x004F2060), a collision object
    // keeps the node's matrix.
    const Vec4 origin{node.matrix[3] * 0.01f, node.matrix[7] * 0.01f, node.matrix[11] * 0.01f, 0.0f};
    p.body = std::make_unique<RigidBody>(*p.def, origin);
    if (d.kind == ObjectKind::Collision) {
        p.body->fixed = true;
        for (size_t i = 0; i < 12; ++i) p.body->node[i] = node.matrix[i];
    }
    m_physics.push_back(std::move(p));
    return true;
}

std::optional<PartPhysics> SceneObjects::makePartPhysics(const std::string& files, const Mat34& nodeWorld,
                                                         const GameFiles* gameFiles) {
    if (gameFiles == nullptr) return std::nullopt;
    // The hulls through the meshes' folder ([G+0x1C]+8); a file that is not there or does not read is none.
    auto hull = [&](const char* suffix) -> std::optional<Ocd> {
        const auto bytes = gameFiles->read(std::filesystem::path("mesh") / (files + suffix));
        if (!bytes) return std::nullopt;
        return parseOcd(reinterpret_cast<const uint8_t*>(bytes->data()), bytes->size());
    };
    const std::optional<Ocd> collider = hull("_collider.ocd");
    const std::optional<Ocd> collidee = hull("_collidee.ocd");
    const std::string profile = "dynamic_" + files;
    const ObjectDynamics* read =
        profileOf(profile, collider ? &*collider : nullptr, collidee ? &*collidee : nullptr, gameFiles);
    if (read == nullptr || (read->kind != ObjectKind::Collision && read->kind != ObjectKind::Dynamic))
        return std::nullopt;
    PartPhysics p;
    p.profile = profile;
    p.kind = read->kind;
    p.def = std::make_unique<BodyDef>(read->body);
    const Vec4 origin{nodeWorld[3] * 0.01f, nodeWorld[7] * 0.01f, nodeWorld[11] * 0.01f, 0.0f};
    p.body = std::make_unique<RigidBody>(*p.def, origin);
    if (read->kind == ObjectKind::Collision) {
        p.body->fixed = true;
        p.body->node = nodeWorld;
    }
    return p;
}

CarPartObject CarPartObject::make(const std::string& nodeName) {
    CarPartObject o;
    o.files = nodeName;
    const char* n = nodeName.c_str();
    if (_stricmp(n, "fbump") == 0) {
        o.axes |= 7;
        o.bit = 1;
    } else if (_stricmp(n, "rbump") == 0) {
        o.axes |= 7;
        o.amount = std::bit_cast<float>(0x3951B717u);
        o.bit = 2;
    } else if (_stricmp(n, "bonnet") == 0) {
        o.axes |= 1;
        o.bit = 4;
    } else if (_stricmp(n, "spoiler") == 0) {
        o.axes |= 1;
        o.softness = 0.5f;
        o.bit = 8;
    } else if (_strnicmp(n, "sskirt_l", 6) == 0) {
        o.axes |= 5;
        o.bit = 0x10;
    } else if (_strnicmp(n, "sskirt_r", 6) == 0) {
        // Stock problem: never reached; "sskirt_l" over 6 characters takes the right skirt too.
        o.axes |= 5;
        o.bit = 0x20;
    } else if (_strnicmp(n, "exhaust", 7) == 0) {
        o.axes |= 3;
        o.softness = 0.5f;
        o.files = "exhaust";
    }
    return o;
}

namespace {

// 0x00407960: sin of a float, rounded to a float.
float sinOf(float a) {
    return static_cast<float>(std::sin(static_cast<double>(a)));
}

}  // namespace

void CarPartObject::wobble(float dt, const DamageZone* zone, float speed, float sideways, const Vec4& change) {
    // The main thread's x87 unit rounds every sum and product to a float.
    const float level = zone != nullptr ? zone->impact * 0.05f : 0.0f;  // [0x006FF994]
    if (axes == 0) return;
    phase = wrapAngle(dt * rate / (level / softness + 0.0125f) + phase);  // [0x006FFBDC]
    const float slow = sinOf(phase * 0.3f);                              // [0x006FFB98]
    const float inverse = 1.0f / softness;
    const float turn = -(std::fabs(sinOf(phase) + slow) * inverse * level * dt) * amount * speed;
    angles = {turn, turn, turn};
    if (sideways > 20.0f) {  // [0x00725298]
        // 0x00407C90 twice: the change x 0.001 ([0x00725294]), then x (1 / softness) x level.
        const float k = inverse * level;
        const float x = change[0] * 0.001f * k;
        const float y = change[1] * 0.001f * k;
        const float z = change[2] * 0.001f * k;
        angles = {turn + y, turn + z, turn + x};
    }
    const float most = 100.0f * amount;  // [0x00725290]
    for (size_t a = 0; a < 3; ++a) {
        if ((axes & (1u << a)) == 0) continue;
        if (angles[a] > most) angles[a] = most;
        if (-most > angles[a]) angles[a] = -most;
    }
}

Mat34 turnObject(uint32_t axes, std::array<float, 3>& angles, const std::array<float, 3>& rates, float nod, float dt) {
    Mat34 turn = identity34();
    for (size_t a = 0; a < 3; ++a) {
        if ((axes & (1u << a)) == 0) continue;
        angles[a] = wrapAngle(dt * rates[a] + angles[a]);
        float angle = angles[a];
        if ((axes & 8) != 0) angle = wrapAngle(std::sin(angle) * nod * 0.02f);
        const float c = std::cos(angle), s = std::sin(angle);
        Mat34 r = identity34();
        if (a == 0) {  // 0x0041B680
            r[5] = c;
            r[6] = -s;
            r[9] = s;
            r[10] = c;
        } else if (a == 1) {  // 0x0041B4E0
            r[0] = c;
            r[2] = s;
            r[8] = -s;
            r[10] = c;
        } else {  // 0x0041B4A0
            r[0] = c;
            r[1] = -s;
            r[4] = s;
            r[5] = c;
        }
        turn = multiply(turn, r);
    }
    return turn;
}

bool helperContains(const Mat34& matrix, uint32_t shape, const Vec4& boxMin, const Vec4& boxMax, const Vec4& centre,
                    float radius, const Vec4& point) {
    if (shape == 1) {
        const Vec4 p = transformPoint(inverse(matrix), point);
        for (int i = 0; i < 3; ++i) {
            if (p[i] < boxMin[i] || p[i] > boxMax[i]) return false;
        }
        return true;
    }
    if (shape == 2) {
        const Vec4 c = transformPoint(matrix, centre);
        const float dx = c[0] - point[0];
        const float dy = c[1] - point[1];
        const float dz = c[2] - point[2];
        const auto squared = static_cast<float>(static_cast<double>(dz) * dz + static_cast<double>(dx) * dx +
                                                static_cast<double>(dy) * dy);
        return std::sqrt(squared) <= radius;
    }
    return false;
}

float zoneDistance(const SceneAtmosZone& zone, const Vec4& point) {
    const auto dx = static_cast<double>(zone.place[0] - point[0]);
    const double dy = static_cast<double>(zone.place[1]) - point[1];
    const double dz = static_cast<double>(zone.place[2]) - point[2];
    return std::sqrt(static_cast<float>((dz * dz + dy * dy) + dx * dx));
}

bool SceneObjects::inReverb(const Vec4& point) const {
    for (const SceneMoveZone& t : m_reverbs) {
        Mat34 m{};
        for (size_t i = 0; i < 12; ++i) m[i] = t.matrix[i];
        if (helperContains(m, t.shape, t.boxMin, t.boxMax, t.centre, t.radius, point)) return true;
    }
    return false;
}

bool SceneObjects::inTunnel(const Vec4& point) const {
    for (const SceneMoveZone& t : m_tunnels) {
        Mat34 m{};
        for (size_t i = 0; i < 12; ++i) m[i] = t.matrix[i];
        if (helperContains(m, t.shape, t.boxMin, t.boxMax, t.centre, t.radius, point)) return true;
    }
    return false;
}

void SceneObjects::update(float stepTime, bool objectsStep, const std::vector<Vec4>& cars) {
    float dt = stepTime;
    if (m_framesBeforeTime != 0) {
        --m_framesBeforeTime;
        dt = 0.0f;
    }
    m_stepTime = dt;
    for (SceneSpline& spline : m_splines) {
        const size_t count = spline.path.size();
        if (count == 0) continue;
        const std::array<float, 16>& world = m_nodes[spline.node].matrix;
        for (SceneRider& r : spline.riders) {
            // 0x00423D00
            if (!r.moving) continue;
            SceneNode& n = m_nodes[r.node];
            const Mat34& m = n.local;
            // The node's place in the world (the spline's matrix times its own place).
            const float px = ((world[0] * m[3] + world[1] * m[7]) + world[2] * m[11]) + world[3];
            const float py = ((world[4] * m[3] + world[5] * m[7]) + world[6] * m[11]) + world[7];
            const float pz = ((world[8] * m[3] + world[9] * m[7]) + world[10] * m[11]) + world[11];
            const auto last = static_cast<uint32_t>(count - 1);
            auto clampIndex = [&](int32_t i) { return static_cast<uint32_t>(i) < count ? static_cast<uint32_t>(i) : last; };
            const int32_t previous = r.next - 1 < 0 ? static_cast<int32_t>(last) : r.next - 1;
            const SplinePathPoint& here = spline.path[clampIndex(r.next)];
            const SplinePathPoint& before = spline.path[clampIndex(previous)];
            const float cx = px - here.position[0], cy = py - here.position[1], cz = pz - here.position[2];
            const float bx = px - before.position[0], by = py - before.position[1], bz = pz - before.position[2];
            const float ahead = (cy * here.direction[1] + cz * here.direction[2]) + cx * here.direction[0];
            const float toHere = std::sqrt((cy * cy + cz * cz) + cx * cx);
            const float sinceBefore = (bz * bz + by * by) + bx * bx;
            const float progress = std::sqrt(sinceBefore) / (std::sqrt(sinceBefore) + toHere);
            if (progress > r.progress) r.progress = progress;
            if (r.progress < 0.0f) {
                r.progress = 0.0f;
            } else if (r.progress > 1.0f) {
                r.progress = 1.0f;
            }
            // Past the point it headed for: the next one (an open path's rider stops at its last).
            if (ahead > 0.0f) {
                ++r.next;
                r.progress = 0.0f;
                if (r.next > static_cast<int32_t>(last)) {
                    if (spline.closed) {
                        r.next = 0;
                    } else {
                        r.next = static_cast<int32_t>(last);
                        r.moving = false;
                    }
                }
            }
            int32_t from = r.next - 1;
            if (from < 0) from = static_cast<int32_t>(last);
            if (!r.moving) continue;
            // The point to steer for: the look-ahead (0.6 x the speed, within an eighth of the segment before and
            // 50000) along the path from where it is, as a place of the curve.
            float look = r.lookAhead * r.speed;
            const float least = before.length * 0.125f;
            if (look < least) {
                look = least;
            } else if (look > 50000.0f) {
                look = 50000.0f;
            }
            float remaining = (1.0f - r.progress) * before.length;
            float fraction = 1.0f - (remaining - look) / before.length;
            uint32_t at = static_cast<uint32_t>(from);
            if (remaining < look) {
                do {
                    ++at;
                    if (at > last) {
                        if (!spline.closed) {
                            at = last;
                            break;
                        }
                        at = 0;
                    }
                    const SplinePathPoint& p = spline.path[at < count ? at : last];
                    fraction = (look - remaining) / p.length;
                    remaining = remaining + p.length;
                } while (remaining < look);
            }
            r.target = spline.pointAt(static_cast<float>(static_cast<int32_t>(at)) + fraction);
            const float lx = m[3], ly = m[7], lz = m[11];
            const float dx = r.target[0] - lx, dy = r.target[1] - ly, dz = r.target[2] - lz;
            const float distance = std::sqrt((dy * dy + dz * dz) + dx * dx);
            if (distance > 100.0f) {
                // Turn towards it: the pitch and the yaw each by at most dt x 0.1 x 60 (3 x the angle left, the
                // roll back to 0); the node turned anew (0x00534D80) about y by pi - yaw, about x by -pitch when it
                // banks.
                const auto yawTo = static_cast<float>(std::atan2(static_cast<double>(dx), static_cast<double>(dz)));
                const float flat = std::sqrt(dz * dz + dx * dx);
                const auto pitchTo = static_cast<float>(std::atan2(static_cast<double>(dy), static_cast<double>(flat)));
                const float pitchLeft = wrapAngle(pitchTo - r.pitch);
                const float yawLeft = wrapAngle(yawTo - r.yaw);
                const float rollLeft = 0.0f - r.roll;
                const float rate = dt * r.turnRate * 60.0f;
                const float gain = rate * 3.0f;
                float pitchBy = pitchLeft * gain;
                if (pitchBy > rate) pitchBy = rate;
                if (pitchBy < -rate) pitchBy = -rate;
                float yawBy = yawLeft * gain;
                if (yawBy > rate) yawBy = rate;
                if (yawBy < -rate) yawBy = -rate;
                r.pitch = pitchBy + r.pitch;
                r.yaw = yawBy + r.yaw;
                r.roll = rollLeft + r.roll;
                r.pitch = wrapAngle(r.pitch);
                r.yaw = wrapAngle(r.yaw);
                n.local = turned(wrapAngle(3.14159274f - r.yaw), r.banks, wrapAngle(-r.pitch));
            }
            // The move: dt x the speed, on an open path slowed within 2000 of its end.
            float move = dt * r.speed;
            if (!spline.closed) {
                const float left = spline.path.back().distance - (before.length * r.progress + before.distance);
                if (left < 2000.0f) move = left * 0.0005f * move;
            }
            const float cosYaw = std::cos(r.yaw);
            const float vz = std::cos(r.pitch) * cosYaw * move;
            const float vy = std::sin(r.pitch) * move;
            const float vx = std::cos(r.pitch) * std::sin(r.yaw) * move;
            r.velocity = {vx, vy, vz, r.velocity[3]};
            n.local[3] = lx + vx;
            n.local[7] = ly + vy;
            n.local[11] = lz + vz;
        }
    }
    if (!objectsStep) return;
    for (SceneObject& o : m_objects) {
        if (o.kind == SceneObject::Kind::Crowd) {
            crowdStep(o, cars);
            continue;
        }
        // 0x0041B050: each axis turned turns on by dt x its rate (held within +-pi); with a nod the angle used is
        // sin(the angle) x the nod x 0.02; the turn about x, then y, then z.
        if (o.axes == 0 || o.node >= m_nodes.size()) continue;
        float turnTime = dt;
        if (o.kind == SceneObject::Kind::Barrel || o.kind == SceneObject::Kind::Bin ||
            o.kind == SceneObject::Kind::Wheel) {
            // Stock problem: their step (0x0041B270, vtable 0x006EF010 and the barrel's and bin's) keeps 1 / dt in its
            // argument for the speed (0x0041B2AA, dt over 0) and hands that on to the turn (0x0041B47F); no barrel,
            // bin or wheel of the game's files turns.
            if (dt > 0.0f) turnTime = 1.0f / dt;
        } else if (o.kind != SceneObject::Kind::Generic) {
            continue;
        }
        m_nodes[o.node].turning = true;
        m_nodes[o.node].turn = turnObject(o.axes, o.angles, o.rates, o.nod, turnTime);
    }
}

namespace {

// 0x006FFBF8: 8 / pi, sixteenths of a turn per radian.
const double kSixteenths = std::bit_cast<float>(0x4022F983u);

}  // namespace

size_t nearestPathPoint(const std::vector<SplinePathPoint>& path, float x, float y, float z) {
    size_t best = 0;
    float least = FLT_MAX;
    for (size_t i = 0; i < path.size(); ++i) {
        const Vec4& p = path[i].position;
        const double dy = static_cast<double>(p[1]) - y;
        const double dx = static_cast<double>(p[0]) - x;
        const double dz = static_cast<double>(p[2]) - z;
        const float d = std::sqrt(static_cast<float>((dy * dy + dx * dx) + dz * dz));
        if (d < least) {
            least = d;
            best = i;
        }
    }
    return best;
}

void SceneObjects::finishTrack(CrtRandom& random) {
    for (SceneObject& o : m_objects) {
        if (o.kind != SceneObject::Kind::Crowd || !m_centreLine) continue;
        // 0x0041D9D0 with the race's centre line: the line's point nearest the member, of the points either side of
        // it the nearer (the next on a tie or less); the heading -atan2(z, x) x 8 / pi of the member's offset from
        // that point plus its offset from the nearest, then (rand() / 32767 - 0.5) x 6 added, held within 0 .. 16.
        // Stock problem: a centre line of no points is read out of its list; here the heading stays 0.
        const size_t count = m_centrePath.size();
        float heading = o.crowdHeading;
        const float mx = o.matrix[3], my = o.matrix[7], mz = o.matrix[11];
        if (count != 0) {
            const size_t at = nearestPathPoint(m_centrePath, mx, my, mz);
            const size_t before = at == 0 ? count - 1 : at - 1;
            const size_t after = at + 1 >= count ? 0 : at + 1;
            const Vec4& p = m_centrePath[at].position;
            const Vec4& a = m_centrePath[before].position;
            const Vec4& b = m_centrePath[after].position;
            const float ax = mx - a[0], ay = my - a[1], az = mz - a[2];
            const float bx = mx - b[0], by = my - b[1], bz = mz - b[2];
            const float toA = std::sqrt(static_cast<float>(
                (static_cast<double>(ax) * ax + static_cast<double>(ay) * ay) + static_cast<double>(az) * az));
            const float toB = std::sqrt(static_cast<float>(
                (static_cast<double>(bz) * bz + static_cast<double>(by) * by) + static_cast<double>(bx) * bx));
            const float ox = toB < toA ? bx : ax;
            const float oz = toB < toA ? bz : az;
            const float px = mx - p[0], pz = mz - p[2];
            const double y = static_cast<double>(oz) + pz;
            const double x = static_cast<double>(ox) + px;
            heading = static_cast<float>(-std::atan2(y, x) * kSixteenths);
        }
        const double drawn = static_cast<double>(static_cast<uint32_t>(random.next())) *
                             static_cast<double>(std::bit_cast<float>(0x38000100u));
        const double turned = (drawn - 0.5) * 6.0 + heading;
        o.crowdHeading = static_cast<float>(turned);
        if (turned < 0.0) o.crowdHeading = static_cast<float>(turned + 16.0);
        if (o.crowdHeading > 16.0f) o.crowdHeading = o.crowdHeading - 16.0f;
    }
}

void SceneObjects::crowdStep(SceneObject& o, const std::vector<Vec4>& cars) const {
    // 0x0041DC00: every 20 steps the car nearest across the ground (0x00419EE0: the first of the nearest, by the
    // squares of x and z) gives the target: -atan2(z, x) x 8 / pi of the member's offset from it less the heading,
    // within -8 .. 8 by whole turns, x 0.4, within -2 .. 2; each step the turn moves toward the target by at most
    // the member's step.
    const int32_t left = o.crowdCountdown;
    o.crowdCountdown = left - 1;
    if (left < 1) {
        o.crowdCountdown = 20;
        // Stock problem: without a centre line ([0x0075ECD0] 0) the cars' trackers are read through null, and with no
        // car the place is the stack's; here the look is passed over.
        if (m_centreLine && !cars.empty()) {
            const float mx = o.matrix[3], mz = o.matrix[11];
            size_t nearest = 0;
            float least = FLT_MAX;
            for (size_t i = 0; i < cars.size(); ++i) {
                const float dx = cars[i][0] - mx;
                const float dz = cars[i][2] - mz;
                const float d = dx * dx + dz * dz;
                if (d < least) {
                    least = d;
                    nearest = i;
                }
            }
            const float dx = mx - cars[nearest][0];
            const double dz = static_cast<double>(mz) - cars[nearest][2];
            double t = -std::atan2(dz, static_cast<double>(dx)) * kSixteenths - o.crowdHeading;
            if (t > 8.0) t -= 16.0;
            if (t < -8.0) t += 16.0;
            t *= static_cast<double>(0.4f);
            o.crowdTarget = static_cast<float>(t);
            if (t > 2.0) o.crowdTarget = 2.0f;
            if (o.crowdTarget < -2.0f) o.crowdTarget = -2.0f;
        }
    }
    double move = static_cast<double>(o.crowdTarget) - o.crowdTurn;
    if (move == 0.0) return;
    if (move > o.crowdTurnStep) move = o.crowdTurnStep;
    if (move < -o.crowdTurnStep) move = -o.crowdTurnStep;
    o.crowdTurn = static_cast<float>(move + o.crowdTurn);
}

std::vector<int32_t> SceneObjects::makeMoveZones(CrtRandom& random) const {
    std::vector<int32_t> draws;
    for (const SceneMoveZone& z : m_moveZones) {
        for (size_t i = 0; i < z.children; ++i) draws.push_back(random.next());
    }
    return draws;
}

// 0x00422750 for a root: the children of a spline (not the centre spline) ride it: each a rider (0x00423A80: a speed
// drawn between SPEED_MIN and SPEED_MAX when both are 0 or more, else the one that is, else 1500); then their start
// (0x004238E0): evenly spaced with EVEN_SPACING, else a single one at the start and several at drawn segments.
bool SceneObjects::filterRoot(const Dlh& dlh, const DlhNode& root, size_t record, Load& load) {
    const DlhObject& object = dlh.objects[root.object];
    // An env_camera* dummy and a helper other than an atmosphere or sound zone do not join the world.
    if (object.type == DlhObject::Dummy && startsWith(object.name, "env_camera")) return false;
    if (object.type == DlhObject::Helper) {
        if (startsWith(object.name, "tunnel")) {
            SceneMoveZone z;
            z.matrix = m_nodes[record].matrix;
            z.shape = object.shape;
            z.boxMin = object.boxMin;
            z.boxMax = object.boxMax;
            z.centre = object.centre;
            z.radius = object.radius;
            m_tunnels.push_back(std::move(z));
            return true;
        }
        if (startsWith(object.name, "atmos")) {
            Mat34 m{};
            for (size_t i = 0; i < 12; ++i) m[i] = m_nodes[record].matrix[i];
            SceneAtmosZone z;
            if (object.shape == 2) {
                z.place = transformPoint(m, object.centre);
                z.radius = object.radius;
            } else {
                const Vec4& a = object.boxMin;
                const Vec4& b = object.boxMax;
                const Vec4 middle{static_cast<float>((static_cast<double>(a[0]) + b[0]) * 0.5),
                                  (a[1] + b[1]) * 0.5f, (a[2] + b[2]) * 0.5f, a[3]};
                z.place = transformPoint(m, middle);
                const auto dx = static_cast<double>(b[0] - a[0]);
                const double dy = static_cast<double>(b[1]) - a[1];
                const double dz = static_cast<double>(b[2]) - a[2];
                z.radius = std::sqrt(static_cast<float>((dz * dz + dy * dy) + dx * dx)) * 0.5f;
            }
            // Stock problem: the name "atmos" alone is read from past its end, and a name of 16 characters or more
            // after the sixth is left without its end (the cut runs on into the zone's place and may write a 0
            // there); here the name is empty or cut at 16.
            std::string name = object.name.size() > 6 ? object.name.substr(6, 16) : std::string();
            for (size_t i = 1; i < name.size(); ++i) {
                if (std::isdigit(static_cast<unsigned char>(name[i])) != 0) {
                    name.resize(i);
                    break;
                }
            }
            z.name = std::move(name);
            m_atmosZones.push_back(std::move(z));
            return true;
        }
        if (startsWith(object.name, "reverb")) {
            SceneMoveZone z;
            z.matrix = m_nodes[record].matrix;
            z.shape = object.shape;
            z.boxMin = object.boxMin;
            z.boxMax = object.boxMax;
            z.centre = object.centre;
            z.radius = object.radius;
            m_reverbs.push_back(std::move(z));
            return true;
        }
        return false;
    }
    if (object.type != DlhObject::Spline || startsWith(object.name, "centre_spline")) return true;
    SceneSpline* spline = nullptr;
    for (size_t i = 0; i < m_splineNodes.size(); ++i) {
        if (m_splineNodes[i] == &root) spline = &m_splines[i];
    }
    if (spline == nullptr) return true;
    for (size_t c = 0; c < root.children.size(); ++c) {
        const DlhNode& child = root.children[c];
        SceneRider r;
        r.name = dlh.objects[child.object].name;
        r.node = m_nodes[record].children[c];
        if (spline->speedMin < 0.0f || spline->speedMax < 0.0f) {
            if (spline->speedMin < 0.0f) {
                if (spline->speedMax >= 0.0f) r.speed = spline->speedMax;
            } else {
                r.speed = spline->speedMin;
            }
        } else {
            r.speed = lerpDrawn(spline->speedMin, spline->speedMax, load.random->next());
        }
        r.banks = !startsWith(r.name, "cablecar");
        spline->riders.push_back(std::move(r));
    }
    // 0x00423B80: a rider at a place of the curve: the point it heads for next, its yaw and pitch towards the
    // curve's point 0.1 further, its node turned by them (about y by pi - yaw, then about x by the pitch) at the
    // place's point.
    const size_t count = spline->path.size();  // +0x20
    auto place = [&](SceneRider& r, float at) {
        r.place = at;
        r.next = static_cast<int32_t>(std::floor(static_cast<double>(at))) + 1;
        if (r.next >= static_cast<int32_t>(count)) r.next = 0;
        const Vec4 p = spline->pointAt(at);
        const Vec4 q = spline->pointAt(at + 0.1f);
        const double dx = static_cast<double>(q[0]) - p[0];
        const double dy = static_cast<double>(q[1]) - p[1];
        const double dz = static_cast<double>(q[2]) - p[2];
        r.yaw = static_cast<float>(std::atan2(dx, dz));
        r.pitch = static_cast<float>(std::atan2(dy, std::sqrt(dx * dx + dz * dz)));
        Mat34 m = turned(wrapAngle(3.14159274f - r.yaw), true, r.pitch);
        m[3] = p[0];
        m[7] = p[1];
        m[11] = p[2];
        SceneNode& n = m_nodes[r.node];
        n.placed = true;
        n.local = m;
    };
    for (SceneRider& r : spline->riders) place(r, 0.0f);
    // 0x004238E0: without EVEN_SPACING a single rider at the start, several each at a drawn point; with it the
    // riders spread at the path's length / (riders + 1), walked point by point.
    if (!spline->evenSpacing) {
        if (spline->riders.size() == 1) {
            place(spline->riders[0], 0.0f);
        } else if (count != 0) {
            for (SceneRider& r : spline->riders) {
                place(r, static_cast<float>(static_cast<uint32_t>(load.random->next()) % count));
            }
        }
    } else if (count != 0) {
        const float step = spline->length / static_cast<float>(spline->riders.size() + 1);
        uint32_t at = 0;
        float walked = 0.0f;
        float fraction = 0.0f;
        for (SceneRider& r : spline->riders) {
            place(r, static_cast<float>(at) + fraction);
            float length = 0.0f;
            do {
                const size_t i = at < count ? at : count - 1;
                length = spline->path[i].length;
                walked = walked + length;
                ++at;
            } while (walked < step);
            walked = walked - step;
            fraction = walked / length;
        }
    }
    return true;
}

}  // namespace oj
