#ifdef OJ_MACOS
#include "Mods/Creative.h"
#endif
#include "Game/Cars/CarModel.h"

#include "Core/Base/GameFiles.h"
#include "Core/Base/Log.h"
#include "Core/Formats/Model.h"

#include <bit>
#include <cstring>
#include <functional>
#include <optional>

namespace oj::scene {

namespace {

const float kRandomScale = std::bit_cast<float>(0x38000100u);  // 1 / 32767 (0x006FF7EC)

// The slots' names (0x00717538, in CarModel::Slot's order) and the wheels' (0x0048A0A0).
constexpr const char* kSlotNames[CarModel::kSlotCount] = {"FBUMP",     "RBUMP",     "BONNET",    "SPOILER",
                                                          "SSKIRT_L",  "SSKIRT_R",  "EXHAUST_C", "EXHAUST_S",
                                                          "EXHAUST_O", "DECAL",     "DRIVER"};
constexpr const char* kWheelNames[] = {"WHEEL_FL", "WHEEL_FR", "WHEEL_RL", "WHEEL_RR"};

std::optional<std::vector<uint8_t>> readBytes(const std::filesystem::path& path) {
    const GameFiles files(std::filesystem::current_path());
    const auto bytes = files.read(path);
    if (!bytes) return std::nullopt;
    std::vector<uint8_t> out(bytes->size());
    if (!out.empty()) std::memcpy(out.data(), bytes->data(), out.size());
    return out;
}

}  // namespace

CarModel::~CarModel() = default;

std::unique_ptr<CarModel> CarModel::load(Engine& engine, const Car& car, const PartCatalogue& catalogue,
                                         CrtRandom& random, bool interface) {
    if (car.def == nullptr) return nullptr;
    const CarDef& def = *car.def;
    std::unique_ptr<CarModel> m(new CarModel(engine));
    m->m_name = def.name;
    m->m_folder = std::filesystem::path("cars") / (interface ? def.name + "_ui" : def.name);
    const auto bytes = readBytes(m->m_folder / "models" / (def.name + "_stock.boh"));
    const auto tree = bytes ? parseBoh(*bytes) : std::nullopt;
    if (!tree || tree->empty()) {
        log::warning("3d: car {} not read", def.name);
        return nullptr;
    }

    // The body material (the CarDef's +0x15C, <car>_BODY) with the car's paint (0x0048BF90): the base colour, the
    // metallic colour, the metallic shininess 30 - amount x 25, the pearl colour, the pearl shininess amount x 6 +
    // 0.01.
    auto body = engine.loadMaterial(m->m_folder / "mesh" / "mats", def.name + "_body", m->m_folder / "textures");
    if (body) {
        m->m_body = body.get();
        m->m_materials.push_back(std::move(body));
        m->paint(car);
    }

    // The nodes as the models' (0x00534F50): depth first, four random values per mesh node; "NONE" an empty slot.
    m->m_model = std::make_unique<Model>();
    Model& model = *m->m_model;
    std::function<render::Node*(const BohNode&, render::Node*)> build = [&](const BohNode& b,
                                                                            render::Node* parent) -> render::Node* {
        render::Node* node = nullptr;
        if (b.attach == 0) {
            oj::Vec4 r{};
            for (float& v : r) v = static_cast<float>(random.next()) * kRandomScale;
            auto n = std::make_unique<render::ModelNode>(r);
            if (_stricmp(b.mesh.c_str(), "NONE") != 0) n->mesh = m->mesh(b.mesh, Folders::Car);
            node = n.get();
            if (parent != nullptr) m->m_tree.emplace_back(b.name, n.get(), n.get());
            model.meshNodes.push_back(std::move(n));
        } else {
            auto n = std::make_unique<render::Node>();
            node = n.get();
            if (parent != nullptr) m->m_tree.emplace_back(b.name, n.get(), nullptr);
            model.otherNodes.push_back(std::move(n));
            if (b.attach == 2 && !b.light.empty() && !interface) {
                if (std::optional<SceneFlare> f = lightFlare(ObjectProperties(b.light), false)) {
                    m->m_flares.push_back(std::make_unique<CarFlare>(CarFlare{std::move(*f), node, true}));
                }
            }
        }
        std::copy_n(b.matrix.begin(), 12, node->local.begin());
        if (parent != nullptr) parent->attach(node);
        node->invalidate();
        for (const BohNode& child : b.children) build(child, node);
        return node;
    };
    for (const BohNode& top : *tree) {
        render::Node* n = build(top, nullptr);
        if (model.root == nullptr) model.root = n;
    }
    // 0x0048A0A0: the wheels' and the parts' slots.
    for (size_t i = 0; i < m->m_wheels.size(); ++i) m->m_wheels[i] = m->slotNode(kWheelNames[i]);
    for (size_t i = 0; i < kSlotCount; ++i) m->m_slots[i] = m->slotNode(kSlotNames[i]);

    // The stock's model points at the record ([model+0x8C]) and its fitting (0x0048BC30) writes the index it fits
    // back into it: the record is the caller's own (no caller passes a const object).
    m->fitParts(const_cast<Car&>(car), catalogue);
    // 0x0048C2E0 after the body parts (0x0048C3B5).
    m->primerToBody();
    // The right wheels face the other way: the race turns them each frame (0x00487670), the interface's car view
    // itself (0x004590E0).
    // 0x0048B860: the driver.
    m->fill(kDriver, "DRIVER", Folders::Shared);
    // 0x0048A0A0 colours the wheels (0x0048C440); 0x0048C2E0 tints the glass as 0x0048C040.
    m->colourWheels(car);
    m->colourDecal(car);
    m->tint(car);
    m->sponsorDecals(car, catalogue);
    log::info("3d: car {}: {} meshes, {} materials", def.name, m->m_meshes.size(), m->m_materials.size());
    return m;
}

// The body parts, the wheels and the exhaust of the car put in the model's slots (in CarModel::load).
void CarModel::fitParts(Car& car, const PartCatalogue& catalogue) {
    if (car.def == nullptr) return;
    const CarDef& def = *car.def;
    // The body parts (0x0048C2E0, 0x0048BC30): the chosen part of each category; none chosen takes the first part
    // when the list has one and it is a STANDARD one (the list's +0x14). The index fitted is written back to the
    // record (body[category]), so a new car with none chosen (0x0048DB40) keeps its standard parts and no decal (the
    // decals are CUSTOM parts alone).
    for (size_t category = 0; category < 9; ++category) {
        const CarPartList& list = def.parts[category];
        const auto chosen = static_cast<int8_t>(car.body[category]);
        int index = 0;
        if (chosen == -1) {
            if (list.parts.empty() || !list.standard) continue;
            index = 0;
        } else if (chosen == 0x7F) {
            index = 0x7F;
        } else if (chosen < 0 || static_cast<size_t>(chosen) >= list.parts.size()) {
            continue;
        } else {
            index = chosen;
        }
        if (list.parts.empty()) continue;
        car.body[category] = static_cast<uint8_t>(index);
        // 0x00447690: the part's mesh name; none for 0x7F.
        const std::string part = index != 0x7F && static_cast<size_t>(index) < list.parts.size()
                                     ? list.parts[static_cast<size_t>(index)].model
                                     : std::string();
        fillBody(static_cast<int>(category), part);
    }
    fitWheelsAndExhaust(car, catalogue);
}

void CarModel::primerToBody() {
    std::string key = m_name + "_primer";
    for (char& ch : key) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    const auto found = m_partMaterials.find(key);
    if (found == m_partMaterials.end()) return;
    for (render::ModelNode* slot : m_slots) {
        if (slot == nullptr || slot->mesh == nullptr) continue;
        for (size_t i = 0; i < slot->mesh->parts().size(); ++i) {
            if (slot->mesh->parts()[i].material == found->second) slot->mesh->setMaterial(i, m_body);
        }
    }
}

void CarModel::refitParts(Car& car, const PartCatalogue& catalogue) {
    if (car.def == nullptr) return;
    for (size_t category = 0; category < 9; ++category) {
        const CarPartList& list = car.def->parts[category];
        if (list.parts.empty()) continue;
        const auto chosen = static_cast<int8_t>(car.body[category]);
        const std::string part = chosen >= 0 && chosen != 0x7F && static_cast<size_t>(chosen) < list.parts.size()
                                     ? list.parts[static_cast<size_t>(chosen)].model
                                     : std::string();
        fillBody(static_cast<int>(category), part);
    }
    fitWheelsAndExhaust(car, catalogue);
}

void CarModel::fitWheelsAndExhaust(const Car& car, const PartCatalogue& catalogue) {
    if (car.def == nullptr) return;
    const CarDef& def = *car.def;
    // The wheels (0x0048ABC0): the CarDef's STANDARDWHEELS from the car's folders, else the catalogue wheel's mesh
    // from the shared ones (one past the list: the wheels as they were); the same mesh front and rear.
    const uint8_t chosenWheel = car.mods[kWheel];
    if (chosenWheel == kNoMod) {
        fillWheels(def.wheels, Folders::Car);
    } else if (chosenWheel < catalogue.lists[kWheel].parts.size()) {
        fillWheels(catalogue.lists[kWheel].parts[chosenWheel].model, Folders::Shared);
    }
    // The exhaust (0x0048ABC0, from the shared folders): the CarDef's STANDARDEXHAUST, else the catalogue exhaust's
    // SHAPE; 0x0048B3C0 puts the one mesh in its slots, none (a car without a STANDARDEXHAUST, a part past the list,
    // a mesh not read) leaving them as they are.
    std::string exhaust;
    const uint8_t chosenExhaust = car.mods[kExhaust];
    if (chosenExhaust == kNoMod) {
        exhaust = def.exhaust;
    } else if (chosenExhaust < catalogue.lists[kExhaust].parts.size()) {
        exhaust = catalogue.lists[kExhaust].parts[chosenExhaust].model;
    }
    if (render::Mesh* m = mesh(exhaust, Folders::Shared); m != nullptr) {
        for (Slot slot : {kExhaustSide, kExhaustCentre, kExhaustOpposite}) {
            if (m_slots[slot] != nullptr) m_slots[slot]->mesh = m;
        }
    }
}

render::Mesh* CarModel::mesh(const std::string& name, Folders folders, bool quiet) {
    if (name.empty()) return nullptr;
    const bool shared = folders == Folders::Shared;
    const std::filesystem::path carMesh = std::filesystem::path("carmesh");
    const std::vector<std::filesystem::path> meshFolders =
        shared ? std::vector{carMesh} : std::vector{m_folder / "mesh", carMesh, std::filesystem::path("mesh")};
    const std::vector<std::filesystem::path> matsFolders =
        shared ? std::vector{carMesh / "mats"}
               : std::vector{m_folder / "mesh" / "mats", carMesh / "mats", std::filesystem::path("mesh") / "mats"};
    const std::filesystem::path textures = shared ? std::filesystem::path("cartextures") : m_folder / "textures";
    std::optional<std::vector<uint8_t>> bytes;
    for (const std::filesystem::path& folder : meshFolders) {
        bytes = readBytes(folder / (name + ".d8m"));
        if (bytes) break;
    }
    const auto parsed = bytes ? parseMesh(*bytes) : std::nullopt;
    if (!parsed) {
        if (!quiet) log::warning("3d: car {}: mesh {} not read", m_name, name);
        return nullptr;
    }
    std::vector<render::Material*> materials;
    for (const MeshPart& p : parsed->parts) {
        if (m_body != nullptr && _stricmp(p.name.c_str(), (m_name + "_body").c_str()) == 0) {
            materials.push_back(m_body);
            continue;
        }
        std::string key = p.name;
        for (char& ch : key) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        // The stock's material manager ([G+0x10], its +0x18 by the name) keeps one material of a name: a part of a
        // name the car has a material of already takes that one (the wheel's mesh and its blur alike, so the wheel's
        // colour, 0x0048C440, is on both).
        if (const auto known = m_partMaterials.find(key); known != m_partMaterials.end()) {
            materials.push_back(known->second);
            continue;
        }
        std::unique_ptr<render::Material> material;
        for (const std::filesystem::path& mats : matsFolders) {
            if (GameFiles(std::filesystem::current_path()).exists(mats / (p.name + ".pmd"))) {
                material = m_engine.loadMaterial(mats, p.name, textures, "cartextures");
                break;
            }
        }
        materials.push_back(material.get());
        if (material) {
            m_partMaterials.emplace(key, material.get());
            m_materials.push_back(std::move(material));
        }
    }
    m_meshes.push_back(std::make_unique<render::Mesh>(m_engine.device(), *parsed, materials));
    std::vector<DamageVertices>& vertices = m_vertices[m_meshes.back().get()];
    vertices.resize(parsed->parts.size());
    for (size_t k = 0; k < parsed->parts.size(); ++k) {
        const MeshPart& p = parsed->parts[k];
        vertices[k].stride = p.stride;
        vertices[k].bytes.assign(p.vertices.begin(), p.vertices.end());
        vertices[k].original = vertices[k].bytes;
    }
    return m_meshes.back().get();
}

std::optional<std::vector<DamagePoint>> CarModel::damagePoints(const char* text) {
    for (const auto& [name, node, meshNode] : m_tree) {
        if (std::strstr(name.c_str(), text) == nullptr) continue;
        std::vector<DamagePoint> points;
        if (meshNode == nullptr || meshNode->mesh == nullptr) return points;
        const auto found = m_vertices.find(meshNode->mesh);
        if (found == m_vertices.end()) return points;
        const float x = node->local[3];
        const float y = node->local[7];
        const float z = node->local[11];
        for (DamageVertices& part : found->second) {
            if (part.stride < 12) continue;
            const auto count = static_cast<uint32_t>(part.bytes.size() / part.stride);
            for (uint32_t v = 0; v < count; ++v) {
                float q[3];
                std::memcpy(q, part.bytes.data() + static_cast<size_t>(v) * part.stride, sizeof(q));
                // A part without the normal and the colour (none of the cars') is not moved.
                DamageVertex vertex{part.stride >= 28 ? &part : nullptr, v};
                points.push_back({{q[0] + x, q[1] + y, q[2] + z, 0.0f}, vertex});
            }
        }
        return points;
    }
    return std::nullopt;
}

float CarModel::nodeRadius(const char* text) const {
    for (const auto& [name, node, meshNode] : m_tree) {
        if (std::strstr(name.c_str(), text) == nullptr) continue;
        return meshNode != nullptr && meshNode->mesh != nullptr ? meshNode->mesh->radius() : 0.0f;
    }
    return 0.0f;
}

render::ModelNode* CarModel::meshNode(const char* text) {
    for (const auto& [name, node, meshNode] : m_tree) {
        if (std::strstr(name.c_str(), text) != nullptr) return meshNode;
    }
    return nullptr;
}

CarFlare* CarModel::flare(const char* text) {
    render::Node* found = node(text);
    if (found == nullptr) return nullptr;
    for (const auto& f : m_flares) {
        if (f->node == found) return f.get();
    }
    return nullptr;
}

render::Node* CarModel::node(const char* text) {
    for (const auto& [name, node, meshNode] : m_tree) {
        if (std::strstr(name.c_str(), text) != nullptr) return node;
    }
    return nullptr;
}

const std::string* CarModel::nameOf(const render::Node* node) const {
    for (const auto& [name, n, meshNode] : m_tree) {
        if (n == node || meshNode == node) return &name;
    }
    return nullptr;
}

bool CarModel::hasNode(const char* text) const {
    for (const auto& [name, node, meshNode] : m_tree) {
        if (std::strstr(name.c_str(), text) != nullptr) return true;
    }
    return false;
}

render::ModelNode* CarModel::slotNode(const char* name) {
    for (const auto& [n, node, meshNode] : m_tree) {
        if (std::strstr(n.c_str(), name) != nullptr) return meshNode;
    }
    return nullptr;
}

void CarModel::fill(Slot slot, const std::string& meshName, Folders folders) {
    render::ModelNode* node = m_slots[slot];
    if (node == nullptr) return;
    node->mesh = mesh(meshName, folders);
}

void CarModel::fillWheels(const std::string& meshName, Folders folders) {
    m_wheelMesh = mesh(meshName, folders);
    for (render::ModelNode* node : m_wheels) {
        if (node != nullptr) node->mesh = m_wheelMesh;
    }
    m_wheelBlurMesh = meshName.empty() ? nullptr : mesh(meshName + "_BLUR", folders, true);
    if (m_wheelBlurMesh == nullptr) m_wheelBlurMesh = mesh("RX7_WHEEL_BLUR", folders, true);
    m_wheelMaterials = {};
    if (folders != Folders::Shared) return;
    std::array<std::string, 5> names{meshName, meshName + "_BS", meshName + "_B", meshName + "_LOD", ""};
    if (const size_t first = meshName.find('_'); first != std::string::npos) {
        names[4] = meshName.substr(0, first) + "_BL" + meshName.substr(first);
        names[4].erase(names[4].rfind('_'));
    }
    for (size_t i = 0; i < names.size(); ++i) {
        std::string key = names[i];
        for (char& ch : key) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        const auto it = m_partMaterials.find(key);
        if (it == m_partMaterials.end() || it->second == nullptr) {
            m_wheelColours[i] = 0xFFFFFFFF;
            continue;
        }
        m_wheelMaterials[i] = it->second;
        const std::vector<std::vector<uint8_t>>& description = it->second->description();
        if (description.empty() || description[0].size() < sizeof(uint32_t)) continue;
        std::memcpy(&m_wheelColours[i], description[0].data(), sizeof(uint32_t));
        it->second->setParam(0, &m_wheelColours[i]);
    }
}

render::ModelNode* CarModel::wheelNode(size_t i) {
    return i < m_wheels.size() ? m_wheels[i] : nullptr;
}

void CarModel::blurWheel(size_t i, bool blurred) {
    if (render::ModelNode* node = wheelNode(i)) node->mesh = blurred ? m_wheelBlurMesh : m_wheelMesh;
}

void CarModel::fillBody(int category, const std::string& meshName) {
    if (category >= 0 && category < 4) {
        fill(static_cast<Slot>(category), meshName, Folders::Car);
    } else if (category == 4) {
        fill(kSkirtLeft, meshName.empty() ? meshName : meshName + "_L", Folders::Car);
        fill(kSkirtRight, meshName.empty() ? meshName : meshName + "_R", Folders::Car);
    } else if (category == 8) {
        fill(kDecal, meshName, Folders::Car);
    }
}

void CarModel::paint(const Car& car) {
    if (m_body == nullptr) return;
    auto colour = [this](uint32_t index, uint32_t argb) { m_body->setParam(index, &argb); };
    auto number = [this](uint32_t index, float v) { m_body->setParam(index, &v); };
    colour(0, car.base);
    colour(3, car.metallic);
    number(5, 30.0f - Car::asFloat(car.metallicAmount) * 25.0f);
    colour(4, car.pearl);
    number(6, Car::asFloat(car.pearlAmount) * 6.0f + 0.01f);
}

void CarModel::colourWheels(const Car& car) {
    if (car.wheelsColoured == 0) return;
    for (render::Material* material : m_wheelMaterials) {
        if (material != nullptr) material->setParam(0, &car.wheelColour);
    }
}

void CarModel::restoreWheelColours() {
    for (size_t i = 0; i < m_wheelMaterials.size(); ++i) {
        if (m_wheelMaterials[i] != nullptr) m_wheelMaterials[i]->setParam(0, &m_wheelColours[i]);
    }
}

void CarModel::keepWheelColours(const Car& car) {
    for (size_t i = 0; i < m_wheelMaterials.size(); ++i) {
        if (m_wheelMaterials[i] != nullptr) m_wheelColours[i] = car.wheelColour;
    }
}

void CarModel::colourDecal(const Car& car) {
    if (m_slots[kDecal] == nullptr || car.def == nullptr) return;
#ifdef OJ_MACOS
    if(mods::enabled() && m_slots[kDecal]->mesh) {
        const bool custom=mods::customLogo(car) && mods::hasImage(false);
        const auto* imported=custom?m_engine.texture(mods::imageName(false),"cartextures"):nullptr;
        for(const auto& p:m_slots[kDecal]->mesh->parts()) {
            if(!p.material || (p.material->classId()!=kColouredSpecABlend && p.material->classId()!=kStandardAlphaBlend))continue;
            auto [original,added]=m_originalDecalTextures.try_emplace(p.material,p.material->texture(0));
            p.material->setTexture(0,imported?imported:original->second);
            if(imported && p.material->classId()==kColouredSpecABlend){const uint32_t white=0xffffffff;p.material->setParam(0,&white);}
        }
        if(imported)return;
    }
#endif
    const auto part = static_cast<int8_t>(car.body[8]);
    const std::vector<CarPart>& parts = car.def->parts[8].parts;
    if (part < 0 || part == 0x7F || static_cast<size_t>(part) >= parts.size()) return;
    const char* decal = std::strstr(parts[static_cast<size_t>(part)].model.c_str(), "DECAL");
    if (decal == nullptr) return;
    std::string key = decal;
    for (char& ch : key) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    const auto it = m_partMaterials.find(key);
    if (it == m_partMaterials.end() || it->second->classId() != kColouredSpecABlend) return;

    it->second->setParam(0, &car.decalColour);
}

void CarModel::sponsorDecals(const Car& car, const PartCatalogue& catalogue) {
    if (!hasNode("SPONS")) return;
    std::array<render::Material*, 9> decals{};
    for (size_t k = 0; k < decals.size(); ++k) {
        const auto it = m_partMaterials.find(std::format("decal_spons_{}", k + 1));
        if (it != m_partMaterials.end()) decals[k] = it->second;
    }
    const render::GameTexture* alpha2 = m_engine.texture("alpha2", "cartextures");
    for (render::Material* material : decals) {
        if (material != nullptr) material->setTexture(0, alpha2);
    }
    if (car.sponsorDecals == 0) return;
    const std::vector<ExeTables::Brand>& brands = m_engine.tables().brands;
    for (size_t k = 0; k < decals.size(); ++k) {
        if (decals[k] == nullptr || car.mods[k] == kNoMod) continue;
        const std::vector<CarPart>& parts = catalogue.lists[k].parts;
        if (parts.empty()) continue;
        const auto index = static_cast<int8_t>(car.mods[k]);
        const bool inList = index >= 0 && static_cast<size_t>(index) < parts.size();
        const CarPart& part = inList ? parts[static_cast<size_t>(index)] : parts[0];
        const int brand = part.brand;
        const std::string name =
            brand >= 0 && static_cast<size_t>(brand) < brands.size() ? brands[static_cast<size_t>(brand)].name : "";
        if (const render::GameTexture* logo = m_engine.texture("decal_" + name, "cartextures")) {
            decals[k]->setTexture(0, logo);
        }
        if (decals[k]->classId() == kColouredSpecABlend) decals[k]->setParam(0, &car.sponsorColour);
    }
}

void CarModel::tint(const Car& car) {
    std::string key = m_name + "_glass";
    for (char& ch : key) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    const auto it = m_partMaterials.find(key);
    if (it == m_partMaterials.end()) return;
    const uint32_t black = 0;
    it->second->setParam(5, &black);
    it->second->setParam(2, &car.tint);
}

void CarModel::fillExhaust(const std::string& meshName) {
    for (Slot slot : {kExhaustSide, kExhaustCentre, kExhaustOpposite}) fill(slot, meshName, Folders::Shared);
}

void CarModel::mirror() {
    if (!m_model || m_model->root == nullptr) return;
    for (auto& node : m_model->meshNodes) node->flags |= 1;
    Mat34& m = m_model->root->local;
    m[1] = -m[1];
    m[5] = -m[5];
    m[9] = -m[9];
    m[7] = -m[7];
    m_model->root->invalidate();
}

void CarModel::drawBelow(const render::Node* top, render::Renderer3d& r, render::Camera& camera, uint32_t mode) {
    if (!m_model || top == nullptr) return;
    for (auto& node : m_model->meshNodes) {
        for (const render::Node* n = node.get(); n != nullptr; n = n->parent()) {
            if (n != top) continue;
            node->draw(r, camera, mode);
            break;
        }
    }
}

render::ModelNode* CarModel::addMeshNode(render::Node& parent, render::Mesh* mesh, const Vec4& random) {
    if (!m_model) return nullptr;
    auto made = std::make_unique<render::ModelNode>(random);
    made->mesh = mesh;
    render::ModelNode* node = made.get();
    parent.attach(node);
    node->invalidate();
    // After the last mesh node at or below the parent, else last.
    auto below = [&parent](const render::Node* n) {
        for (; n != nullptr; n = n->parent()) {
            if (n == &parent) return true;
        }
        return false;
    };
    auto& nodes = m_model->meshNodes;
    size_t at = nodes.size();
    for (size_t k = nodes.size(); k > 0; --k) {
        if (below(nodes[k - 1].get())) {
            at = k;
            break;
        }
    }
    nodes.insert(nodes.begin() + static_cast<std::ptrdiff_t>(at), std::move(made));
    return node;
}

void CarModel::undent() {
    for (auto& [mesh, parts] : m_vertices) {
        for (DamageVertices& part : parts) {
            if (part.bytes == part.original) continue;
            part.bytes = part.original;
            part.changed = true;
        }
    }
}

void CarModel::draw(render::Renderer3d& r, render::Camera& camera, uint32_t mode) {
    if (!m_model) return;
    // The parts the dents moved (0x005556C0 writes them into the locked vertex buffers at once).
    for (auto& [mesh, parts] : m_vertices) {
        for (size_t k = 0; k < parts.size() && k < mesh->parts().size(); ++k) {
            DamageVertices& part = parts[k];
            if (!part.changed) continue;
            part.changed = false;
            r.device().updateVertexBuffer(mesh->parts()[k].vertices, part.bytes.data(),
                                          static_cast<uint32_t>(part.bytes.size()));
        }
    }
    for (auto& node : m_model->meshNodes) node->draw(r, camera, mode);
}

}  // namespace oj::scene
