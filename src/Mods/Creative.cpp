#include "Mods/Creative.h"
#include "Core/Cars/Car.h"
#include "Core/Cars/CarDefs.h"
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
std::map<std::string, int> logoSlots;
std::map<std::string, size_t> originalDecalCounts;
std::optional<Request> pending;
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
    profile(0);
}
bool enabled() {
    std::lock_guard lock(mutex);
    return !root.empty();
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
            n.effect = std::clamp(n.effect, 0, 3);
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
    if (!enabled())
        return;
    std::lock_guard lock(mutex);
    for (auto &def : defs.cars) {
        auto &parts = def.parts[8].parts;
        if (parts.empty())
            continue;
        originalDecalCounts.try_emplace(def.model, parts.size());
        auto it = std::find_if(parts.begin(), parts.end(),
                               [](const auto &p) { return p.model.find("CUSTOMLOGO") != std::string::npos; });
        if (it != parts.end())
            logoSlots[def.model] = int(it - parts.begin());
        else if (parts.size() < 126) {
            logoSlots[def.model] = int(parts.size());
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
    return enabled() && int(car.body[8]) == logoIndex(car.modelName());
}
std::string neonTexture(const Car &car) {
    std::lock_guard lock(mutex);
    if (!enabled() || neons.find(car.modelName()) == neons.end())
        return {};
    auto n = neon(car);
    if ((car.neon & 0xffffff) != n.rgb)
        return {};
    if (n.effect != 2)
        return "mod_neon_glow";
    int frame = int(std::fmod(seconds() * .25 * n.speed, 1.) * 32);
    return "mod_neon_scan_" + std::to_string(frame);
}
void requestMenu(std::function<void()> finished) {
    if (enabled())
        pending = Request{nullptr, {}, false, std::move(finished)};
}
void requestPortrait() {
    if (enabled() && !portraitAsked && !hasImage(true)) {
        portraitAsked = true;
        save();
        pending = Request{nullptr, {}, true};
    }
}
void requestNeon(Car &car, std::function<void(const Neon &)> apply) {
    if (enabled())
        pending = Request{&car, std::move(apply), false};
}
std::optional<Request> takeRequest() {
    auto r = std::move(pending);
    pending.reset();
    return r;
}
Neon neon(const Car &car) {
    std::lock_guard lock(mutex);
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
    if (!enabled())
        return car.neon;
    const auto n = neon(car);
    if ((car.neon & 0xffffff) != n.rgb)
        return car.neon;
    const double phase = time * (.25 * n.speed);
    uint32_t rgb = n.rgb;
    float gain = 1;
    if (n.effect == 1)
        gain = .15f + .85f * float(.5 + .5 * std::sin(phase * 6.2831853));

    if (n.effect == 3) {
        rgb = 0;
        for (int i = 0; i < 3; ++i) {
            double h = phase + double(i) / 3;
            float v = float(.5 + .5 * std::cos(h * 6.2831853));
            rgb |= uint32_t(v * 255) << (16 - i * 8);
        }
    }
    uint32_t out = car.neon & 0xff000000;
    for (unsigned shift : {0u, 8u, 16u})
        out |= uint32_t(((rgb >> shift) & 255) * gain) << shift;
    return out;
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
    auto image = imageName(name == "ui_crewmember_00");
    if (name == "ui_crewmember_00" && !image.empty())
        return image;
    if ((name == "ui_chooselogo_01" || name == "mod_custom_logo") && hasImage(false))
        return imageName(false);
    return name;
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
