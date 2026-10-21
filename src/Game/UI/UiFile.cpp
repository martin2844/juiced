#include "Game/UI/UiFile.h"

#include "Core/Base/GameFiles.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <utility>

namespace oj::ui {

ElementKind elementKind(const std::string& name) {
    static constexpr std::pair<const char*, ElementKind> kKinds[] = {
        {"Box", ElementKind::Box},       {"Text", ElementKind::Text},         {"Button", ElementKind::Button},
        {"Menu", ElementKind::Menu},     {"Meter", ElementKind::Meter},       {"Ticker", ElementKind::Ticker},
        {"Movie", ElementKind::Movie},   {"Dial", ElementKind::Dial},         {"Slider", ElementKind::Slider},
        {"ColourChart", ElementKind::ColourChart}, {"Line", ElementKind::Line},
    };
    for (const auto& [word, kind] : kKinds) {
        if (name == word) return kind;
    }
    if (_stricmp(name.c_str(), "3d") == 0) return ElementKind::View3d;
    return ElementKind::Other;
}

const Property* Element::find(const std::string& property) const {
    for (const Property& p : properties) {
        if (p.name == property) {
            return &p;
        }
    }
    return nullptr;
}

namespace {

std::vector<std::string> words(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream in(line);
    std::string w;
    while (in >> w) {
        out.push_back(w);
    }
    return out;
}

}  // namespace

bool Window::load(const std::filesystem::path& file) {
    elements.clear();
    // A path of the game's (ui.dat as the game mounts it).
    const auto data = GameFiles(std::filesystem::current_path()).read(file);
    if (!data) {
        return false;
    }
    std::istringstream in(std::string(reinterpret_cast<const char*>(data->data()), data->size()));
    std::string line;
    Element* current = nullptr;
    bool inside = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto w = words(line);
        if (w.empty() || w[0].rfind(";;", 0) == 0) {
            continue;
        }
        if (w[0] == "NextStringID" && w.size() > 1) {
            nextStringId = std::atoi(w[1].c_str());
        } else if (w[0] == "[BeginUI]") {
            inside = true;
        } else if (w[0] == "[EndUI]") {
            inside = false;
        } else if (!inside) {
            continue;
        } else if (w[0].rfind("Begin", 0) == 0 && w[0].size() > 5) {
            Element e;
            e.kindName = w[0].substr(5);
            e.kind = elementKind(e.kindName);
            if (w.size() > 1) e.name = w[1];
            if (w.size() > 5) {
                e.x = std::strtof(w[2].c_str(), nullptr);
                e.y = std::strtof(w[3].c_str(), nullptr);
                e.w = std::strtof(w[4].c_str(), nullptr);
                e.h = std::strtof(w[5].c_str(), nullptr);
            }
            elements.push_back(std::move(e));
            current = &elements.back();
        } else if (w[0].rfind("End", 0) == 0 && w[0].size() > 3) {
            current = nullptr;
        } else if (current != nullptr) {
            current->properties.push_back({w[0], std::vector<std::string>(w.begin() + 1, w.end())});
        }
    }
    return true;
}

}  // namespace oj::ui
