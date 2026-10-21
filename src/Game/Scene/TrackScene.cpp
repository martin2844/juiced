#include "Game/Scene/TrackScene.h"

#include "Core/Tracks/Atmosphere.h"
#include "Core/Base/GameFiles.h"
#include "Core/Saves/GraphicsConfig.h"
#include "Core/Base/Log.h"
#include "Core/Physics/TrackCollision.h"
#include "Game/Scene/PostEffects.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <optional>

namespace oj::scene {

using render::ClearFlags;
using render::Primitive;
using render::RenderState;

namespace {

const float kRandomScale = std::bit_cast<float>(0x38000100u);  // 1 / 32767 (0x006FF7EC)
const float kEpsilon = std::bit_cast<float>(0x34000000u);      // 2^-23 (0x006FF99C)
constexpr size_t kClasses = 53;  // the region lists hold the classes 0 .. 52 (0x00543610)

std::optional<std::vector<uint8_t>> readBytes(const GameFiles& files, const std::filesystem::path& path) {
    const auto bytes = files.read(path);
    if (!bytes) return std::nullopt;
    std::vector<uint8_t> out(bytes->size());
    if (!out.empty()) std::memcpy(out.data(), bytes->data(), out.size());
    return out;
}

// The atmosphere's name part of the race's time of day and weather (0x00424270).
std::string conditionsName(uint32_t timeOfDay, uint32_t weather) {
    const char* time = timeOfDay == 1 ? "_morn" : timeOfDay == 3 ? "_eve" : timeOfDay == 4 ? "_night" : "_noon";
    return std::string(time) + (weather == 2 ? "_wet" : "_dry");
}

}  // namespace

oj::Mat34 TrackScene::Instance::matrix() const {
    oj::Mat34 m{};
    std::copy_n(block.begin(), 12, m.begin());
    return m;
}

TrackScene::TrackScene(Engine& engine) : m_engine(engine), m_lens(engine) {
    // The cube's face cameras (0x005C8D10): aspect 1, a quarter turn up and across (on a wide screen 1.1780972, the
    // wide screen's factor making it a quarter turn); turned to look along +x, -x, +y, -y, +z and -z.
    const bool wide = engine.wideScreen();
    const float quarter = std::bit_cast<float>(0x3FC90FDBu);
    for (render::Camera& f : m_faces) {
        f.setAspect(1.0f);
        f.setFov(wide ? std::bit_cast<float>(0x3F96CBE4u) : quarter, quarter, wide);
    }
    rotateY(m_faces[0].local, -quarter);
    rotateY(m_faces[1].local, quarter);
    rotateX(m_faces[2].local, quarter);
    rotateX(m_faces[3].local, -quarter);
    rotateY(m_faces[5].local, std::bit_cast<float>(0x40490FDBu));
    for (render::Camera& f : m_faces) f.invalidate();
}

TrackScene::~TrackScene() {
    render::IRenderDevice& device = m_engine.device();
    for (const auto& m : m_meshes) {
        if (m->vertices != 0) device.destroyBuffer(m->vertices);
        if (m->indices != 0) device.destroyBuffer(m->indices);
    }
    for (const auto& t : m_textures) {
        if (t) device.destroyTexture(t->id);
    }
    for (const auto& t : m_tiles) {
        if (t) device.destroyTexture(t->id);
    }
}

bool TrackScene::load(const TrackSetup& setup, CrtRandom& random, std::string* why) {
    auto fail = [why](std::string text) {
        if (why != nullptr) *why = std::move(text);
        return false;
    };
    const GameFiles files(std::filesystem::current_path());
    const std::filesystem::path base = std::filesystem::path("tracks") / setup.track;

    const auto tdfBytes = readBytes(files, base / (setup.track + ".tdf"));
    const auto tdf = tdfBytes ? parseTdf(*tdfBytes) : std::nullopt;
    if (!tdf) return fail("no .tdf of " + setup.track);
    const TdfRoute* route = nullptr;
    for (const TdfRoute& r : tdf->routes) {
        if (_stricmp(r.name.c_str(), (setup.route + ".wld").c_str()) == 0) route = &r;
    }
    if (route == nullptr) return fail("no route " + setup.route + " in " + setup.track);
    const std::string prefix = tdf->track + "_";
    const std::string routeBase = prefix + setup.route;

    // The atmosphere; a ShadowSet left at -1 takes the time of day's light set (0x00424270).
    m_atmosphere = std::make_unique<render::AtmosphereSet>();
    if (const auto atm = files.read(base / (tdf->track + conditionsName(setup.timeOfDay, setup.weather) + ".atm"))) {
        m_atmosphere->list = parseAtmospheres(std::string_view(reinterpret_cast<const char*>(atm->data()), atm->size()));
    }
    if (m_atmosphere->list.empty()) {
        m_atmosphere.reset();
        return fail("no atmosphere of " + setup.track);
    }
    // 0x0053393B: the set's first atmosphere in use (each [BeginAtmosphere] took the next of its 20 slots and was
    // the one in use, 0x00531E30), its +0x1C0 = 1 (the renderer's +0x188 too).
    Atmosphere& atmosphere = m_atmosphere->list.front();
    m_atmosphere->current = &atmosphere;
    atmosphere.drawQuality = 1.0f;
    m_engine.renderer().drawQuality = 1.0f;
    if (atmosphere.shadowSet == -1 && setup.timeOfDay >= 1 && setup.timeOfDay <= 4) {
        atmosphere.shadowSet = static_cast<int32_t>(setup.timeOfDay) - 1;
    }
    // 0x004245C2: the ground maps of the light set: <track>_<set>.sqh, the sun on what moves, and .pqh, the puddles.
    const std::string mapName = setup.track + "_" + std::to_string(atmosphere.shadowSet);
    if (const auto sqh = readBytes(files, base / (mapName + ".sqh"))) m_sunMap = parseSunMap(*sqh);
    if (const auto pqh = readBytes(files, base / (mapName + ".pqh"))) m_puddleMap = parseSunMap(*pqh);
    // The effects manager of the race: the particles' colours of the atmosphere (0x00533943), the sun map (+0x140).
    if (setup.effects != nullptr) {
        m_particles = std::make_unique<ParticleSystem>(m_engine, *setup.effects);
        m_particles->setColours(atmosphere);
        m_particles->sunMap = sunMap();
        m_particles->density = setup.density;
        m_particles->jobRandom = &random;
        m_particles->soundWorld = setup.soundWorld;
        m_soundWorld = setup.soundWorld;
    }
    m_environment = m_engine.cubeTexture(atmosphere.envmap.empty() ? "arcade_env" : atmosphere.envmap);
    // The sky dome of the atmosphere (as the interface's views make it, 0x005315A0).
    m_sky = std::make_unique<render::SkyDome>(m_engine.renderer(), m_engine.newMaterial("Sky"),
                                              render::skyColours(m_atmosphere.get()));
    const uint32_t condition = conditionBit(atmosphere.night != 0, atmosphere.wet != 0);

    // The route's materials, by the class numbers of its .d8m.
    const auto d8m = readBytes(files, base / (routeBase + ".d8m"));
    const auto materials = d8m ? parseRouteMaterials(*d8m) : std::nullopt;
    if (!materials) return fail("no materials of " + setup.route);
    struct MaterialOf {
        render::Material* material = nullptr;
        uint32_t classId = 0;
        uint32_t flags = 0;
    };
    std::vector<MaterialOf> made(materials->materials.size());
    std::map<uint32_t, size_t> missing;
    // The tree classes draw rand() values when they are made and given their parameters.
    m_engine.renderer().materialRandom = &random;
    for (size_t i = 0; i < made.size(); ++i) {
        const RouteMaterial& d = materials->materials[i];
        const ExeTables::MaterialClass* info = nullptr;
        auto material = m_engine.newMaterial(d.classId, &info);
        made[i].classId = d.classId;
        made[i].flags = info != nullptr ? info->flags : 0;
        if (!material) {
            ++missing[d.classId];
            continue;
        }
        for (size_t p = 0; p < d.params.size(); ++p) {
            if (d.params[p].size() >= 4) material->setParam(static_cast<uint32_t>(p), d.params[p].data());
        }
        made[i].material = material.get();
        m_materials.push_back(std::move(material));
    }
    m_engine.renderer().materialRandom = nullptr;
    // The world's +0xCC: the flags of the materials' classes together (0x00543A15); with bit 0 or 1 the world's
    // +0x1E8 is textures\testenv.dds (0x00543F05, 0x00510280), the environment its reflection cubes are drawn with.
    uint32_t classFlags = 0;
    for (const MaterialOf& m : made) classFlags |= m.flags;
    if ((classFlags & 3) != 0) m_testEnvironment = m_engine.cubeTexture("testenv");

    // The geometry layers in the .tdf order: a layer hidden in the race's conditions keeps its meshes and instances
    // (their places in the .ocl and the light sets) with the main view's range 0 .. 0 (0x00543D20, 0x005450C0).
    // A .dlh in the order (0x0054F520) is loaded only in the conditions it is not hidden in; its objects draw from
    // the same thread's rand().
    size_t instanceCount = 0;
    SceneObjects::Load objectsLoad;
    objectsLoad.random = &random;
    objectsLoad.effects = setup.effects;
    objectsLoad.sound = setup.sound;
    objectsLoad.timeOfDay = setup.timeOfDay;
    objectsLoad.files = &files;
    objectsLoad.wet = atmosphere.wet != 0;
    const EffectLibrary noEffects;
    if (objectsLoad.effects == nullptr) objectsLoad.effects = &noEffects;
    for (const TdfEntry& e : route->entries) {
        if (e.name.size() >= 4 && _stricmp(e.name.c_str() + e.name.size() - 4, ".dlh") == 0) {
            if ((e.hidden & condition) != 0) continue;
            const auto bytes = readBytes(files, base / (prefix + e.name));
            const auto dlh = bytes ? parseDlh(*bytes) : std::nullopt;
            if (!dlh) {
                log::warning("track: scene objects {} not read", e.name);
                continue;
            }
            m_sceneObjects.loadDlh(*dlh, objectsLoad);
            // The file's meshes: the parts as a model mesh's, each part's material through the route's table.
            std::vector<render::Mesh*> meshes(dlh->objects.size(), nullptr);
            std::vector<std::array<float, 5>> spheres(dlh->objects.size());
            for (size_t o = 0; o < dlh->objects.size(); ++o) {
                const DlhObject& object = dlh->objects[o];
                if (object.type != DlhObject::Mesh || object.mesh.submeshes.empty()) continue;
                oj::Mesh data;
                data.indices = object.mesh.indices;
                std::vector<render::Material*> partMaterials;
                for (const DlhSubmesh& s : object.mesh.submeshes) {
                    MeshPart p;
                    p.indexStart = s.indexStart;
                    p.indexCount = s.indexCount;
                    p.faceType = s.list != 0 ? 1u : 0u;
                    p.lodIn = s.lodMin;
                    p.lodOut = s.lodMax;
                    p.renderFlag = s.flags;
                    p.declaration = s.declaration;
                    p.stride = s.stride;
                    p.vertexCount = s.vertexCount;
                    p.vertices = s.vertices;
                    data.parts.push_back(std::move(p));
                    const int32_t which = s.material < materials->remap.size() ? materials->remap[s.material] : -1;
                    partMaterials.push_back(which >= 0 && static_cast<size_t>(which) < made.size()
                                                ? made[static_cast<size_t>(which)].material
                                                : nullptr);
                }
                m_objectMeshes.push_back(
                    std::make_unique<render::Mesh>(m_engine.device(), data, partMaterials));
                meshes[o] = m_objectMeshes.back().get();
                spheres[o] = object.mesh.sphere;
            }
            m_meshOf.push_back(std::move(meshes));
            m_sphereOf.push_back(std::move(spheres));
            continue;
        }
        if (e.name.size() < 4 || _stricmp(e.name.c_str() + e.name.size() - 4, ".lyr") != 0) continue;
        const std::string layer = e.name.substr(0, e.name.size() - 4);
        auto bytes = readBytes(files, base / (prefix + layer + ".d8g"));
        if (!bytes) bytes = readBytes(files, base / (layer + ".d8g"));
        const auto groups = bytes ? parseD8g(*bytes) : std::nullopt;
        if (!groups) {
            log::warning("track: layer {} not read", layer);
            continue;
        }
        const bool visible = (e.hidden & condition) == 0;
        const bool reflected = (e.reflected & condition) != 0;
        render::IRenderDevice& device = m_engine.device();
        for (const TrackGroup& g : *groups) {
            for (const TrackMesh& tm : g.meshes) {
                auto mesh = std::make_unique<Mesh>();
                mesh->range = tm.range;
                // The world's +0x190: the greatest far of the cars' environment range.
                if (tm.range[3] > m_cubeRange) m_cubeRange = tm.range[3];
                if (!visible) mesh->range[0] = mesh->range[1] = 0.0f;
                if (!reflected) mesh->range[4] = mesh->range[5] = 0.0f;
                mesh->centre = tm.centre;
                mesh->primitive = g.primitive;
                const int32_t which = tm.materialId < materials->remap.size() ? materials->remap[tm.materialId] : -1;
                if (which >= 0 && static_cast<size_t>(which) < made.size()) {
                    mesh->material = made[static_cast<size_t>(which)].material;
                    mesh->classId = made[static_cast<size_t>(which)].classId;
                    mesh->deferred = (made[static_cast<size_t>(which)].flags & 0x24) != 0;
                }
                mesh->vertexCount = tm.vertexCount;
                mesh->indexCount = static_cast<uint32_t>(tm.indices.size());
                mesh->stride = tm.stride;
                if (visible || reflected) {
                    mesh->vertices = device.createVertexBuffer(tm.vertices.data(), static_cast<uint32_t>(tm.vertices.size()));
                    if (!tm.indices.empty()) {
                        mesh->indices = device.createIndexBuffer(tm.indices.data(), mesh->indexCount);
                    }
                    std::vector<render::VertexElement> elements(tm.declaration.size());
                    for (size_t i = 0; i < elements.size(); ++i) {
                        const VertexElement& v = tm.declaration[i];
                        elements[i] = {v.stream, v.offset, v.type, v.method, v.usage, v.usageIndex};
                    }
                    mesh->declaration = device.createDeclaration(elements.data());
                }
                // The instances (0x005450C0): the node's rows; the columns' lengths and the columns over them; the
                // radius; four random values; the sphere's centre in the world. The loading thread's x87 works at
                // 53 bits and each value is stored as a float.
                const size_t count = tm.instanceCount();
                mesh->instances.resize(count);
                for (size_t n = 0; n < count; ++n) {
                    Instance& in = mesh->instances[n];
                    in.regions.fill(0xFFFFFFFFu);
                    std::array<float, 16> node{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
                    if (!tm.nodes.empty()) node = tm.nodes[n];
                    std::copy_n(node.begin(), 12, in.block.begin());
                    for (int c = 0; c < 3; ++c) {
                        const double a = node[c], b = node[4 + c], d = node[8 + c];
                        const auto squared = static_cast<float>((d * d + b * b) + a * a);
                        const double length = std::sqrt(static_cast<double>(squared));
                        if (c == 0 || static_cast<double>(in.radius) < length) in.radius = static_cast<float>(length);
                        in.block[12 + c] = static_cast<float>(length);
                        const double k = 1.0 / (length + static_cast<double>(kEpsilon));
                        in.block[16 + c] = static_cast<float>(a * k);
                        in.block[20 + c] = static_cast<float>(k * b);
                        in.block[24 + c] = static_cast<float>(k * d);
                    }
                    // Stock problem: w of the column lengths and of the normalised rows is never written (the
                    // memory the instances were made in); 0 here.
                    in.radius = tm.radius * in.radius;
                    for (float& v : in.random) v = static_cast<float>(random.next()) * kRandomScale;
                    const double px = tm.centre[0], py = tm.centre[1], pz = tm.centre[2];
                    in.centre = {static_cast<float>(((node[2] * pz + node[1] * py) + px * node[0]) + node[3]),
                                 static_cast<float>(((node[4] * px + node[6] * pz) + node[5] * py) + node[7]),
                                 static_cast<float>(((node[8] * px + node[10] * pz) + node[9] * py) + node[11]),
                                 0.0f};
                }
                // The reflection range's far is at most 5000 x the square of the greatest radius of its
                // instances; the world's +0x1A0 the greatest of them.
                float widest = 0.0f;
                for (const Instance& in : mesh->instances) {
                    if (in.radius > widest) widest = in.radius;
                }
                const float reach = widest * widest * 5000.0f;
                if (reach < mesh->range[5]) mesh->range[5] = reach;
                if (mesh->range[5] > m_reflectionRange) m_reflectionRange = mesh->range[5];
                instanceCount += count;
                m_meshes.push_back(std::move(mesh));
            }
        }
    }

    // The route's textures (.d8w, pixels in the .d8t) and the light tiles of its light set (.shd, pixels in the
    // .txb), the set named by the atmosphere's ShadowSet (0x00544AF0, 0x00544EF0).
    const auto d8w = readBytes(files, base / (routeBase + ".d8w"));
    const auto world = d8w ? parseD8w(*d8w, instanceCount) : std::nullopt;
    if (!world) return fail("no .d8w of " + setup.route);
    const auto d8t = readBytes(files, base / (tdf->track + ".d8t"));
    m_textures.resize(world->textures.textures.size());
    auto textureOf = [&](uint32_t index) -> const render::GameTexture* {
        if (index >= m_textures.size() || !d8t) return nullptr;
        if (!m_textures[index]) {
            m_textures[index] = m_engine.makeTexture(textureFile(world->textures.textures[index], *d8t));
        }
        return m_textures[index].get();
    };
    for (size_t i = 0; i < made.size(); ++i) {
        if (made[i].material == nullptr) continue;
        const RouteMaterial& d = materials->materials[i];
        for (size_t slot = 0; slot < d.textures.size(); ++slot) {
            made[i].material->setTexture(static_cast<uint32_t>(slot), textureOf(d.textures[slot]));
        }
    }
    const int32_t set = atmosphere.shadowSet;
    if (set >= 0 && static_cast<size_t>(set) < world->sets.size()) {
        const D8w::LightSet& lights = world->sets[static_cast<size_t>(set)];
        const auto shd = readBytes(files, base / (prefix + lights.name + ".shd"));
        const auto directory = shd ? parseShd(*shd) : std::nullopt;
        const auto txb = directory ? readBytes(files, base / (prefix + "shadowMaps.txb")) : std::nullopt;
        if (directory && txb) {
            for (const TextureEntry& t : directory->textures) {
                m_tiles.push_back(m_engine.makeTexture(textureFile(t, *txb)));
            }
            size_t at = 0;
            for (const auto& m : m_meshes) {
                for (Instance& in : m->instances) in.tile = lights.tiles[at++];
            }
        } else {
            log::info("track: no light tiles {}", lights.name);
        }
    }

    // The regions (.ocl): each instance takes its record in order; without the file every region draws it.
    if (const auto ocl = readBytes(files, base / (routeBase + ".ocl"))) m_regions = parseOcl(*ocl);
    if (m_regions) {
        size_t at = 0;
        for (const auto& m : m_meshes) {
            for (Instance& in : m->instances) {
                if (at < m_regions->records.size()) in.regions = m_regions->records[at];
                ++at;
            }
        }
    }
    // The lists of the regions (0x00543610): the instances of a region in the meshes' order, then put in the order
    // of their material's class; the meshes without a material (none of the name) are left out.
    m_lists.assign(256, {});
    for (uint32_t region = 0; region < 256; ++region) {
        std::vector<Entry> all;
        for (const auto& m : m_meshes) {
            if (m->material == nullptr || m->vertices == 0) continue;
            for (Instance& in : m->instances) {
                if ((in.regions[region >> 5] & (1u << (region & 31))) != 0) all.push_back({m.get(), &in});
            }
        }
        std::vector<Entry>& list = m_lists[region];
        for (uint32_t c = 0; c < kClasses; ++c) {
            for (const Entry& e : all) {
                if (e.mesh->classId == c) list.push_back(e);
            }
        }
    }

    // The render tree (0x00548790): each leaf's records the instances of their numbers (0x00543230); those of the
    // meshes without a material are left out, as from the lists.
    if (auto tree = parseRenderTree(world->tree)) {
        m_tree = std::move(*tree);
    } else {
        log::warning("track: the render tree of {} does not read", setup.route);
    }
    std::vector<Entry> numbered;
    for (const auto& m : m_meshes) {
        for (Instance& in : m->instances) numbered.push_back({m.get(), &in});
    }
    // The regions of the nodes (0x00544B90): a leaf's with those of its records' instances, an inner node's with its
    // children's (Stock problem: added to the bytes the file holds there, left over from the tool that wrote it).
    for (size_t n = m_tree.size(); n-- > 0;) {
        RenderTreeNode& node = m_tree[n];
        if (node.leaf) {
            for (uint32_t i : node.instances) {
                if (i >= numbered.size()) continue;
                for (size_t w = 0; w < 8; ++w) node.regions[w] |= numbered[i].instance->regions[w];
            }
        } else {
            for (uint32_t c : node.children) {
                for (size_t w = 0; w < 8; ++w) node.regions[w] |= m_tree[c].regions[w];
            }
        }
    }
    m_treeEntries.assign(m_tree.size(), {});
    for (size_t n = 0; n < m_tree.size(); ++n) {
        for (uint32_t i : m_tree[n].instances) {
            if (i >= numbered.size()) continue;
            const Entry& e = numbered[i];
            if (e.mesh->material == nullptr || e.mesh->vertices == 0) continue;
            m_treeEntries[n].push_back(e);
        }
    }

    if (const auto spl = files.read(base / (routeBase + ".spl"))) {
        m_lines = parseSpl(std::string(reinterpret_cast<const char*>(spl->data()), spl->size()));
    }
    if (const auto cld = readBytes(files, base / (routeBase + ".cld"))) m_collision = parseCld(*cld);
    // The road's height for the camera's splashes (0x00504630, the manager's world's collision).
    if (m_particles && m_collision) m_particles->collision = &*m_collision;
    makeSceneNodes();
    // The emitters of the effects' nodes, each under its node.
    if (m_particles) {
        for (const SceneObjects::Emitter& e : m_sceneObjects.emitterNodes()) {
            std::shared_ptr<Emitter> emitter = m_particles->make(e.name);
            if (!emitter || e.node >= m_sceneNodes.size()) continue;
            if (setup.soundWorld != nullptr) emitter->makeSound(*setup.soundWorld, e.soundDraw);
            emitter->setNode(m_sceneNodes[e.node].get());
            m_sceneEmitters.push_back(std::move(emitter));
        }
    }
    makeFlares();
    log::info("track: scene objects drawn from {} roots, {} model meshes", m_sceneRoots.size(), m_objectMeshes.size());
    if (m_sceneObjects.environmentPlace()) m_environmentPlace = *m_sceneObjects.environmentPlace();
    log::info("track: render tree {} nodes; reflection cube from ({:.0f}, {:.0f}, {:.0f}), reach {:.0f}, reflection "
              "range {:.0f}, testenv {}", m_tree.size(), m_environmentPlace[0], m_environmentPlace[1],
              m_environmentPlace[2], std::sqrt(m_cubeRange), m_reflectionRange, m_testEnvironment ? "on" : "off");

    std::string notDrawn;
    for (const auto& [id, count] : missing) notDrawn += std::format(" {} ({})", id, count);
    log::info("track: {} {}: {} meshes, {} instances, {} textures, {} light tiles, regions {}; classes not drawn yet:{}",
              setup.track, setup.route, m_meshes.size(), instanceCount, m_textures.size(), m_tiles.size(),
              m_regions ? "read" : "none", notDrawn.empty() ? " none" : notDrawn);
    return true;
}

void TrackScene::startObjectLights() {
    m_objectLights.clear();
    const bool night = m_atmosphere && m_atmosphere->current != nullptr && m_atmosphere->current->night != 0;
    for (const SceneObject& o : m_sceneObjects.objects()) {
        const bool lit = o.kind == SceneObject::Kind::Barrel || o.kind == SceneObject::Kind::Bin ||
                         o.kind == SceneObject::Kind::Wheel;
        if (!lit || o.node >= m_sceneNodes.size()) continue;
        render::ModelNode* node = m_sceneNodes[o.node].get();
        m_objectLights.emplace_back(LightStep::Kind::Eased, node);
        LightStep& l = m_objectLights.back();
        // 0x0041ABF0: a barrel's or a bin's trashcan with the 3D world.
        const bool trashcan = o.kind == SceneObject::Kind::Barrel || o.kind == SceneObject::Kind::Bin;
        if (o.kind == SceneObject::Kind::Barrel) l.baseFrequency = 15000.0f;  // 0x00418CE0
        l.barrel = o.kind == SceneObject::Kind::Barrel;
        if (o.kind == SceneObject::Kind::Bin) l.baseFrequency = 30000.0f;     // 0x00429FE0
        if (trashcan && m_soundWorld != nullptr) {
            l.sound = m_soundWorld->create("trashcan", static_cast<render::Node*>(node), 0);
            if (l.sound != nullptr) {
                l.sound->setRolloff(std::bit_cast<float>(0x38D1B717u));  // 0.0001
                l.sound->setLooped(0);
                l.sound->setDoppler(0.0f);
            }
        }
        // 0x0041ACF0: with +0xB0 set a drop shadow (gen_shadow01) under the node, its mesh's box x 1.4 across; no
        // step sets +0xB0.
        constexpr bool kDropShadow = false;
        if (kDropShadow && node->mesh != nullptr) {
            const auto& a = node->mesh->boundsMin();
            const auto& b = node->mesh->boundsMax();
            l.shadow = addProjector(*node, {a[0] * 1.4f, a[1], a[2], 0.0f}, {b[0] * 1.4f, b[1], b[2], 0.0f},
                                    m_engine.texture("gen_shadow01"), nullptr);
        }
        l.start(sunMap(), m_sceneObjects, night);
    }
}

void TrackScene::stepObjectLights(float dt, float frameTime) {
    for (LightStep& l : m_objectLights) {
        l.frame(sunMap(), m_sceneObjects, dt);
        if (l.barrel) barrelStep(l, frameTime);
        if (l.shadow != nullptr && collision() != nullptr) l.shadow->update(*collision());
    }
}

// 0x00418D70, a barrel's step after the light's: past 300 across the ground ([0x00725280]) the effects under its node
// (the children of the effect kind, +0x8C 3) at 0 (vt+0x0C with 0 and 1); past 300 ([0x0072527C]) the first time
// (+0xCC) the cruise ([G+0x80]+0x60) told of a crash of 2000 by no car (0x004B0280: then each zone asked whether it
// holds the barrel's place, the answers not kept).
void TrackScene::barrelStep(LightStep& l, float frameTime) {
    if (l.sideways > 300.0f) {
        for (const std::shared_ptr<Emitter>& e : m_sceneEmitters) {
            if (e->node != nullptr && e->node->parent() == l.node) e->setLevel(0.0f, 1.0f);
        }
    }
    if (l.sideways > 300.0f) {
        if (!l.barrelHit && barrelCrash) barrelCrash(frameTime);
        l.barrelHit = true;
    }
}

Projector* TrackScene::addProjector(render::Node& node, const Vec4& low, const Vec4& high,
                                    const render::GameTexture* shadow, const render::GameTexture* glow) {
    auto p = std::make_unique<Projector>(m_engine, node, low, high, shadow, glow);
    Projector* added = p.get();
    m_projectors.push_back(std::move(p));
    return added;
}

Silhouette* TrackScene::addSilhouette(render::Node& node, int type, Silhouette::DrawCar drawCar) {
    auto s = std::make_unique<Silhouette>(m_engine, node, type, std::move(drawCar));
    Silhouette* added = s.get();
    m_projectors.push_back(std::move(s));
    return added;
}

CarModel* TrackScene::addCar(std::unique_ptr<CarModel> car, const Spl::GridPoint& place) {
    if (!car || car->root() == nullptr) return nullptr;
    // The model's front is +Z; the place's forward is (sin h, 0, cos h).
    const float s = std::sin(place.heading), c = std::cos(place.heading);
    car->root()->local = {c, 0.0f, s, place.x, 0.0f, 1.0f, 0.0f, place.y, -s, 0.0f, c, place.z};
    car->root()->invalidate();
    // The flares of its light nodes join the world's after those made before.
    for (const auto& cf : car->flares()) {
        Flare f;
        f.def = &cf->def;
        f.node = cf->node;
        f.on = &cf->on;
        f.batch = m_lens.batch(cf->def.texture);
        m_lens.makeReflectionBatches(f);
        m_lens.flares().push_back(f);
    }
    if (!car->flares().empty()) m_lens.makeDevice();
    m_cars.push_back(std::move(car));
    return m_cars.back().get();
}

void TrackScene::setLightTile(const render::GameTexture* tile) {
    render::Renderer3d& r = m_engine.renderer();
    if (r.lightTile == tile) return;
    r.lightTile = tile;
    // A material reading the tile (class flag 0x200) is bound again.
    if (const render::Material* current = r.currentMaterial(); current != nullptr && (current->flags() & 0x200) != 0) {
        r.forgetMaterial(current);
    }
}

void TrackScene::drawInstance(render::Camera& camera, const Mesh& mesh, const Instance& in) {
    render::Renderer3d& r = m_engine.renderer();
    render::IRenderDevice& device = r.device();
    device.setStream(mesh.vertices, mesh.stride);
    if (mesh.primitive == 1 || mesh.primitive == 2) device.setIndices(mesh.indices);
    if (mesh.declaration != r.lastDeclaration) {
        device.setDeclaration(mesh.declaration);
        r.lastDeclaration = mesh.declaration;
    }
    const bool hasTile = in.tile >= 0 && static_cast<size_t>(in.tile) < m_tiles.size();
    setLightTile(hasTile ? m_tiles[static_cast<size_t>(in.tile)].get() : nullptr);

    render::Material& material = *mesh.material;
    const render::Program* program = material.program();
    if (program == nullptr || !program->usesFixedVertices()) {
        r.setVertexConstants(0, in.block.data(), 7);
        const oj::Mat44 toClip = oj::multiplyAffine(camera.worldToClip(), in.matrix());
        r.setVertexConstants(18, toClip.data(), 4);
        if (program != nullptr && program->usesRandom()) r.setVertexConstants(40, in.random.data(), 1);
    } else {
        static bool told = false;
        if (!told) log::warning("track: a fixed-function technique's transforms are not set yet");
        told = true;
    }
    const uint32_t passes = material.passes();
    for (uint32_t pass = 0; pass < passes; ++pass) {
        r.beginMaterialPass(material, pass);
        if (mesh.primitive == 1) {
            device.drawIndexed(Primitive::TriangleList, mesh.vertexCount, 0, mesh.indexCount / 3);
        } else if (mesh.primitive == 3) {
            device.draw(Primitive::LineStrip, 0, mesh.vertexCount - 1);
        } else {
            device.drawIndexed(Primitive::TriangleStrip, mesh.vertexCount, 0, mesh.indexCount - 2);
        }
    }
}

void TrackScene::drawRegion(render::Camera& camera, uint32_t region, float lodScale) {
    // 0x00546A0F: the fixed-function lights (0x005463C0) before the region's instances.
    m_engine.renderer().fixedFunctionLights();
    const render::Renderer3d& r = m_engine.renderer();
    const oj::Mat34& c = camera.world();
    for (const Entry& e : m_lists[region]) {
        const Instance& in = *e.instance;
        const Mesh& m = *e.mesh;
        const float dx = c[3] - in.centre[0];
        const float dy = c[7] - in.centre[1];
        const float dz = c[11] - in.centre[2];
        const float d = ((dz * dz + dy * dy) + dx * dx) * lodScale;
        if (!(r.lodBias * m.range[0] < d) || !(d <= r.lodBias * m.range[1])) continue;
        if (in.radius == 0.0f || !camera.sphereVisible(in.matrix(), m.centre, in.radius)) continue;
        if (m.deferred) {
            m_deferred.push_back(e);
        } else {
            drawInstance(camera, m, in);
        }
    }
}

void TrackScene::makeSceneNodes() {
    const std::vector<SceneNode>& nodes = m_sceneObjects.nodes();
    m_sceneNodes.clear();
    m_sceneNodes.reserve(nodes.size());
    for (const SceneNode& n : nodes) {
        auto node = std::make_unique<render::ModelNode>(n.random);
        if (n.mesh && n.file < m_meshOf.size() && n.object < m_meshOf[n.file].size()) {
            node->mesh = m_meshOf[n.file][n.object];
        }
        if (n.hidden) node->flags |= 4;
        m_sceneNodes.push_back(std::move(node));
    }
    // The file's world matrices: a child's own matrix is its parent's inverse times its world matrix.
    auto rowsOf = [](const std::array<float, 16>& m) {
        Mat34 r{};
        for (size_t i = 0; i < 12; ++i) r[i] = m[i];
        return r;
    };
    for (size_t i = 0; i < nodes.size(); ++i) {
        const Mat34 parent = rowsOf(nodes[i].matrix);
        const Mat34 inverse = inverseRigid(parent);
        for (size_t c : nodes[i].children) {
            // A spline's rider takes the matrix of its start on the curve.
            m_sceneNodes[c]->local = nodes[c].placed ? nodes[c].local : multiply(inverse, rowsOf(nodes[c].matrix));
            m_sceneNodes[i]->attach(m_sceneNodes[c].get());
        }
    }
    m_sceneRoots.clear();
    for (size_t root : m_sceneObjects.roots()) m_sceneNodes[root]->local = rowsOf(nodes[root].matrix);
    m_baseLocal.clear();
    for (const auto& n : m_sceneNodes) m_baseLocal.push_back(n->local);
    for (size_t root : m_sceneObjects.roots()) {
        m_sceneNodes[root]->invalidate();
        SceneRoot r;
        r.node = root;
        const SceneNode& n = nodes[root];
        if (n.mesh && n.file < m_sphereOf.size() && n.object < m_sphereOf[n.file].size()) {
            const std::array<float, 5>& s = m_sphereOf[n.file][n.object];
            r.centre = {s[0], s[1], s[2], s[3]};
            r.radius = s[4];
        } else if (n.hasSphere) {
            r.centre = n.sphereCentre;
            r.radius = n.sphereRadius;
        }
        m_sceneRoots.push_back(r);
    }
}

void TrackScene::drawSceneObjects(render::Camera& camera) {
    render::Renderer3d& r = m_engine.renderer();
    const std::vector<SceneNode>& nodes = m_sceneObjects.nodes();
    // The riders of the splines where they moved to; the turning objects turned from their own matrices.
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].placed) {
            m_sceneNodes[i]->local = nodes[i].local;
            m_sceneNodes[i]->invalidate();
        } else if (nodes[i].turning) {
            m_sceneNodes[i]->local = multiply(m_baseLocal[i], nodes[i].turn);
            m_sceneNodes[i]->invalidate();
        }
    }
    // A physics object's node takes its body's matrix (the body's sync, 0x004F2C00).
    for (const ScenePhysics& p : m_sceneObjects.physics()) {
        if (p.node >= m_sceneNodes.size() || p.body == nullptr) continue;
        m_sceneNodes[p.node]->local = p.body->node;
        m_sceneNodes[p.node]->invalidate();
    }
    std::function<void(size_t, uint32_t)> drawTree = [&](size_t i, uint32_t mode) {
        if (nodes[i].mesh) m_sceneNodes[i]->draw(r, camera, mode);
        for (size_t c : nodes[i].children) drawTree(c, mode);
    };
    for (const SceneRoot& root : m_sceneRoots) {
        render::ModelNode& node = *m_sceneNodes[root.node];
        const Vec4 centre = transformPoint(node.world(), root.centre);
        if (!camera.sphereVisible(centre, root.radius)) continue;
        drawTree(root.node, 1);
        drawTree(root.node, 2);
    }
}

void TrackScene::identityObject(render::Camera& camera) {
    oj::scene::identityObject(m_engine.renderer(), camera);
}

void TrackScene::makeFlares() {
    m_lens.clear();
    for (const SceneFlare& def : m_sceneObjects.flares()) {
        if (def.node >= m_sceneNodes.size()) continue;
        Flare f;
        f.def = &def;
        // It takes its node's place under the node's parent: it moves with the parent (its world each frame).
        f.node = m_sceneNodes[def.node].get();
        // The batch of the texture: one Light Flare material each (0x00541210).
        f.batch = m_lens.batch(def.texture);
        m_lens.makeReflectionBatches(f);
        m_lens.flares().push_back(f);
    }
    m_sun.reset();
    if (m_atmosphere && m_atmosphere->current != nullptr && !m_atmosphere->current->sunFlare.empty() &&
        m_engine.texture(m_atmosphere->current->sunFlare) != nullptr) {
        const Atmosphere& a = *m_atmosphere->current;
        m_sun = std::make_unique<SceneFlare>();
        m_sun->texture = a.sunFlare;
        m_sun->size = a.sunFlareSize * 2000.0f;
        m_sun->cosMin = 9.0f;
        m_sun->cosMax = 10.0f;
        m_sun->offset = 0.0f;
        m_sun->screen = 200000.0f;
        m_sun->sunAngle = a.sunAngle;
        // The road reflection's record (no y reflection texture) and, with SunReflection, the reflection on the
        // cars: 200000 x SunReflectionSize x 0.01 across.
        m_sun->roadWidth = 100.0f;
        m_sun->roadHeight = 200.0f;
        m_sun->roadAlphaTop = 128.0f;
        m_sun->roadAlphaBottom = 64.0f;
        m_sun->reflection = a.sunReflection;
        m_sun->reflectionSize = a.sunReflection.empty() ? 300.0f : 200000.0f * a.sunReflectionSize * 0.01f;
        Flare f;
        f.def = m_sun.get();
        f.batch = m_lens.batch(a.sunFlare);
        m_lens.makeReflectionBatches(f);
        m_lens.flares().push_back(f);
    }
    if (m_lens.empty()) return;
    m_lens.makeDevice();
    size_t missing = 0;
    for (size_t b = 0; b < m_lens.batchCount(); ++b) missing += m_lens.batchAt(b).material ? 0 : 1;
    log::info("track: {} lens flares in {} batches ({} without a material), occlusion queries {}",
              m_lens.flares().size(), m_lens.batchCount(), missing, m_lens.queries() ? "on" : "off");
}

void TrackScene::draw(render::Camera& camera, float clock) {
    render::Renderer3d& r = m_engine.renderer();
    render::IRenderDevice& device = m_engine.device();
    camera.update();
    setViewConstants(r, camera);
    device.setRenderState(RenderState::FogEnable, 1);
    device.setRenderState(RenderState::ZEnable, 1);

    // 0x00547690. The world's cube when it is due (Stock problem: its faces' constants stay for the pass).
    Engine::ReflectionTargets& targets = m_engine.reflectionTargets();
    if (targets.worldCubeDue) drawWorldCube(clock);
    if (m_atmosphere) render::applyAtmosphere(r, *m_atmosphere, camera);
    const oj::Mat34& c = camera.world();
    const Region region = regionAt(c[3], c[7], c[11]);
    r.time = m_clock.read(clock);
    r.endMaterial();
    // The cars' environment (the renderer's +0x118): the cars' cube with the reflections on and the renderer's +0x188
    // of 0.8 or more, else the world's cube.
    const GraphicsConfig& settings = graphicsConfig();
    const render::GameTexture* carCube = targets.carCube.id != 0 ? &targets.carCube : nullptr;
    const render::GameTexture* worldCube = targets.worldCube.id != 0 ? &targets.worldCube : nullptr;
    r.dynamicEnvironment = settings.reflections != 0 && !(r.drawQuality < 0.8f) ? carCube : worldCube;
    // The wet road's reflection (the world's +0x19C): the world's cube as +0x11C, the target (+0x164) taken and
    // cleared. Over a face of the route under the camera, with a reflection range, the scene upside down about the
    // highest face: the camera's far plane the range, the culling the other way, the render tree, the sky dome and
    // the instances drawn after the cars.
    PostEffects& post = m_engine.postEffects();
    if (targets.road.id != 0) {
        r.environment = worldCube;
        r.reflection = &targets.road;
        post.takeTarget(targets.road.id);
        device.clear(ClearFlags::Target | ClearFlags::ZBuffer | ClearFlags::Stencil, 0, 1.0f, 0);
        m_cameraPlace = {c[3], c[7], c[11], 0.0f};
        float lowest = 0.0f;
        float highest = -3.4028235e38f;
        if (m_collision) groundSpan(*m_collision, c[3], c[11], lowest, highest);
        m_lodScale = camera.lodScale();
        if (highest > -3.4028235e38f && m_reflectionRange > 0.0f) {
            const float far = camera.farPlane();
            camera.setFar(m_reflectionRange);
            r.defaultCull = 2;
            r.endMaterial();
            const bool reflected = settings.reflections != 0 && r.drawQuality > 0.8f;
            if (reflected) drawReflectedTree(camera, region, highest);
            if (m_sky) m_sky->drawReflected(camera);
            if (reflected) {
                const float twice = highest + highest;
                for (auto it = m_deferred.rbegin(); it != m_deferred.rend(); ++it) {
                    mirror(*it->instance, twice);
                    drawInstance(camera, *it->mesh, *it->instance);
                    mirror(*it->instance, twice);
                }
            }
            m_deferred.clear();
            r.defaultCull = 3;
            r.endMaterial();
            camera.setFar(far);
        }
    }
    // The object constants of no object, the flares' reflections on the road (0x0054FF00), the view's target again
    // (0x00517390).
    identityObject(camera);
    drawFlareRoadReflections(camera);
    post.resume();

    // The scale of the main view's distances, the region the camera is in; the world's +0xA8 (the sky dome) after the
    // region's opaque instances; the instances of classes drawn after the cars go last, the last found first.
    m_cameraPlace = {c[3], c[7], c[11], 0.0f};
    m_lodScale = camera.lodScale() / ((r.drawQuality + 1.0f) * 0.5f);
    if (!m_lists.empty()) drawRegion(camera, region.number, m_lodScale);
    if (m_sky) m_sky->draw(camera);
    r.endMaterial();
    for (auto it = m_deferred.rbegin(); it != m_deferred.rend(); ++it) drawInstance(camera, *it->mesh, *it->instance);
    m_deferred.clear();
    setLightTile(nullptr);
    // After the object constants of no object (0x0054D8D0) the textures laid on the route (the world's +0x98, each
    // its [5]), the trees below the world's node +0xD8 (0x00550500): the scene objects' roots, then the cars' (made
    // after the track), each tree's parts of materials not drawn late (0x24) first and those after (a car's glass over
    // its driver); then again after them the lens flares (+0x120, 0x0054FC60).
    identityObject(camera);
    for (const auto& p : m_projectors) p->draw(camera);
    drawSceneObjects(camera);
    for (const auto& car : m_cars) {
        if (car->hidden) continue;
        car->draw(r, camera, 1);
        car->draw(r, camera, 2);
    }
    r.endMaterial();
    identityObject(camera);
    m_lens.draw(camera);
    // The particles (the effects manager's +0x10, 0x00505030), after the object constants of no object.
    if (m_particles) {
        identityObject(camera);
        m_particles->frame(camera, m_clock.read(clock));
    }
    // The environments and the reflection let go.
    r.dynamicEnvironment = nullptr;
    r.environment = nullptr;
    r.reflection = nullptr;

    r.endMaterial();
    device.setRenderState(RenderState::ZEnable, 0);
    device.setRenderState(RenderState::FogEnable, 0);
}

void drawSplineDebug(const SceneSpline& spline, const Mat34& node, const render::Camera& camera,
                     render::IRenderDevice& device) {
    const auto count = static_cast<float>(spline.path.size());
    const Mat44& m = camera.worldToClip();
    auto screen = [&](const Vec4& p, float out[3]) {
        const float x = m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3];
        const float y = m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7];
        const float w = m[12] * p[0] + m[13] * p[1] + m[14] * p[2] + m[15];
        const float inv = w != 0.0f ? 1.0f / w : 0.0f;
        out[0] = (x * inv * 0.5f + 0.5f) * 640.0f;
        out[1] = (0.5f - y * inv * 0.5f) * 480.0f;
        out[2] = w;
    };
    auto moved = [&](Vec4 p) {
        p[0] = p[0] + node[3];
        p[1] = p[1] + node[7];
        p[2] = p[2] + node[11];
        return p;
    };
    const float sx = static_cast<float>(device.width()) / 640.0f;
    const float sy = static_cast<float>(device.height()) / 480.0f;
    bool red = false;
    for (float t = 0.0f; t < count; t = t + 0.1f) {
        const Vec4 a = moved(spline.pointAt(t));
        const Vec4 b = moved(spline.pointAt(count > t ? t + 0.1f : 0.0f));
        float sa[3];
        float sb[3];
        screen(a, sa);
        screen(b, sb);
        const bool inside = sa[0] > 0.0f && sa[0] < 640.0f && sa[1] > 0.0f && sa[1] < 480.0f && sb[0] > 0.0f &&
                            sb[0] < 640.0f && sb[1] > 0.0f && sb[1] < 480.0f && sa[2] > 10.0f && sb[2] > 10.0f;
        if (inside) device.drawLine(sa[0] * sx, sa[1] * sy, sb[0] * sx, sb[1] * sy, red ? 0xFFFF0000u : 0xFF00FF00u);
        red = !red;
    }
}

}  // namespace oj::scene
