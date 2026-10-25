#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>
namespace oj {
struct Car;
struct CarDefs;
struct CarDef;
} // namespace oj
namespace oj::mods {
struct Neon {
    uint32_t rgb = 0x00ff80;
    int effect = 0;
    int speed = 2;
};
struct Request {
    bool onboarding = false;
    std::function<void()> finished;
    bool management = false;
};
void configure(std::filesystem::path folder);
bool enabled();
bool feature(const char *name);
int effectCount();
std::string effectName(int index);
void previewNeon(const Car &car, const Neon &value);
void clearNeonPreview();
void neonEditor(bool active);
void neonKey(uint8_t code);
uint32_t takeNeonEvent();
void profile(uint64_t id);
void profileName(const std::string &name);
void addLogoSlots(CarDefs &defs);
bool customLogo(const Car &car);
int logoIndex(const std::string &model);
// Original catalogue extent for stock crew liveries and automatic upgrades.
size_t decalCount(const CarDef &def, size_t actual);
std::string neonTexture(const Car &car);
uint64_t profileId();
void requestMenu(std::function<void()> finished = {});
void requestPortrait();
void requestManager();
std::optional<Request> takeRequest();
Neon neon(const Car &car);
bool saveNeon(const Car &car, const Neon &value);
uint32_t neonColour(const Car &car, double seconds);
double seconds();
std::string imageName(bool portrait);
std::string uiTexture(const std::string &name);
std::optional<std::vector<std::byte>> imageBytes(const std::string &name);
bool hasImage(bool portrait);
bool clearImage(bool portrait);
// Platform adapter: imports a chosen image into a bounded, native texture.
std::string importImage(bool portrait);
std::string importImageFile(const std::filesystem::path &path, bool portrait);
std::filesystem::path imageDirectory();
bool imageImported(bool portrait, const std::string &name);
} // namespace oj::mods
