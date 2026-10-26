#include "Android/SaveEditor.h"
#include "Core/Career/CareerInit.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace oj::saveedit {
std::string quote(const std::string& text) {
    std::string out = "\"";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c < 32) {
            const char* hex = "0123456789abcdef";
            out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15];
        } else out += char(c);
    }
    return out + '"';
}
save::Archive load(GameData& game, const std::vector<std::byte>& bytes) {
    if (bytes.size() < 16 || bytes.size() > 2 * 1024 * 1024)
        throw std::runtime_error("Invalid save size");
    save::Archive archive;
    std::string error;
    if (!archive.read(bytes, &error)) throw std::runtime_error("Invalid save: " + error);
    const auto found = archive.entries.find("CMSGlobal.sav");
    if (found == archive.entries.end()) throw std::runtime_error("This profile has no career yet");
    if (!game.career || !game.career->read(found->second, &error))
        throw std::runtime_error("Cannot read career with this car roster: " + error);
    if (game.career->write() != found->second)
        throw std::runtime_error("Unsupported career format; original save was not changed");
    if (game.career->garage.size() > 55) throw std::runtime_error("Unsupported garage size");
    for (const auto& car : game.career->garage)
        if (std::none_of(game.defs.cars.begin(), game.defs.cars.end(), [&](const auto& def) {
            return def.model == car.modelName();
        })) throw std::runtime_error("Enable the matching car mods before editing this profile");
    return archive;
}
std::string describe(GameData& game, const std::vector<std::byte>& bytes) {
    load(game, bytes);
    const auto& career = *game.career;
    std::string out = "{\"money\":" + std::to_string(career.profile.money) + ",\"respect\":[";
    for (size_t i = 1; i <= 8; ++i) {
        if (i != 1) out += ',';
        out += std::to_string(career.crews[i].respect);
    }
    out += "],\"garage\":[";
    for (size_t i = 0; i < career.garage.size(); ++i) {
        if (i) out += ',';
        const auto model = career.garage[i].modelName();
        auto def = std::find_if(game.defs.cars.begin(), game.defs.cars.end(), [&](const auto& d) { return d.model == model; });
        out += quote(def->displayName);
    }
    return out + "]}";
}
std::vector<uint8_t> edit(GameData& game, const std::vector<std::byte>& bytes, const Changes& changes) {
    auto archive = load(game, bytes);
    auto& career = *game.career;
    if (changes.money < 0 || changes.money > 999999999)
        throw std::runtime_error("Money must be between 0 and 999,999,999");
    for (size_t i = 0; i < changes.respect.size(); ++i)
        if (changes.respect[i] != career.crews[i + 1].respect &&
            (changes.respect[i] < 0 || changes.respect[i] > 1500))
            throw std::runtime_error("Respect must be between 0 and 1,500");
    if (career.garage.size() + changes.addCars.size() > 55)
        throw std::runtime_error("The garage can hold at most 55 cars");
    career.profile.money = changes.money;
    for (size_t i = 1; i <= 8; ++i) {
        career.crews[i].respect = changes.respect[i - 1];
        career.crews[i].respectNow = changes.respect[i - 1];
    }
    for (const auto& model : changes.addCars) {
        auto def = std::find_if(game.defs.cars.begin(), game.defs.cars.end(), [&](const auto& d) { return d.model == model; });
        if (def == game.defs.cars.end()) throw std::runtime_error("Car is not in the selected game roster");
        auto car = makeCar(&*def);
        car.grid = GridDamage{};
        career.garage.push_back(std::move(car));
    }
    if (!changes.addCars.empty()) career.padding = garagePadding(career, game.defs);
    archive.entries["CMSGlobal.sav"] = career.write();
    const auto result = archive.write();
    std::vector<std::byte> check(result.size());
    std::memcpy(check.data(), result.data(), result.size());
    load(game, check);
    return result;
}
}
