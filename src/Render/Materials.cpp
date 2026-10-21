#include "Render/Materials.h"

#include "Render/Materials/MaterialsInternal.h"
#include "Render/Materials/StandardMaterials.h"
#include "Render/Materials/UnlitMaterials.h"
#include "Render/Materials/WorldMaterials.h"
#include "Render/Materials/SpriteMaterials.h"
#include "Render/Materials/CarMaterials.h"

#include <cfenv>
#include <memory>

namespace oj::render {

using namespace materials;

void Material::touched() {
    m_r.forgetMaterial(this);
}

void Material::cull(Program* program, bool none) {
    (void)program;  // the state manager of the program goes straight to the device
    m_r.device().setRenderState(RenderState::CullMode, none ? Cull::None : m_r.defaultCull);
}

std::unique_ptr<Material> createMaterial(const ExeTables::MaterialClass& info, Renderer3d& renderer,
                                         const ProgramLookup& programs, const ExeTables& tables) {
    // The creators of the line classes (0x00722FA0, 0x00722FA4, 0x00722FA8; no material of the game's files names
    // them) make objects of other classes, their ids and records those classes': Line Textured Opaque (50) an Unlit
    // Opaque (0x00530A90), Line Textured ABlend (51) an Unlit ABlend (0x005302B0), Line Vert-Col ABlend (52) a Vertex
    // Coloured Unlit (0x0052B770).
    if (info.id >= kLineTexturedOpaque && info.id <= kLineVertColABlend) {
        const ExeTables::MaterialClass* made =
            tables.materialClass(info.id == kLineTexturedOpaque   ? kUnlitOpaque
                                 : info.id == kLineTexturedABlend ? kUnlitABlend
                                                                  : kVertexColouredUnlit);
        if (made == nullptr) return nullptr;
        return createMaterial(*made, renderer, programs, tables);
    }
    switch (info.id) {
    case kCarBody: return std::make_unique<CarBody>(info, renderer, programs);
    case kStandardOpaque: return std::make_unique<StandardOpaque>(info, renderer, programs);
    case kGlass: return std::make_unique<Glass>(info, renderer, programs);
    case kColouredSpecOpaque: return std::make_unique<ColouredSpec>(info, renderer, programs, false);
    case kColouredSpecABlend: return std::make_unique<ColouredSpec>(info, renderer, programs, true);
    case kCarGlass: return std::make_unique<CarGlass>(info, renderer, programs);
    case kStandardAlphaTest: return std::make_unique<StandardAlphaTest>(info, renderer, programs);
    case kSpecularOpaque: return std::make_unique<Specular>(info, renderer, programs, false);
    case kConstantColour: return std::make_unique<ConstantColour>(info, renderer, programs);
    case kStandardAlphaBlend: return std::make_unique<StandardAlphaBlend>(info, renderer, programs);
    case kTarmac: return std::make_unique<Tarmac>(info, renderer, programs);
    case kUnlitOpaque: return std::make_unique<UnlitOpaque>(info, renderer, programs);
    case kUnlitABlend: return std::make_unique<UnlitAlphaBlend>(info, renderer, programs);
    case kUnlitSignScroll: return std::make_unique<UnlitSignScroll>(info, renderer, programs);
    case kShinyOpaque: return std::make_unique<Shiny>(info, renderer, programs, false);
    case kChrome: return std::make_unique<Chrome>(info, renderer, programs);
    case kUnlitSignAnim: return std::make_unique<UnlitSignAnim>(info, renderer, programs);
    case kUnlitATest: return std::make_unique<UnlitAlphaTest>(info, renderer, programs);
    case kTreeStandard: return std::make_unique<Tree>(info, renderer, programs, false);
    case kTreeSprite: return std::make_unique<Tree>(info, renderer, programs, true);
    case kStandardFadeOut: return std::make_unique<StandardFadeOut>(info, renderer, programs);
    case kAnimAlphaAdd: return std::make_unique<AnimAlphaAdd>(info, renderer, programs);
    case kCrowdSprite: return std::make_unique<CrowdSprite>(info, renderer, programs, false);
    case kFadeToConstColour: return std::make_unique<FadeToConstColour>(info, renderer, programs);
    case kGrass:
        return std::make_unique<PlainClass>(info, renderer, programs, "Material_Grass", true,
                                            PlainClass::Setup::DefaultCull);
    case kVertColOpaque:
        return std::make_unique<PlainClass>(info, renderer, programs, "Material_VertColOpaque", true,
                                            PlainClass::Setup::DefaultCullFog);
    case kCloudSprite:
        return std::make_unique<PlainClass>(info, renderer, programs, "Material_CloudSprite", true,
                                            PlainClass::Setup::NoCull);
    case kVertexColoured:
        return std::make_unique<PlainClass>(info, renderer, programs, "Material_VertColoured", false,
                                            PlainClass::Setup::DefaultCullFog);
    case kVertexColouredUnlit:
        return std::make_unique<PlainClass>(info, renderer, programs, "Material_VertColouredUnlit", false,
                                            PlainClass::Setup::DefaultCullFog);
    case kAmbientLit: return std::make_unique<AmbientLit>(info, renderer, programs);
    case kShinyUnlitOpaque: return std::make_unique<Shiny>(info, renderer, programs, true);
    case kShinyUnlitATest: return std::make_unique<Shiny>(info, renderer, programs, true, "Material_ShinyUnlitATest");
    case kBlurredShadow: return std::make_unique<BlurredShadow>(info, renderer, programs);
    case kTwoTextureModulate: return std::make_unique<TwoTexture>(info, renderer, programs, TwoTexture::Kind::Modulate);
    case kTwoTextureModulateSpecular:
        return std::make_unique<TwoTexture>(info, renderer, programs, TwoTexture::Kind::Specular);
    case kBuilding: return std::make_unique<Building>(info, renderer, programs);
    case kSpecularATest: return std::make_unique<Specular>(info, renderer, programs, false, true);
    case kSpecMapATest: return std::make_unique<Specular>(info, renderer, programs, true, true);
    case kVertColTwoTexture:
        return std::make_unique<TwoTexture>(info, renderer, programs, TwoTexture::Kind::VertexColour);
    case kWater: return std::make_unique<Water>(info, renderer, programs);
    case kSea: return std::make_unique<Sea>(info, renderer, programs);
    case kCrowdSpriteAnim: return std::make_unique<CrowdSprite>(info, renderer, programs, true);
    case kSpriteUnlit: return std::make_unique<Sprite>(info, renderer, programs, false);
    case kSpriteLit: return std::make_unique<Sprite>(info, renderer, programs, true);
    case kSky: return std::make_unique<Sky>(info, renderer, programs);
    case kBuildingOpaque: return std::make_unique<BuildingOpaque>(info, renderer, programs);
    case kSpecMapOpaque: return std::make_unique<Specular>(info, renderer, programs, true);
    case kLightFlare: return std::make_unique<LightFlare>(info, renderer, programs);
    case kVertColAlphaTest: return std::make_unique<VertexColour>(info, renderer, programs, false);
    case kVertColAlphaBlend: return std::make_unique<VertexColour>(info, renderer, programs, true);
    default: return nullptr;
    }
}

}  // namespace oj::render
