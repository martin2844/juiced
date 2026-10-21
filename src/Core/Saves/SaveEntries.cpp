#include "Core/Saves/SaveEntries.h"

#include <cstring>
#include <initializer_list>

namespace oj::save {

namespace {

uint32_t u32At(const std::vector<uint8_t>& d, size_t o) {
    uint32_t v = 0;
    std::memcpy(&v, d.data() + o, 4);
    return v;
}

void putU32(std::vector<uint8_t>& out, uint32_t v) {
    const auto* p = reinterpret_cast<const uint8_t*>(&v);
    out.insert(out.end(), p, p + 4);
}

}  // namespace

bool UiHelp::read(const std::vector<uint8_t>& data) {
    if (data.size() % 4 != 0) {
        return false;
    }
    shown.clear();
    for (size_t o = 0; o < data.size(); o += 4) {
        shown.push_back(u32At(data, o));
    }
    return true;
}

std::vector<uint8_t> UiHelp::write() const {
    std::vector<uint8_t> out;
    for (uint32_t v : shown) putU32(out, v);
    return out;
}

bool ArcadeUnlock::read(const std::vector<uint8_t>& data) {
    constexpr size_t kTail = kCarFlagsSize + kRouteFlagsSize + 4 + 4;
    constexpr size_t kSeries = 5 * 8;
    if (data.size() < kTail || (data.size() - kTail) % kSeries != 0) {
        return false;
    }
    series.resize((data.size() - kTail) / kSeries);
    size_t o = 0;
    for (auto& s : series) {
        for (Race& race : s) {
            race.locked = u32At(data, o);
            race.completed = u32At(data, o + 4);
            o += 8;
        }
    }
    carFlags.assign(data.begin() + o, data.begin() + o + kCarFlagsSize);
    o += kCarFlagsSize;
    routeFlags.assign(data.begin() + o, data.begin() + o + kRouteFlagsSize);
    o += kRouteFlagsSize;
    developersOpen = u32At(data, o);
    extremeOpen = u32At(data, o + 4);
    return true;
}

std::vector<uint8_t> ArcadeUnlock::write() const {
    std::vector<uint8_t> out;
    for (const auto& s : series) {
        for (const Race& race : s) {
            putU32(out, race.locked);
            putU32(out, race.completed);
        }
    }
    out.insert(out.end(), carFlags.begin(), carFlags.end());
    out.insert(out.end(), routeFlags.begin(), routeFlags.end());
    putU32(out, developersOpen);
    putU32(out, extremeOpen);
    return out;
}

const std::vector<std::pair<uint32_t, uint32_t>>& Settings::layout() {
    static const std::vector<std::pair<uint32_t, uint32_t>> kLayout = {
        {0x00, 4},  {0x04, 4},   {0x08, 4},   {0x0C, 4},   {0x10, 4},   {0x14, 4},   {0x18, 4},   {0x1C, 4},
        {0x20, 4},  {0x24, 4},   {0x2C, 4},   {0x28, 4},   {0x30, 4},   {0x34, 4},   {0x38, 4},   {0x3C, 4},
        {0x40, 4},  {0x44, 4},   {0x78, 4},   {0x7C, 4},   {0x80, 4},   {0x84, 4},   {0x88, 4},   {0x8C, 4},
        {0x90, 15}, {0xE4, 62},  {0x122, 102}, {0x188, 62}, {0x208, 16}, {0x218, 4},  {0x21C, 56}, {0x254, 20},
        {0x268, 72}, {0x2B0, 36}, {0x60, 8},   {0x58, 8},   {0x50, 8},   {0x68, 8},   {0x70, 8},   {0xA0, 68},
        {0x4C, 4},  {0x2D4, 64}, {0x314, 64},
    };
    return kLayout;
}

Settings Settings::defaults(int languageIndex) {
    Settings s;
    auto f32 = [&](uint32_t offset, float v) {
        uint32_t bits = 0;
        std::memcpy(&bits, &v, 4);
        s.setU32(offset, bits);
    };
    auto u16 = [&](uint32_t offset, uint16_t v) { std::memcpy(s.object.data() + offset, &v, 2); };
    constexpr uint32_t kNone = 0xFFFFFFFF;
    constexpr uint32_t kUnset = 9999;
    // The constructor (0x00409750): the crew's paint scheme's base, metallic, pearl and their amounts, then the
    // defaults (0x004097F0).
    s.setU32(kScheme, kNone);
    s.setU32(kScheme + 4, 0xFF000000);
    s.setU32(kScheme + 8, kNone);
    f32(kScheme + 12, 0.5f);
    f32(kScheme + 16, 0.1f);
    f32(kMusicVolume, 0.9f);
    f32(kEffectsVolume, 0.75f);
    f32(kSpeechVolume, 1.0f);
    s.setU32(kSpeakers, 1);
    f32(kCustomDifficulty, 3.0f);
    s.setU32(kCustomLaps, 3);
    s.setU32(kCustomOpponents, 2);
    using Offsets = std::initializer_list<uint32_t>;
    for (uint32_t o : Offsets{0x1C, kAids, kMap, kAutomatic, kAutomatic + 4, kOnline, kVoiceChat}) s.setU32(o, 1);
    for (uint32_t o : Offsets{kLogo, kSteeringWheel, kView, kView + 4, kDifficulty}) s.setU32(o, 0);
    for (uint32_t i : {5u, 6u, 9u, 10u, 11u, 12u, 14u, 15u, 16u}) s.setU32(kScheme + 4 * i, kNone);
    s.setU32(kMph, languageIndex == 0 ? 1 : 0);
    for (uint32_t o : Offsets{kWheelForce, kWheelForce + 4, 0x68, 0x6C}) s.setU32(o, 1);
    for (uint32_t o : Offsets{kWheelStrength, kWheelStrength + 4, kSensitivity, kSensitivity + 4}) f32(o, 0.5f);
    u16(kOnlineNick, 0);
    u16(kOnlineEmail, 0);
    u16(kOnlinePassword, 0);
    s.setU32(kCustomTimeOfDay, 2);
    s.setU32(kCustomWeather, 1);
    std::memcpy(s.object.data() + kCrewName, "The Z's", 8);
    for (uint32_t i = 0; i < 4; ++i) s.setU32(kSongs + 4 * i, kNone);
    u16(0x220, 0);
    u16(0x26C, 0);
    for (uint32_t o : {0x24Cu, 0x250u, 0x254u, 0x294u, 0x2ACu, 0x2B0u}) s.setU32(o, 1);
    for (uint32_t o : {0x248u, 0x258u, 0x25Cu, 0x260u, 0x264u, 0x2B4u, 0x2B8u, 0x2C4u, 0x2C8u, 0x2D0u}) {
        s.setU32(o, kUnset);
    }
    s.setU32(0x298, 7);
    s.setU32(0x2BC, 7);
    std::memcpy(s.object.data() + kVoiceInput, "none", 5);
    std::memcpy(s.object.data() + kVoiceOutput, "none", 5);
    return s;
}

uint32_t Settings::u32(uint32_t offset) const {
    uint32_t v = 0;
    std::memcpy(&v, object.data() + offset, 4);
    return v;
}

void Settings::setU32(uint32_t offset, uint32_t value) {
    std::memcpy(object.data() + offset, &value, 4);
}

std::wstring Settings::wideText(uint32_t offset, size_t chars) const {
    std::wstring out;
    for (size_t i = 0; i < chars && offset + i * 2 + 1 < object.size(); ++i) {
        const auto c = static_cast<wchar_t>(object[offset + i * 2] | (object[offset + i * 2 + 1] << 8));
        if (c == 0) break;
        out.push_back(c);
    }
    return out;
}

void Settings::setWideText(uint32_t offset, std::wstring_view text, size_t chars) {
    size_t i = 0;
    for (; i + 1 < chars && i < text.size() && offset + i * 2 + 1 < object.size(); ++i) {
        object[offset + i * 2] = static_cast<uint8_t>(text[i] & 0xFF);
        object[offset + i * 2 + 1] = static_cast<uint8_t>((text[i] >> 8) & 0xFF);
    }
    if (offset + i * 2 + 1 < object.size()) {
        object[offset + i * 2] = 0;
        object[offset + i * 2 + 1] = 0;
    }
}

std::string Settings::crewName() const {
    const char* p = reinterpret_cast<const char*>(object.data() + kCrewName);
    return std::string(p, strnlen(p, 15));
}

void Settings::setCrewName(std::wstring_view name) {
    size_t o = kCrewName;
    for (wchar_t w : name) {
        const auto c = static_cast<uint8_t>(w);
        if (c == 0 || o + 1 >= 0xA0) {
            break;
        }
        object[o++] = c;
    }
    object[o] = 0;
}

bool Settings::read(const std::vector<uint8_t>& data) {
    object.fill(0);
    size_t o = 0;
    for (const auto& [offset, size] : layout()) {
        if (o + size > data.size()) {
            return false;
        }
        std::memcpy(object.data() + offset, data.data() + o, size);
        o += size;
    }
    return o == data.size();
}

std::vector<uint8_t> Settings::write() const {
    std::vector<uint8_t> out;
    for (const auto& [offset, size] : layout()) {
        out.insert(out.end(), object.begin() + offset, object.begin() + offset + size);
    }
    return out;
}

}  // namespace oj::save
