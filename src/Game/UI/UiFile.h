#pragma once
// A window of the game's interface: ui\windows\<name>.ui ("Jet UI Window File"), text.
//
//   NextStringID n
//   [BeginUI]
//     Begin<Kind> <NAME> x y w h     Kind: Box Text Button Menu Meter Ticker Movie 3d Dial ...;
//       <Property> values...         x y w h: fractions of the screen
//     End<Kind>
//   [EndUI]
//
// Lines starting with ";;" are comments. Element properties are kept as words; the meaning of the common ones:
//   Colour a r g b, VertexColour TOPLEFT|TOPRIGHT|BOTLEFT|BOTRIGHT a r g b, Texture <name> (ui\textures\<name>.ddt),
//   UVs u0 u1 v0 v1, UVScroll du dv (per second), Additive, Transition FADEIN <duration> <delay>,
//   Caption <n> (line n of the window's text file), Font <name> a r g b, Align Left|Centre|Right, Movie <name>,
//   Looped.

#include <filesystem>
#include <string>
#include <vector>

namespace oj::ui {

struct Property {
    std::string name;
    std::vector<std::string> values;
};

// The kinds of element of the window files (Begin<Kind>; docs/spec/05-scripts.md): the names the code tells apart, as
// they are written (Begin3d's in any case), and any other.
enum class ElementKind { Box, Text, Button, Menu, Meter, Ticker, Movie, View3d, Dial, Slider, ColourChart, Line, Other };
ElementKind elementKind(const std::string& name);

struct Element {
    std::string kindName;  // the word after Begin: Box, Text, Movie, ...
    ElementKind kind = ElementKind::Other;
    std::string name;
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    std::vector<Property> properties;

    const Property* find(const std::string& property) const;
    bool has(const std::string& property) const { return find(property) != nullptr; }
};

struct Window {
    int nextStringId = 0;
    std::vector<Element> elements;  // in file order, later ones drawn over earlier ones

    bool load(const std::filesystem::path& file);
};

}  // namespace oj::ui
