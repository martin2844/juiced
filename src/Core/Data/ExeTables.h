#pragma once
// Name tables built into the stock executable that the game's data files and saves rely on (docs/spec/06-cars.md).
// They are part of the program, so OpenJuice has them built in (stock(), the retail build's): it needs no Juiced.exe.
// load() reads them from an executable of the game, for the tests to compare the built-in ones with.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace oj {

// A material parameter's type (the material classes' table): what a .pmd value reads as and what it takes.
enum class MaterialParamType : uint32_t {
    Int = 1,
    Float = 2,
    Fraction = 3,  // a float
    Bool = 4,      // FALSE or another word
    Colour = 5,    // r g b
    Float3 = 6,
    Text = 7,  // 256 characters
};

// A route's type (the routes' table +0x84); Any only as 0x0042B7B0 asks: a circuit or a point-to-point.
enum class RouteType : int32_t { Any = 0, Circuit = 1, Sprint = 2, Cruise = 3, PointToPoint = 4 };

// The material classes' ids (the classes' table of Juiced.exe, ExeTables::materialClasses: 53 of the .pmd files and
// the internal ones from 0x10000), each with its name there.
enum MaterialClassId : uint32_t {
    kCarBody = 0,                     // Car Body
    kVertexColoured = 1,              // Vertex Coloured
    kConstantColour = 2,              // Constant Colour
    kStandardOpaque = 3,              // Standard Opaque
    kStandardAlphaBlend = 4,          // Standard Alpha Blend
    kStandardAlphaTest = 5,           // Standard Alpha Test
    kSpecularOpaque = 6,              // Specular Opaque
    kTwoTextureModulate = 7,          // 2-Texture Modulate
    kTwoTextureModulateSpecular = 8,  // 2-Texture Modulate Specular
    kGlass = 9,                       // Glass
    kBuilding = 10,                   // Building
    kTarmac = 11,                     // Tarmac
    kGrass = 12,                      // Grass
    kSpriteUnlit = 13,                // Sprite (Unlit)
    kUnlitOpaque = 14,                // Unlit Opaque
    kUnlitABlend = 15,                // Unlit ABlend
    kSpriteLit = 16,                  // Sprite (Lit)
    kLightFlare = 17,                 // Light Flare
    kUnlitSignScroll = 18,            // Unlit Sign Scroll
    kUnlitSignAnim = 19,              // Unlit Sign Anim
    kVertColOpaque = 20,              // Vert-Col Opaque
    kVertColAlphaTest = 21,           // Vert-Col Alpha Test
    kVertColAlphaBlend = 22,          // Vert-Col Alpha Blend
    kVertColTwoTexture = 23,          // Vert-Col 2-Texture
    kShinyOpaque = 24,                // Shiny Opaque
    kVertexColouredUnlit = 25,        // Vertex Coloured Unlit
    kChrome = 26,                     // Chrome
    kColouredSpecOpaque = 27,         // Coloured Spec Opaque
    kColouredSpecABlend = 28,         // Coloured Spec ABlend
    kUnlitATest = 29,                 // Unlit ATest
    kTreeStandard = 30,               // Tree-Standard
    kTreeSprite = 31,                 // Tree-Sprite
    kStandardFadeOut = 32,            // Standard Fade-Out
    kAnimAlphaAdd = 33,               // Anim Alpha-Add
    kBlurredShadow = 34,              // Blurred Shadow
    kCrowdSprite = 35,                // Crowd Sprite
    kFadeToConstColour = 36,          // Fade To Const Colour
    kCloudSprite = 37,                // Cloud Sprite
    kAmbientLit = 38,                 // AmbientLit
    kShinyUnlitOpaque = 39,           // Shiny Unlit Opaque
    kShinyUnlitATest = 40,            // Shiny Unlit ATest
    kSky = 41,                        // Sky
    kBuildingOpaque = 42,             // Building Opaque
    kSpecularATest = 43,              // Specular ATest
    kSpecMapOpaque = 44,              // SpecMap Opaque
    kSpecMapATest = 45,               // SpecMap ATest
    kWater = 46,                      // Water
    kCrowdSpriteAnim = 47,            // Crowd Sprite Anim
    kSea = 48,                        // Sea
    kCarGlass = 49,                   // Car Glass
    kLineTexturedOpaque = 50,         // Line Textured Opaque
    kLineTexturedABlend = 51,         // Line Textured ABlend
    kLineVertColABlend = 52,          // Line Vert-Col ABlend
    kFont = 0x10000,                  // Font
    kLine = 0x10001,                  // Line
    kTexShape2D = 0x10002,            // TexShape2D
    kMotionBlur = 0x10003,            // MotionBlur
    kAntialias = 0x10004,             // Antialias
    kBlur2D = 0x10005,                // 2DBlur
    kAntialiasZBuf = 0x10006,         // AntialiasZBuf
    kHeatHaze = 0x10007,              // HeatHaze
};

struct ExeTables {
    struct Brand {
        std::string name;
        int id = 0;
    };

    // The internal names of the car models by car id (53, "NONE" first; 64-byte entries at 0x007167B8 of the
    // reference executable). A car of cardefs.txt gets the id of its name (0x0042A8F0).
    std::vector<std::string> carModels;
    // MANUFACTURER names (0x007162F8, 18) and regions (0x00716178: NONE JAPAN USA EUROPE AUSTRALIA).
    std::vector<std::string> manufacturers;
    std::vector<std::string> regions;
    // Part suppliers with their ids (68-byte entries at 0x00717970, 50).
    std::vector<Brand> brands;

    // A race route (0x94-byte entries at 0x00711A3C; index 0 is empty): its track (location) and route names.
    struct Route {
        std::string track;
        std::string name;
        int location = 0;
        RouteType type = RouteType::Any;
        int index = 0;
        uint32_t text = 0;  // the name's game text (its global; +0x8C of the entry from 0x00711A38)
    };
    std::vector<Route> routes;

    // Career tables (docs/spec/08-career.md), found through the code that reads them.
    std::vector<int> dealerThresholds;      // per car model (0x00718A78)
    // What an unlock step raises: a part's level (the profile's partsA), a body part's (partsB), the car's level.
    enum class UnlockKind : int32_t { Part = 0, BodyPart = 1, CarLevel = 2 };
    struct UnlockStep {
        int index = 0;
        int value = 0;
        UnlockKind kind = UnlockKind::Part;
    };
    std::vector<UnlockStep> unlockSteps;    // up to the record with value -1 (0x00718B50)
    std::vector<int> modelClasses;          // per car model, the jump table of 0x0048CE20
    std::vector<std::string> challengeTypes;  // LapTime ... (0x007187A8)
    std::vector<std::string> crewNames;     // the crews' display names (0x00718738)
    std::vector<std::string> logos;         // ui_chooselogo_NN (0x00718A28)
    std::vector<int> respectSteps;          // 4 (0x00718ECC)
    // Per car class (0x0071875C), and the word after the 8 (0x0071877C: 0, written by nothing) that class 8 reads.
    std::vector<float> prizeFactors;
    std::vector<int> crewClassLimits;       // per crew (0x00718780)

    // The material classes (docs/spec/11-effects.md): the records the start-up code registers (0x006C1810 ..), the
    // 53 of the .pmd files and the internal ones from 0x10000.
    struct MaterialParam {
        uint32_t index = 0;  // the parameter number the class's object takes
        MaterialParamType type = MaterialParamType::Int;
        std::string name;
    };
    struct MaterialClass {
        uint32_t id = 0;
        std::string name;
        uint32_t value = 0;
        uint32_t flags = 0;
        std::vector<MaterialParam> params;
        std::vector<std::pair<uint32_t, std::string>> textures;  // slot, name
    };
    std::vector<MaterialClass> materialClasses;
    const MaterialClass* materialClass(const std::string& name) const;
    // By the class number (the .d8m of a route names its classes so).
    const MaterialClass* materialClass(uint32_t id) const;

    // The places of the city map (40-byte records at 0x00767E50 that the start-up code 0x00462310 fills): the text
    // shown (+0x00, from a wide string of the executable), the name (the track's), its picture and the point on the
    // map (cm).
    struct CityLocation {
        std::wstring text;
        std::string name;
        std::string texture;
        float x = 0.0f, y = 0.0f, z = 0.0f;
    };
    std::vector<CityLocation> cityLocations;
    // The nine place names the arcade's map finds a location's text among (0x00767DC0, 0x10 apart; wide strings of
    // the executable, made at start by 0x006BED50): "Angel North Central" .. "Super Speedway".
    std::vector<std::wstring> locationNames;

    // The tables built into OpenJuice: the retail build's (StockTables.cpp, written by the tests' generator).
    static const ExeTables& stock();
    // The tables read from an executable of the game, found by their contents and the code that reads them; false
    // when they are not found (a protected executable's code cannot be read).
    bool load(const std::filesystem::path& exe, std::string* error);
    // The route index of a track and route name (0x00426490): both compared without regard to case; 0 when none.
    int routeIndex(const std::string& track, const std::string& route) const;
    // The id of a car model name (compared without regard to case), 11 for an unknown name as the stock game does.
    int carModelId(const std::string& name) const;
};

}  // namespace oj
