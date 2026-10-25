#include "Core/Data/ExeTables.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>

namespace oj {

namespace {

std::string at(const std::vector<char>& data, size_t offset, size_t size) {
    if (offset + size > data.size()) {
        return {};
    }
    const char* p = data.data() + offset;
    return std::string(p, strnlen(p, size));
}

// The offset of a table of fixed-size names that starts with first and has second as its next entry.
size_t find(const std::vector<char>& data, const char* first, const char* second, size_t stride) {
    std::string needle(stride, '\0');
    std::memcpy(needle.data(), first, std::strlen(first));
    needle += second;
    needle.push_back('\0');
    const auto it = std::search(data.begin(), data.end(), needle.begin(), needle.end());
    return it == data.end() ? std::string::npos : static_cast<size_t>(it - data.begin());
}

// The sections of the executable, to turn addresses into file offsets.
class Image {
public:
    explicit Image(const std::vector<char>& data) : m_data(data) {
        const uint32_t pe = u32(0x3C);
        if (pe + 0x18 > data.size() || std::memcmp(data.data() + pe, "PE\0\0", 4) != 0) {
            return;
        }
        const uint16_t sections = u16(pe + 6);
        const uint16_t optional = u16(pe + 0x14);
        m_base = u32(pe + 0x18 + 0x1C);
        size_t at = pe + 0x18 + optional;
        for (uint16_t i = 0; i < sections && at + 40 <= data.size(); ++i, at += 40) {
            m_sections.push_back({u32(at + 12), u32(at + 8), u32(at + 20), u32(at + 16)});
        }
    }

    uint32_t u32(size_t offset) const {
        uint32_t v = 0;
        if (offset + 4 <= m_data.size()) std::memcpy(&v, m_data.data() + offset, 4);
        return v;
    }
    uint16_t u16(size_t offset) const {
        uint16_t v = 0;
        if (offset + 2 <= m_data.size()) std::memcpy(&v, m_data.data() + offset, 2);
        return v;
    }
    // The address of a file offset, 0 outside the sections' file data.
    uint32_t addressOf(size_t offset) const {
        for (const Section& s : m_sections) {
            if (offset >= s.raw && offset - s.raw < s.rawSize) {
                return m_base + s.address + static_cast<uint32_t>(offset - s.raw);
            }
        }
        return 0;
    }
    // The file offset of an address, npos outside the sections' file data.
    size_t offsetOf(uint32_t address) const {
        const uint32_t rva = address - m_base;
        for (const Section& s : m_sections) {
            if (rva >= s.address && rva - s.address < s.rawSize) {
                return s.raw + (rva - s.address);
            }
        }
        return std::string::npos;
    }
    // The address operand of the first code with the bytes before and after it, 0 when not found.
    uint32_t operand(std::initializer_list<uint8_t> before, std::initializer_list<uint8_t> after) const {
        std::vector<char> b;
        for (uint8_t v : before) b.push_back(static_cast<char>(v));
        std::vector<char> a;
        for (uint8_t v : after) a.push_back(static_cast<char>(v));
        auto it = m_data.begin();
        while ((it = std::search(it, m_data.end(), b.begin(), b.end())) != m_data.end()) {
            const auto at = static_cast<size_t>(it - m_data.begin()) + b.size();
            if (at + 4 + a.size() <= m_data.size() && std::equal(a.begin(), a.end(), m_data.begin() + at + 4)) {
                return u32(at);
            }
            ++it;
        }
        return 0;
    }
    std::string string(uint32_t address) const {
        const size_t offset = offsetOf(address);
        if (offset == std::string::npos) return {};
        return at(m_data, offset, std::min<size_t>(256, m_data.size() - offset));
    }
    // A UTF-16 string ending with 0 (256 characters at most).
    std::wstring wideString(uint32_t address) const {
        const size_t offset = offsetOf(address);
        std::wstring text;
        if (offset == std::string::npos) return text;
        for (size_t i = offset; i + 2 <= m_data.size() && text.size() < 256; i += 2) {
            const uint16_t c = u16(i);
            if (c == 0) break;
            text += static_cast<wchar_t>(c);
        }
        return text;
    }

private:
    struct Section {
        uint32_t address, size, raw, rawSize;
    };
    const std::vector<char>& m_data;
    uint32_t m_base = 0;
    std::vector<Section> m_sections;
};

template <class T>
std::vector<T> numbers(const Image& image, uint32_t address, size_t count) {
    std::vector<T> values;
    const size_t offset = image.offsetOf(address);
    if (address == 0 || offset == std::string::npos) return values;
    for (size_t i = 0; i < count; ++i) {
        const uint32_t bits = image.u32(offset + i * 4);
        T v;
        std::memcpy(&v, &bits, 4);
        values.push_back(v);
    }
    return values;
}

std::vector<std::string> strings(const Image& image, uint32_t address, size_t count) {
    std::vector<std::string> values;
    const size_t offset = image.offsetOf(address);
    if (address == 0 || offset == std::string::npos) return values;
    for (size_t i = 0; i < count; ++i) {
        const uint32_t p = image.u32(offset + i * 4);
        const std::string s = p != 0 ? image.string(p) : std::string();
        if (s.empty()) break;
        values.push_back(s);
    }
    return values;
}

}  // namespace

bool ExeTables::load(const std::filesystem::path& exe, std::string* error) {
    std::ifstream in(exe, std::ios::binary);
    if (!in) {
        if (error) *error = "cannot read " + exe.string();
        return false;
    }
    const std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    const size_t models = find(data, "NONE", "celica", 64);
    const size_t makes = find(data, "NONE", "HONDA", 64);
    const size_t regionTable = find(data, "NONE", "JAPAN", 64);
    const size_t brandTable = find(data, "BILSTEIN", "EIBACH", 0x44);  // id 0 is zero bytes
    if (models == std::string::npos || makes == std::string::npos || regionTable == std::string::npos ||
        brandTable == std::string::npos) {
        if (error) *error = "the name tables are not in " + exe.string();
        return false;
    }
    carModels.clear();
    for (size_t i = 0; i < 53; ++i) {
        carModels.push_back(at(data, models + i * 64, 64));
    }
    manufacturers.clear();
    for (size_t i = 0; i < 18; ++i) {
        manufacturers.push_back(at(data, makes + i * 64, 64));
    }
    regions.clear();
    for (size_t i = 0; i < 5; ++i) {
        regions.push_back(at(data, regionTable + i * 64, 64));
    }
    brands.clear();
    for (size_t i = 0; i < 50; ++i) {
        Brand b;
        b.name = at(data, brandTable + i * 0x44, 0x40);
        int32_t id = 0;
        std::memcpy(&id, data.data() + brandTable + i * 0x44 + 0x40, 4);
        b.id = id;
        brands.push_back(std::move(b));
    }

    const size_t route1 = find(data, "Angel North Central", "Angel_North_R1", 64);
    if (route1 == std::string::npos || route1 < 0x94) {
        if (error) *error = "the routes table is not in " + exe.string();
        return false;
    }
    const Image image(data);
    routes.clear();
    for (size_t entry = route1 - 0x94; entry + 0x94 <= data.size(); entry += 0x94) {
        Route r;
        r.track = at(data, entry, 64);
        r.name = at(data, entry + 64, 64);
        r.location = static_cast<int>(image.u32(entry + 0x80));
        r.type = static_cast<RouteType>(image.u32(entry + 0x84));
        r.index = static_cast<int>(image.u32(entry + 0x8C));
        r.text = image.u32(entry + 0x88);
        // The table ends where the names stop being text.
        const bool text = std::all_of(r.track.begin(), r.track.end(), [](char c) { return c >= 0x20 && c < 0x7F; });
        if (!routes.empty() && (r.track.empty() || !text)) break;
        routes.push_back(std::move(r));
    }

    dealerThresholds = numbers<int>(image, image.operand({0x8B, 0x04, 0x9D}, {0x0F, 0xBF, 0x49, 0x4E}), 53);
    unlockSteps.clear();
    if (const uint32_t steps = image.operand({0x8D, 0x1C, 0x9D}, {0x83, 0x7B, 0x04, 0xFF}); steps != 0) {
        for (uint32_t a = steps; unlockSteps.size() < 256; a += 12) {
            const std::vector<int> r = numbers<int>(image, a, 3);
            if (r.size() != 3 || r[1] == -1) break;
            unlockSteps.push_back({r[0], r[1], static_cast<UnlockKind>(r[2])});
        }
    }
    modelClasses.clear();
    if (const uint32_t table = image.operand({0x83, 0xF8, 0x34, 0x77, 0x13, 0xFF, 0x24, 0x85}, {}); table != 0) {
        // The cases are "mov eax, n; ret" or "xor eax, eax; ret".
        for (uint32_t target : numbers<uint32_t>(image, table, 53)) {
            const size_t code = image.offsetOf(target);
            const bool load = code != std::string::npos && static_cast<uint8_t>(data[code]) == 0xB8;
            modelClasses.push_back(load ? static_cast<int>(image.u32(code + 1)) : 0);
        }
    }
    challengeTypes = strings(image, image.operand({0x8B, 0x04, 0x9D}, {0x50, 0x55, 0xE8}), 4);
    crewNames = strings(image, image.operand({0x8B, 0x14, 0x8D}, {0x52, 0x8D, 0x87, 0xDE, 0x00, 0x00, 0x00}), 9);
    logos = strings(image, image.operand({0x8B, 0x0C, 0x85}, {0x5E, 0x8D, 0x53, 0x58}), 64);
    // Stock problem: the table goes from ui_chooselogo_18 to _20, so logo 19 (in the game's files and in
    // cmschooselogo.ui) is never offered. The fixed build puts it in its place.
    if (const auto at = std::find(logos.begin(), logos.end(), std::string("ui_chooselogo_20"));
        at != logos.end() && std::find(logos.begin(), logos.end(), std::string("ui_chooselogo_19")) == logos.end()) {
        logos.insert(at, "ui_chooselogo_19");
    }
    respectSteps = numbers<int>(image, image.operand({0x3B, 0x0C, 0xBD}, {0x7C, 0x09}), 4);
    prizeFactors = numbers<float>(image, image.operand({0xD8, 0x0C, 0x95}, {0x80, 0xCC, 0x0C}), 9);
    crewClassLimits = numbers<int>(image, image.operand({0x3B, 0x34, 0x85}, {0x7E, 0x0B}), 9);
    // Each class registers with "mov edx, [eax + 8]; mov [edx + ecx * 4], record".
    materialClasses.clear();
    {
        const char pattern[] = {static_cast<char>(0x8B), 0x50, 0x08,
                                static_cast<char>(0xC7), 0x04, static_cast<char>(0x8A)};
        for (auto it = data.begin();
             (it = std::search(it, data.end(), std::begin(pattern), std::end(pattern))) != data.end(); ++it) {
            const uint32_t record = image.u32(static_cast<size_t>(it - data.begin()) + sizeof(pattern));
            const size_t r = image.offsetOf(record);
            if (r == std::string::npos) continue;
            MaterialClass c;
            c.id = image.u32(r);
            c.name = image.string(image.u32(r + 4));
            const uint32_t paramCount = image.u32(r + 12);
            const uint32_t params = image.u32(r + 16);
            const uint32_t textureCount = image.u32(r + 20);
            const uint32_t textures = image.u32(r + 24);
            c.value = image.u32(r + 28);
            c.flags = image.u32(r + 32);
            if (c.name.empty() || paramCount > 64 || textureCount > 16) continue;
            for (uint32_t i = 0; i < paramCount; ++i) {
                const size_t p = image.offsetOf(params + i * 16);
                if (p == std::string::npos) break;
                c.params.push_back(
                    {image.u32(p), static_cast<MaterialParamType>(image.u32(p + 4)), image.string(image.u32(p + 8))});
            }
            for (uint32_t i = 0; i < textureCount; ++i) {
                const size_t t = image.offsetOf(textures + i * 12);
                if (t == std::string::npos) break;
                c.textures.emplace_back(image.u32(t), image.string(image.u32(t + 4)));
            }
            materialClasses.push_back(std::move(c));
        }
    }
    // The city places: the start-up code stores each record's fields with "mov dword ptr [address], value"; the
    // first record's name is "ANGEL NORTH CENTRAL".
    cityLocations.clear();
    {
        const std::string first = std::string("ANGEL NORTH CENTRAL") + '\0';
        const auto found = std::search(data.begin(), data.end(), first.begin(), first.end());
        const uint32_t textAddress =
            found != data.end() ? image.addressOf(static_cast<size_t>(found - data.begin())) : 0;
        size_t code = std::string::npos;
        uint32_t base = 0;
        for (size_t i = 0; textAddress != 0 && i + 10 <= data.size(); ++i) {
            if (static_cast<uint8_t>(data[i]) == 0xC7 && static_cast<uint8_t>(data[i + 1]) == 0x05 &&
                image.u32(i + 6) == textAddress) {
                code = i;
                base = image.u32(i + 2) - 0x10;
                break;
            }
        }
        if (code != std::string::npos) {
            std::map<uint32_t, uint32_t> values;
            // Each record's text is made first: "mov eax, <wide string>; lea ecx, [esp + 4]; call", in order (the
            // first 0x3F bytes before the first store in 1.00, 0x87 in 1.01).
            std::vector<uint32_t> texts;
            const size_t from = code > 0x100 ? code - 0x100 : 0;
            for (size_t i = from; i + 10 <= data.size() && i < code + 0x800; ++i) {
                if (static_cast<uint8_t>(data[i]) == 0xB8 && static_cast<uint8_t>(data[i + 5]) == 0x8D &&
                    static_cast<uint8_t>(data[i + 6]) == 0x4C && static_cast<uint8_t>(data[i + 7]) == 0x24 &&
                    static_cast<uint8_t>(data[i + 8]) == 0x04) {
                    texts.push_back(image.u32(i + 1));
                }
                if (static_cast<uint8_t>(data[i]) != 0xC7 || static_cast<uint8_t>(data[i + 1]) != 0x05) continue;
                const uint32_t address = image.u32(i + 2);
                if (address >= base && address < base + 16 * 40) values[address] = image.u32(i + 6);
            }
            auto number = [&values](uint32_t address) {
                const auto v = values.find(address);
                float f = 0.0f;
                if (v != values.end()) std::memcpy(&f, &v->second, 4);
                return f;
            };
            for (uint32_t record = base;; record += 40) {
                const auto name = values.find(record + 0x10);
                if (name == values.end()) break;
                CityLocation l;
                const size_t index = cityLocations.size();
                if (index < texts.size()) l.text = image.wideString(texts[index]);
                l.name = image.string(name->second);
                if (const auto t = values.find(record + 0x14); t != values.end()) l.texture = image.string(t->second);
                l.x = number(record + 0x18);
                l.y = number(record + 0x1C);
                l.z = number(record + 0x20);
                cityLocations.push_back(std::move(l));
            }
        }
        // The ninth record (0x00462671 .. 0x004626C5 of the unprotected code): Juiced.exe stores it from code its
        // protection runs in its own machine, which the search above cannot read; its values as that code writes
        // them. A tenth record of no name at (0, 0, 0) ends the table (0x004626F4 .. 0x0046271F).
        if (cityLocations.size() == 8 && cityLocations.back().name == "ANGEL WESTSIDE") {
            CityLocation l;
            l.text = L"ANGEL SUPERSPEEDWAY";
            l.name = "ANGEL SUPERSPEEDWAY";
            l.texture = "ui_angelwestside";
            l.x = std::bit_cast<float>(0x48B74D20u);  // 375401
            l.y = 0.0f;
            l.z = std::bit_cast<float>(0xC843B540u);  // -200405
            cityLocations.push_back(std::move(l));
        }
    }
    // The arcade's nine place names: UTF-16 strings one after another, each 4-byte aligned.
    locationNames.clear();
    {
        const std::wstring first = L"Angel North Central";
        std::string bytes;
        for (wchar_t c : first) {
            bytes += static_cast<char>(c & 0xFF);
            bytes += static_cast<char>((c >> 8) & 0xFF);
        }
        bytes += std::string(2, '\0');
        const auto found = std::search(data.begin(), data.end(), bytes.begin(), bytes.end());
        size_t at = found != data.end() ? static_cast<size_t>(found - data.begin()) : std::string::npos;
        for (int k = 0; at != std::string::npos && k < 9; ++k) {
            std::wstring name;
            size_t i = at;
            for (; i + 2 <= data.size(); i += 2) {
                const uint16_t c = image.u16(i);
                if (c == 0) break;
                name += static_cast<wchar_t>(c);
            }
            locationNames.push_back(name);
            at = (i + 2 + 3) & ~size_t{3};
        }
    }
    if (dealerThresholds.size() != 53 || unlockSteps.empty() || modelClasses.size() != 53 ||
        challengeTypes.size() != 4 || crewNames.size() != 9 || logos.empty() || respectSteps.size() != 4 ||
        prizeFactors.size() != 9 || crewClassLimits.size() != 9) {
        if (error) *error = "the career tables are not in " + exe.string();
        return false;
    }
    return true;
}

int ExeTables::routeIndex(const std::string& track, const std::string& route) const {
    for (size_t i = 0; i < routes.size(); ++i) {
        if (_stricmp(routes[i].name.c_str(), route.c_str()) == 0 &&
            _stricmp(routes[i].track.c_str(), track.c_str()) == 0) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

const ExeTables::MaterialClass* ExeTables::materialClass(const std::string& name) const {
    // The .pmd reader takes the last class of the name (0x00523760).
    const MaterialClass* found = nullptr;
    for (const MaterialClass& c : materialClasses) {
        if (_stricmp(c.name.c_str(), name.c_str()) == 0) found = &c;
    }
    return found;
}

const ExeTables::MaterialClass* ExeTables::materialClass(uint32_t id) const {
    for (const MaterialClass& c : materialClasses) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

int ExeTables::carModelId(const std::string& name) const {
    for (size_t i = 0; i < carModels.size(); ++i) {
        if (_stricmp(carModels[i].c_str(), name.c_str()) == 0) {
            return static_cast<int>(i);
        }
    }
    return 11;
}

}  // namespace oj
