// Integration tests: ExeTables: the built-in tables, and how they stand against the game folder's executable.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Data/ExeTables.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>

namespace {

using oj::ExeTables;
using oj::test::check;

// ---- The generator: the C++ of a set of tables as StockTables.cpp holds them.

std::string quoted(const std::string& s) {
    std::string out = "\"";
    for (const char c : s) {
        const auto u = static_cast<unsigned char>(c);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (u < 0x20 || u >= 0x7F) {
            out += std::format("\\{:03o}", u);
        } else {
            out += c;
        }
    }
    return out + "\"";
}

std::string quoted(const std::wstring& s) {
    std::string out = "L\"";
    for (const wchar_t c : s) {
        if (c == L'"' || c == L'\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c < 0x20 || c >= 0x7F) {
            out += std::format("\\u{:04X}", static_cast<unsigned>(c));
        } else {
            out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

std::string floatOf(float f) {
    return std::format("std::bit_cast<float>(0x{:08X}u)", std::bit_cast<uint32_t>(f));
}

std::string routeType(oj::RouteType t) {
    switch (t) {
    case oj::RouteType::Any: return "RouteType::Any";
    case oj::RouteType::Circuit: return "RouteType::Circuit";
    case oj::RouteType::Sprint: return "RouteType::Sprint";
    case oj::RouteType::Cruise: return "RouteType::Cruise";
    case oj::RouteType::PointToPoint: return "RouteType::PointToPoint";
    }
    return std::format("static_cast<RouteType>({})", static_cast<int32_t>(t));
}

std::string unlockKind(ExeTables::UnlockKind k) {
    switch (k) {
    case ExeTables::UnlockKind::Part: return "UnlockKind::Part";
    case ExeTables::UnlockKind::BodyPart: return "UnlockKind::BodyPart";
    case ExeTables::UnlockKind::CarLevel: return "UnlockKind::CarLevel";
    }
    return std::format("static_cast<UnlockKind>({})", static_cast<int32_t>(k));
}

std::string paramType(oj::MaterialParamType t) {
    switch (t) {
    case oj::MaterialParamType::Int: return "MaterialParamType::Int";
    case oj::MaterialParamType::Float: return "MaterialParamType::Float";
    case oj::MaterialParamType::Fraction: return "MaterialParamType::Fraction";
    case oj::MaterialParamType::Bool: return "MaterialParamType::Bool";
    case oj::MaterialParamType::Colour: return "MaterialParamType::Colour";
    case oj::MaterialParamType::Float3: return "MaterialParamType::Float3";
    case oj::MaterialParamType::Text: return "MaterialParamType::Text";
    }
    return std::format("static_cast<MaterialParamType>({})", static_cast<uint32_t>(t));
}

// A list of items, wrapped at 120 columns with the given indent.
std::string list(const std::vector<std::string>& items, size_t indent) {
    if (items.empty()) return "{}";
    std::string out = "{";
    size_t column = indent + 1;
    const std::string pad(indent + 4, ' ');
    for (size_t i = 0; i < items.size(); ++i) {
        const std::string item = items[i] + (i + 1 < items.size() ? "," : "");
        if (column + item.size() + 1 > 118) {
            out += "\n" + pad;
            column = pad.size();
        } else if (i > 0) {
            out += " ";
            ++column;
        }
        out += item;
        column += item.size();
    }
    return out + "}";
}

template <class T, class F>
std::vector<std::string> each(const std::vector<T>& values, F f) {
    std::vector<std::string> out;
    for (const T& v : values) out.push_back(f(v));
    return out;
}

std::string cppOf(const ExeTables& t) {
    const auto num = [](auto v) { return std::to_string(v); };
    std::string s;
    s += "// The tables of the retail build built into OpenJuice (docs/spec/06-cars.md, 08-career.md, 11-effects.md):\n";
    s += "// written by the tests' generator (tests/Integration/GameData/ExeTablesTests.cpp) from the build's executable.\n";
    s += "\n#include \"Core/Data/ExeTables.h\"\n\n#include <bit>\n\nnamespace oj {\n\n";
    s += "const ExeTables& ExeTables::stock() {\n    static const ExeTables tables = [] {\n        ExeTables t;\n";
    const auto field = [&](const char* name, const std::string& value) {
        s += std::format("        t.{} = {};\n", name, value);
    };
    field("carModels", list(each(t.carModels, [](const std::string& v) { return quoted(v); }), 8));
    field("manufacturers", list(each(t.manufacturers, [](const std::string& v) { return quoted(v); }), 8));
    field("regions", list(each(t.regions, [](const std::string& v) { return quoted(v); }), 8));
    field("brands", list(each(t.brands, [](const ExeTables::Brand& b) { return std::format("{{{}, {}}}", quoted(b.name), b.id); }), 8));
    field("routes", list(each(t.routes, [](const ExeTables::Route& r) {
                             return std::format("{{{}, {}, {}, {}, {}, 0x{:08X}u}}", quoted(r.track), quoted(r.name),
                                                r.location, routeType(r.type), r.index, r.text);
                         }),
                         8));
    field("dealerThresholds", list(each(t.dealerThresholds, num), 8));
    field("unlockSteps", list(each(t.unlockSteps, [](const ExeTables::UnlockStep& u) {
                                  return std::format("{{{}, {}, {}}}", u.index, u.value, unlockKind(u.kind));
                              }),
                              8));
    field("modelClasses", list(each(t.modelClasses, num), 8));
    field("challengeTypes", list(each(t.challengeTypes, [](const std::string& v) { return quoted(v); }), 8));
    field("crewNames", list(each(t.crewNames, [](const std::string& v) { return quoted(v); }), 8));
    field("logos", list(each(t.logos, [](const std::string& v) { return quoted(v); }), 8));
    field("respectSteps", list(each(t.respectSteps, num), 8));
    field("prizeFactors", list(each(t.prizeFactors, [](float f) { return floatOf(f); }), 8));
    field("crewClassLimits", list(each(t.crewClassLimits, num), 8));
    std::vector<std::string> classes;
    for (const ExeTables::MaterialClass& c : t.materialClasses) {
        const std::string params = list(each(c.params, [](const ExeTables::MaterialParam& p) {
                                            return std::format("{{{}, {}, {}}}", p.index, paramType(p.type), quoted(p.name));
                                        }),
                                        12);
        const std::string textures = list(
            each(c.textures, [](const std::pair<uint32_t, std::string>& x) { return std::format("{{{}, {}}}", x.first, quoted(x.second)); }),
            12);
        classes.push_back(std::format("{{0x{:X}, {}, 0x{:X}, 0x{:X}, {}, {}}}", c.id, quoted(c.name), c.value, c.flags,
                                      params, textures));
    }
    s += "        t.materialClasses = {\n";
    for (const std::string& c : classes) s += "            " + c + ",\n";
    s += "        };\n";
    field("cityLocations", list(each(t.cityLocations, [](const ExeTables::CityLocation& l) {
                                    return std::format("{{{}, {}, {}, {}, {}, {}}}", quoted(l.text), quoted(l.name),
                                                       quoted(l.texture), floatOf(l.x), floatOf(l.y), floatOf(l.z));
                                }),
                                8));
    field("locationNames", list(each(t.locationNames, [](const std::wstring& v) { return quoted(v); }), 8));
    s += "        return t;\n    }();\n    return tables;\n}\n\n}  // namespace oj\n";
    return s;
}

// ---- The comparison: the same tables, the routes' texts (their globals) taken from route 0's.

// The game text of a route's name as the windows take it (Game/Frontend/WindowTools.cpp, routeNameLine).
size_t nameLine(uint32_t text, uint32_t base) {
    if (text >= base && (text - base) / 16 <= 16) return 585 + (text - base) / 16;
    return 1972;
}

bool sameFloat(float a, float b) {
    return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b);
}

std::string difference(const ExeTables& a, const ExeTables& b) {
    if (a.carModels != b.carModels) return "car models";
    if (a.manufacturers != b.manufacturers) return "manufacturers";
    if (a.regions != b.regions) return "regions";
    if (a.brands.size() != b.brands.size()) return "brands";
    for (size_t i = 0; i < a.brands.size(); ++i) {
        if (a.brands[i].name != b.brands[i].name || a.brands[i].id != b.brands[i].id) return "brands";
    }
    if (a.routes.size() != b.routes.size()) return "routes";
    for (size_t i = 0; i < a.routes.size(); ++i) {
        const ExeTables::Route& x = a.routes[i];
        const ExeTables::Route& y = b.routes[i];
        const bool textSame = (x.text == 0) == (y.text == 0) &&
                              (x.text == 0 || nameLine(x.text, a.routes[0].text) == nameLine(y.text, b.routes[0].text));
        if (x.track != y.track || x.name != y.name || x.location != y.location || x.type != y.type ||
            x.index != y.index || !textSame) {
            return std::format("route {}", i);
        }
    }
    if (a.dealerThresholds != b.dealerThresholds) return "dealer thresholds";
    if (a.unlockSteps.size() != b.unlockSteps.size()) return "unlock steps";
    for (size_t i = 0; i < a.unlockSteps.size(); ++i) {
        if (a.unlockSteps[i].index != b.unlockSteps[i].index || a.unlockSteps[i].value != b.unlockSteps[i].value ||
            a.unlockSteps[i].kind != b.unlockSteps[i].kind) {
            return "unlock steps";
        }
    }
    if (a.modelClasses != b.modelClasses) return "model classes";
    if (a.challengeTypes != b.challengeTypes) return "challenge types";
    if (a.crewNames != b.crewNames) return "crew names";
    if (a.logos != b.logos) return "logos";
    if (a.respectSteps != b.respectSteps) return "respect steps";
    if (a.prizeFactors.size() != b.prizeFactors.size()) return "prize factors";
    for (size_t i = 0; i < a.prizeFactors.size(); ++i) {
        if (!sameFloat(a.prizeFactors[i], b.prizeFactors[i])) return "prize factors";
    }
    if (a.crewClassLimits != b.crewClassLimits) return "crew class limits";
    if (a.materialClasses.size() != b.materialClasses.size()) return "material classes";
    for (size_t i = 0; i < a.materialClasses.size(); ++i) {
        const ExeTables::MaterialClass& x = a.materialClasses[i];
        const ExeTables::MaterialClass& y = b.materialClasses[i];
        bool same = x.id == y.id && x.name == y.name && x.value == y.value && x.flags == y.flags &&
                    x.params.size() == y.params.size() && x.textures == y.textures;
        for (size_t k = 0; same && k < x.params.size(); ++k) {
            same = x.params[k].index == y.params[k].index && x.params[k].type == y.params[k].type &&
                   x.params[k].name == y.params[k].name;
        }
        if (!same) return std::format("material class {}", x.name);
    }
    if (a.cityLocations.size() != b.cityLocations.size()) return "city places";
    for (size_t i = 0; i < a.cityLocations.size(); ++i) {
        const ExeTables::CityLocation& x = a.cityLocations[i];
        const ExeTables::CityLocation& y = b.cityLocations[i];
        if (x.text != y.text || x.name != y.name || x.texture != y.texture || !sameFloat(x.x, y.x) ||
            !sameFloat(x.y, y.y) || !sameFloat(x.z, y.z)) {
            return std::format("city place {}", x.name);
        }
    }
    if (a.locationNames != b.locationNames) return "location names";
    return {};
}

}  // namespace

// The built-in tables against the game folder's executable when its code can be read (an executable whose code its
// protection keeps cannot, and needs not be: OpenJuice takes no table from it). They differing, the generator writes
// the executable's as StockTables.cpp would hold them, to the temporary folder.
OJ_INTEGRATION_TEST(ExeTables, StockMatchesTheExecutable) {
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    const std::filesystem::path exe = game.resolve(L"Juiced.exe");
    ExeTables read;
    std::string why;
    if (!std::filesystem::is_regular_file(exe)) {
        std::printf("     no Juiced.exe in the game folder to compare the built-in tables with\n");
        return;
    }
    if (!read.load(exe, &why)) {
        std::printf("     the folder's Juiced.exe gives no tables to compare (%s)\n", why.c_str());
        return;
    }
    const std::string different = difference(ExeTables::stock(), read);
    if (!different.empty()) {
        const std::filesystem::path out = std::filesystem::temp_directory_path() / "OpenJuice.StockTables.cpp";
        std::ofstream(out, std::ios::binary) << cppOf(read);
        std::printf("     the executable's tables written to %s\n", out.string().c_str());
    }
    check(different.empty(), "the built-in tables are the executable's" + (different.empty() ? "" : ": " + different));
}

OJ_UNIT_TEST(ExeTables, NameTables) {
    const ExeTables& tables = ExeTables::stock();
    check(tables.carModels.size() == 53 && tables.carModels[0] == "NONE" && tables.carModelId("FOCUS_2") == 52 &&
              tables.manufacturers[1] == "HONDA" && tables.regions[4] == "AUSTRALIA" &&
              tables.brands[42].name == "WINGS" && tables.brands[42].id == 42,
          "the name tables: car models, makes, regions, brands");
}

OJ_UNIT_TEST(ExeTables, CareerTables) {
    const ExeTables& tables = ExeTables::stock();
    const std::vector<int> respect{100, 300, 600, 1000};
    check(tables.routes.size() == 109 && tables.routes[1].name == "Angel_North_R1" &&
              tables.routeIndex("Angel SuperSpeedway", "Speedway_r1") == 97 && tables.dealerThresholds[1] == 4 &&
              tables.dealerThresholds[4] == 0 && tables.unlockSteps.size() == 38 &&
              tables.unlockSteps[6].kind == ExeTables::UnlockKind::BodyPart && tables.modelClasses[0] == 2 &&
              tables.modelClasses[9] == 1 && tables.modelClasses[1] == 0 && tables.challengeTypes[0] == "LapTime" &&
              tables.crewNames[1] == "Urban Maulerz" && tables.logos[0] == "ui_chooselogo_01" &&
              tables.logos.size() == 19 && tables.logos[18] == "ui_chooselogo_20" && tables.respectSteps == respect &&
              tables.prizeFactors[0] == 3.0f && tables.prizeFactors[7] == 1.0f && tables.prizeFactors.size() == 9 &&
              tables.prizeFactors[8] == 0.0f,
          "the career tables: routes, dealer, unlock steps, model classes, names, respect, prizes");
    const ExeTables::MaterialClass* opaque = tables.materialClass("Standard Opaque");
    const ExeTables::MaterialClass* building = tables.materialClass("building opaque");
    check(tables.materialClasses.size() == 61 && opaque != nullptr && opaque->id == 3 && opaque->flags == 0x200 &&
              opaque->params.size() == 2 && opaque->params[1].name == "Z-Bias" &&
              opaque->params[1].type == oj::MaterialParamType::Fraction && opaque->textures.size() == 1 &&
              building != nullptr && building->id == 42 && building->textures.size() == 2 &&
              building->textures[1].second == "Night-Light",
          "the material classes: 53 of the .pmd files and 8 internal, with parameters and slots");
    check(tables.cityLocations.size() == 9 && tables.cityLocations[0].name == "ANGEL NORTH CENTRAL" &&
              tables.cityLocations[8].name == "ANGEL SUPERSPEEDWAY" &&
              tables.cityLocations[4].name == "ENGSTROM DISTRICT" &&
              tables.cityLocations[4].texture == "ui_sanricardo" && tables.cityLocations[4].y == 0.0f &&
              tables.locationNames.size() == 9 && tables.locationNames[0] == L"Angel North Central",
          "the city places: names, pictures and points of the map");
}
