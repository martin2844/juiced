#include "Mods/Creative.h"
#include "Core/Cars/Car.h"
#include "Core/Cars/CarDefs.h"
#include "Mods/Runtime.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
namespace oj::mods {
namespace {
std::filesystem::path root;
uint64_t active = 0;
std::string portraitName, logoName;
bool portraitAsked = false;
std::map<std::string, Neon> neons;
std::optional<std::pair<std::string, Neon>> preview;
std::map<std::string, int> logoSlots;
std::map<std::string, size_t> originalDecalCounts;
std::optional<Request> pending;
bool editingNeon = false;
uint32_t neonEvent = 0;
std::recursive_mutex mutex;
std::filesystem::path folder() {
    return root / std::to_string(active);
}
bool save() {
    std::error_code ec;
    std::filesystem::create_directories(folder(), ec);
    auto path = folder() / "creative.ini";
    auto tmp = path;
    tmp += ".tmp";
    std::ofstream f(tmp);
    f << "portrait " << std::quoted(portraitName) << '\n' << "logo " << std::quoted(logoName) << '\n';
    f << "portraitAsked " << portraitAsked << '\n';
    for (const auto &[model, n] : neons)
        f << "neon " << std::quoted(model) << ' ' << n.rgb << ' ' << n.effect << ' ' << n.speed << '\n';
    f.close();
    if (!f)
        return false;
    std::filesystem::rename(tmp, path, ec);
    return !ec;
}
} // namespace
void configure(std::filesystem::path folder) {
    std::lock_guard lock(mutex);
    root = std::move(folder);
    logoSlots.clear();
    originalDecalCounts.clear();
    preview.reset();
    neonEditor(false);
    profile(0);
}
bool enabled() {
    std::lock_guard lock(mutex);
    return !root.empty();
}
bool feature(const char *name) {
    return enabled() && runtime::feature(name);
}
int effectCount() {
    return int(std::clamp(runtime::number("effect_count", {}, 1), 1., 64.));
}
std::string effectName(int index) {
    return runtime::text("effect_name", {double(index)}, "GLOW");
}
void previewNeon(const Car &car, const Neon &n) {
    std::lock_guard lock(mutex);
    preview = std::pair{car.modelName(), n};
}
void neonEditor(bool value) {
    editingNeon = value;
    neonEvent = 0;
}
void neonKey(uint8_t key) {
    if (editingNeon) {
        if (key == 16)
            neonEvent = 6;
        else if (key == 18)
            neonEvent = 7;
    }
}
uint32_t takeNeonEvent() {
    auto event = neonEvent;
    neonEvent = 0;
    return event;
}
void clearNeonPreview() {
    std::lock_guard lock(mutex);
    preview.reset();
}
uint64_t profileId() {
    std::lock_guard lock(mutex);
    return active;
}
void profile(uint64_t id) {
    std::lock_guard lock(mutex);
    if (!enabled())
        return;
    active = id;
    neons.clear();
    preview.reset();
    portraitName.clear();
    logoName.clear();
    portraitAsked = false;
    std::ifstream f(folder() / "creative.ini");
    std::string kind;
    while (f >> kind) {
        if (kind == "portrait" || kind == "logo") {
            std::string name;
            f >> std::quoted(name);
            if (name.starts_with("mod_image_") &&
                name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") == std::string::npos)
                (kind == "portrait" ? portraitName : logoName) = name;
        } else if (kind == "portraitAsked") {
            f >> portraitAsked;
        } else if (kind == "neon") {
            std::string model;
            Neon n;
            f >> std::quoted(model) >> n.rgb >> n.effect >> n.speed;
            n.rgb &= 0xffffff;
            n.effect = std::clamp(n.effect, 0, 63);
            n.speed = std::clamp(n.speed, 1, 5);
            neons[model] = n;
        } else {
            std::string rest;
            std::getline(f, rest);
        }
    }
}
void profileName(const std::string &name) {
    uint64_t key = 14695981039346656037ull;
    for (unsigned char c : name) {
        key ^= c;
        key *= 1099511628211ull;
    }
    profile(key);
}
void addLogoSlots(CarDefs &defs) {
    if (!feature("logo"))
        return;
    std::lock_guard lock(mutex);
    for (auto &def : defs.cars) {
        auto &parts = def.parts[8].parts;
        if (parts.empty())
            continue;
        originalDecalCounts.try_emplace(def.model, parts.size());
        auto it = std::find_if(parts.begin(), parts.end(),
                               [](const auto &p) { return p.model.find("CUSTOMLOGO") != std::string::npos; });
        const int existing = it == parts.end() ? -1 : int(it - parts.begin());
        const int selected =
            int(std::clamp(runtime::number("logo_slot", {def.model, double(parts.size()), double(existing)}, -1), -1.,
                           double(parts.size())));
        if (selected >= 0 && selected < int(parts.size()))
            logoSlots[def.model] = selected;
        else if (selected == int(parts.size()) && parts.size() < 126) {
            logoSlots[def.model] = selected;
            parts.push_back(parts.front());
        }
    }
}
size_t decalCount(const CarDef &def, size_t actual) {
    std::lock_guard lock(mutex);
    if (!enabled())
        return actual;
    auto it = originalDecalCounts.find(def.model);
    return it == originalDecalCounts.end() ? actual : std::min(actual, it->second);
}
int logoIndex(const std::string &model) {
    auto it = logoSlots.find(model);
    return it == logoSlots.end() ? -1 : it->second;
}
bool customLogo(const Car &car) {
    return feature("logo") && int(car.body[8]) == logoIndex(car.modelName());
}
std::string neonTexture(const Car &car) {
    std::lock_guard lock(mutex);
    if (!feature("neon") ||
        (neons.find(car.modelName()) == neons.end() && (!preview || preview->first != car.modelName())))
        return {};
    auto n = neon(car);
    if ((car.neon & 0xffffff) != n.rgb)
        return {};
    auto texture = runtime::text("neon_texture", {double(n.rgb), double(n.effect), double(n.speed), seconds()});
    if (texture == "mod_neon_glow")
        return texture;
    if (texture.starts_with("mod_neon_scan_")) {
        try {
            int frame = std::stoi(texture.substr(14));
            if (frame >= 0 && frame < 32)
                return "mod_neon_scan_" + std::to_string(frame);
        } catch (...) {
        }
    }
    return {};
}
void requestMenu(std::function<void()> finished) {
    if (enabled())
        pending = Request{false, std::move(finished)};
}
void requestPortrait() {
    if (feature("portrait") &&
        runtime::call("profile_prompt", {portraitAsked, hasImage(true)}, false) == runtime::Value(true)) {
        portraitAsked = true;
        save();
        pending = Request{true};
    }
}
std::optional<Request> takeRequest() {
    auto r = std::move(pending);
    pending.reset();
    return r;
}
Neon neon(const Car &car) {
    std::lock_guard lock(mutex);
    if (preview && preview->first == car.modelName())
        return preview->second;
    auto it = neons.find(car.modelName());
    return it != neons.end() ? it->second : Neon{car.neon & 0xffffff, 0, 2};
}
bool saveNeon(const Car &car, const Neon &n) {
    std::lock_guard lock(mutex);
    auto previous = neons;
    neons[car.modelName()] = n;
    if (save())
        return true;
    neons = std::move(previous);
    return false;
}
double seconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
uint32_t neonColour(const Car &car, double time) {
    if (!feature("neon"))
        return car.neon;
    const auto n = neon(car);
    if ((car.neon & 0xffffff) != n.rgb)
        return car.neon;
    double result =
        runtime::number("neon_colour", {double(n.rgb), double(n.effect), double(n.speed), time}, double(n.rgb));
    auto rgb = uint32_t(std::clamp(result, 0., 16777215.));
    return (car.neon & 0xff000000) | rgb;
}
std::string imageName(bool portrait) {
    std::lock_guard lock(mutex);
    return portrait ? portraitName : logoName;
}
bool hasImage(bool portrait) {
    return !imageName(portrait).empty();
}
std::filesystem::path imageDirectory() {
    std::lock_guard lock(mutex);
    return folder() / "images";
}
bool imageImported(bool portrait, const std::string &name) {
    std::lock_guard lock(mutex);
    auto &current = portrait ? portraitName : logoName;
    auto previous = current;
    current = name;
    if (save())
        return true;
    current = previous;
    return false;
}
bool clearImage(bool portrait) {
    return imageImported(portrait, "");
}
std::string uiTexture(const std::string &name) {
    if (!enabled())
        return name;
    return runtime::text("image_alias",
                         {name, feature("portrait") ? imageName(true) : "", feature("logo") ? imageName(false) : ""},
                         name);
}
std::optional<std::vector<std::byte>> imageBytes(const std::string &name) {
    if (enabled() && name.starts_with("mod_neon_")) {
        int frame = -1;
        if (name.starts_with("mod_neon_scan_")) {
            try {
                frame = std::stoi(name.substr(14));
            } catch (...) {
                return {};
            }
        }
        constexpr int size = 128;
        std::vector<std::byte> out(44 + size * size * 4);
        uint32_t h[11] = {21, size, size, 1, 2, 2, 0, 3, 3, 0, 1};
        std::memcpy(out.data(), h, 44);
        float centre = frame < 0 ? .5f : .5f + .32f * std::sin(float(frame) * 6.2831853f / 32);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x) {
                float u = (x + .5f) / size - .5f, v = (y + .5f) / size - centre;
                float glow = std::exp(-u * u * 15 - v * v * (frame < 0 ? 15 : 130));
                auto at = 44 + (y * size + x) * 4;
                for (int c = 0; c < 3; ++c)
                    out[at + c] = std::byte(255);
                out[at + 3] = std::byte(int(255 * glow));
            }
        return out;
    }
    if (!enabled() || !name.starts_with("mod_image_") ||
        name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos)
        return {};
    std::ifstream f(imageDirectory() / (name + ".ddt"), std::ios::binary | std::ios::ate);
    if (!f)
        return {};
    auto size = f.tellg();
    if (size < 0 || size > 4 * 1024 * 1024)
        return {};
    std::vector<std::byte> bytes(static_cast<size_t>(size));
    f.seekg(0);
    f.read(reinterpret_cast<char *>(bytes.data()), size);
    if (!f)
        return {};
    return bytes;
}
} // namespace oj::mods
