#include "Game/Frontend/Career/Gamble.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace oj::frontend {

ui::Window gambleWindow() {
    ui::Window w;
    const auto element = [&](const char *kind, const std::string &name, float x, float y, float width, float height,
                             std::vector<ui::Property> props) {
        w.elements.push_back({kind, ui::elementKind(kind), name, x, y, width, height, std::move(props)});
    };
    const auto text = [&](const std::string &name, float x, float y, float width, const char *font = "goodtime11") {
        element("Text", name, x, y, width, 0.045f,
                {{"Colour", {"0", "0", "0", "0"}}, {"Font", {font, "255", "255", "255", "255"}}, {"Align", {"Left"}}});
    };
    element("Box", "BACKDROP", 0, 0, 1, 1, {{"Colour", {"255", "17", "14", "27"}}});
    element("3d", "POKER_TABLE", 0, 0.07f, 1, 0.82f, {});
    element("Box", "HEADER_BLUE", 0, 0, .5f, 0.11f,
            {{"Texture", {"ui_test_panblue"}}, {"Colour", {"255", "255", "255", "255"}}});
    element("Box", "HEADER_PINK", .5f, 0, .5f, 0.11f,
            {{"Texture", {"ui_test_panpink"}}, {"Colour", {"255", "255", "255", "255"}}});
    element("Box", "HEADER_TRIBAL", 0, 0, .65f, .16f,
            {{"Texture", {"ui_tribal_career_01"}}, {"Colour", {"170", "255", "255", "255"}}});
    text("TITLE", 0.04f, 0.035f, 0.3f, "concielian18");
    text("POT", 0.40f, 0.04f, 0.56f);
    for (int i = 1; i <= 4; ++i) {
        const float positions[] = {.04f, .23f, .63f, .82f};
        const float x = positions[i - 1];
        const auto id = std::to_string(i);
        element("Box", "FRAME" + id, x - 0.003f, 0.147f, 0.146f, 0.18f, {{"Colour", {"255", "111", "70", "143"}}});
        element("Box", "FACE" + id, x, 0.15f, 0.14f, 0.172f, {{"Colour", {"255", "255", "255", "255"}}});
        element("Movie", "HEAD" + id, x, 0.15f, 0.14f, 0.172f,
                {{"Colour", {"0", "0", "0", "0"}}, {"Movie", {"(null)"}}});
        text("NAME" + id, x, 0.112f, 0.21f);
        text("STACK" + id, x, 0.33f, 0.18f, "goodtime10");
    }
    element("Box", "DEALER_FRAME", .397f, .267f, .066f, .116f, {{"Colour", {"255", "111", "70", "143"}}});
    element("Box", "DEALER_FACE", .40f, .27f, .06f, .11f, {{"Colour", {"255", "255", "255", "255"}}});
    text("DEALER_LABEL", .40f, .23f, .12f, "goodtime10");
    element("Box", "COMMAND_PANEL", 0.68f, 0.47f, 0.30f, 0.385f, {{"Colour", {"220", "26", "20", "38"}}});
    element("Menu", "Menu", 0.70f, 0.49f, 0.27f, 0.355f,
            {{"Colour", {"0", "0", "0", "0"}},
             {"Font", {"goodtime11", "255", "230", "230", "230"}},
             {"HighFont", {"goodtime11", "255", "0", "180", "255"}},
             {"Align", {"Left"}},
             {"ItemHeight", {"0.055"}},
             {"MenuType", {"VERTICAL"}}});
    element("Box", "TITLE_CAR_BACK", .035f, .63f, .26f, .21f, {{"Colour", {"245", "17", "14", "27"}}});
    element("Box", "TITLE_CAR_EDGE", .035f, .63f, .004f, .21f, {{"Colour", {"255", "187", "64", "143"}}});
    element("3d", "TITLE_CAR_VIEW", .045f, .64f, .24f, .15f,
            {{"Ambient", {"96", "96", "96"}},
             {"Directional", {"220", "220", "220", "-0.4", "-1", "0.4"}},
             {"Atmosphere", {"dealer"}}});
    text("TITLE_CAR_LOADING", .06f, .69f, .22f, "goodtime10");
    text("TITLE_CAR_NAME", .05f, .795f, .235f, "goodtime10");
    text("INFO", 0.04f, 0.855f, 0.93f);
    text("STATUS", 0.04f, 0.905f, 0.93f);
    text("HINT", 0.04f, 0.955f, 0.93f);
    // A persistent result banner above the felt; next-hand controls remain accessible.
    element("Box", "RESULT_BACK", .035f, .40f, .635f, .285f, {{"Colour", {"245", "17", "14", "27"}}});
    element("Box", "RESULT_STRIPE", .035f, .40f, .008f, .285f, {{"Colour", {"255", "0", "180", "255"}}});
    element("Box", "RESULT_TRIBAL", .043f, .40f, .62f, .09f,
            {{"Texture", {"ui_test_panblue"}}, {"Colour", {"180", "255", "255", "255"}}});
    text("RESULT_TITLE", .06f, .435f, .58f, "concielian18");
    text("RESULT_VALUE", .06f, .51f, .58f, "concielian18");
    text("RESULT_DETAIL", .06f, .60f, .58f, "goodtime10");
    return w;
}

namespace {
struct Point {
    float x, y, z;
};
struct Face {
    std::array<Point, 3> points;
    uint32_t colour;
    float depth;
};

// Small native mesh scene: perspective projection and depth-tested triangles through the
// shared renderer. No platform-specific GL calls or external casino assets.
class PokerView final : public ui::View3d {
  public:
    PokerView(render::IRenderDevice &device, const Gamble &game, bool gallery = false)
        : m_device(device), m_game(game), m_gallery(gallery) {}
    void draw(float x0, float y0, float x1, float y1, float seconds, float dt) override;

  private:
    void tri(Point a, Point b, Point c, uint32_t colour) {
        m_faces.push_back({{a, b, c}, colour, (depth(a) + depth(b) + depth(c)) / 3});
    }
    float depth(Point p) const {
        return p.z * 0.8f - p.y * 0.6f + 9.0f;
    }
    void quad(Point a, Point b, Point c, Point d, uint32_t colour) {
        tri(a, b, c, colour);
        tri(a, c, d, colour);
    }
    void disk(float x, float y, float z, float rx, float rz, uint32_t colour);
    void cylinder(float x, float y, float z, float rx, float rz, float height, uint32_t top, uint32_t side);
    void box(float x, float y, float z, float w, float h, float d, uint32_t colour);
    void card(float x, float z, poker::Card c, bool face, float scale = 1);
    void glyph(float x, float y, float z, char c, uint32_t colour, float scale);
    render::IRenderDevice &m_device;
    const Gamble &m_game;
    bool m_gallery = false;
    std::vector<Face> m_faces;
    const poker::CareerHand *m_lastHand = nullptr;
    size_t m_boardCount = 0, m_previousBoardCount = 0;
    float m_time = 0, m_dealTime = 0, m_boardTime = 0;
};

void PokerView::disk(float x, float y, float z, float rx, float rz, uint32_t colour) {
    constexpr int slices = 64;
    for (int i = 0; i < slices; ++i) {
        const float a = i * 2.0f * std::numbers::pi_v<float> / slices;
        const float b = (i + 1) * 2.0f * std::numbers::pi_v<float> / slices;
        tri({x, y, z}, {x + rx * std::cos(a), y, z + rz * std::sin(a)}, {x + rx * std::cos(b), y, z + rz * std::sin(b)},
            colour);
    }
}
void PokerView::cylinder(float x, float y, float z, float rx, float rz, float height, uint32_t top, uint32_t side) {
    constexpr int slices = 64;
    for (int i = 0; i < slices; ++i) {
        const float a = i * 2.0f * std::numbers::pi_v<float> / slices;
        const float b = (i + 1) * 2.0f * std::numbers::pi_v<float> / slices;
        const float ax = x + rx * std::cos(a), az = z + rz * std::sin(a);
        const float bx = x + rx * std::cos(b), bz = z + rz * std::sin(b);
        quad({ax, y - height, az}, {ax, y, az}, {bx, y, bz}, {bx, y - height, bz}, side);
    }
    disk(x, y, z, rx, rz, top);
}
void PokerView::box(float x, float y, float z, float w, float h, float d, uint32_t colour) {
    quad({x - w, y, z - d}, {x + w, y, z - d}, {x + w, y + h, z - d}, {x - w, y + h, z - d}, colour);
    quad({x + w, y, z - d}, {x + w, y, z + d}, {x + w, y + h, z + d}, {x + w, y + h, z - d}, colour);
    quad({x - w, y, z + d}, {x - w, y, z - d}, {x - w, y + h, z - d}, {x - w, y + h, z + d}, colour);
    quad({x - w, y + h, z - d}, {x + w, y + h, z - d}, {x + w, y + h, z + d}, {x - w, y + h, z + d}, colour);
}
void PokerView::glyph(float x, float y, float z, char c, uint32_t colour, float scale) {
    const char *rows = "00000000000000000000000000000000000";
    switch (c) {
    case '0':
        rows = "01110100011000110001100011000101110";
        break;
    case '1':
        rows = "00100011000010000100001000010001110";
        break;
    case '2':
        rows = "01110100010000100010001000100011111";
        break;
    case '3':
        rows = "11110000010000101110000010000111110";
        break;
    case '4':
        rows = "00010001100101010010111110001000010";
        break;
    case '5':
        rows = "11111100001000011110000010000111110";
        break;
    case '6':
        rows = "01110100001000011110100011000101110";
        break;
    case '7':
        rows = "11111000010001000100010000100001000";
        break;
    case '8':
        rows = "01110100011000101110100011000101110";
        break;
    case '9':
        rows = "01110100011000101111000010000101110";
        break;
    case 'J':
        rows = "00111000100001000010000101001001100";
        break;
    case 'Q':
        rows = "01110100011000110001101011001001101";
        break;
    case 'K':
        rows = "10001100101010011000101001001010001";
        break;
    case 'A':
        rows = "01110100011000111111100011000110001";
        break;
    case 'D':
        rows = "11110100011000110001100011000111110";
        break;
    }
    const float p = 0.021f * scale;
    for (int row = 0; row < 7; ++row)
        for (int col = 0; col < 5; ++col)
            if (rows[row * 5 + col] == '1') {
                const float xx = x + col * p, zz = z - row * p;
                quad({xx, y, zz}, {xx + p, y, zz}, {xx + p, y, zz - p}, {xx, y, zz - p}, colour);
            }
}

void PokerView::card(float x, float z, poker::Card c, bool face, float scale) {
    const float w = 0.23f * scale, h = 0.34f * scale, y = 0.045f;
    quad({x - w, y, z - h}, {x + w, y, z - h}, {x + w, y, z + h}, {x - w, y, z + h}, face ? 0xFFF5EFDE : 0xFF662752);
    if (!face) {
        quad({x - w * .7f, y + .002f, z - h * .8f}, {x + w * .7f, y + .002f, z - h * .8f},
             {x + w * .7f, y + .002f, z + h * .8f}, {x - w * .7f, y + .002f, z + h * .8f}, 0xFF9D5874);
        return;
    }
    const uint32_t ink = c / 13 == 1 || c / 13 == 2 ? 0xFFC02837 : 0xFF171622;
    // Use the same full rank as the text labels: ten is always "10", never poker shorthand "T".
    std::string rank = poker::label(c);
    rank.pop_back();
    for (size_t i = 0; i < rank.size(); ++i) {
        glyph(x - w * .85f + i * .126f * scale, y + .004f, z + h * .87f, rank[i], ink, scale);
        glyph(x + w * .85f - i * .126f * scale, y + .004f, z - h * .87f, rank[i], ink, -scale);
    }
    const float d = 0.10f * scale;
    const float cy = y + .006f;
    if (c / 13 == 1)
        quad({x, cy, z - d}, {x + d, cy, z}, {x, cy, z + d}, {x - d, cy, z}, ink);
    else if (c / 13 == 2) {
        disk(x - d * .45f, cy, z + d * .2f, d * .6f, d * .6f, ink);
        disk(x + d * .45f, cy, z + d * .2f, d * .6f, d * .6f, ink);
        tri({x - d, cy, z + d * .2f}, {x + d, cy, z + d * .2f}, {x, cy, z - d}, ink);
    } else if (c / 13 == 3) {
        // Rounded spade shoulders and a separate stem: distinguish it from a plain triangle.
        tri({x, cy, z + d}, {x + d, cy, z - d * .15f}, {x - d, cy, z - d * .15f}, ink);
        disk(x - d * .45f, cy, z - d * .15f, d * .55f, d * .55f, ink);
        disk(x + d * .45f, cy, z - d * .15f, d * .55f, d * .55f, ink);
        tri({x, cy, z}, {x + d * .35f, cy, z - d}, {x - d * .35f, cy, z - d}, ink);
    } else {
        // Three distinct round leaves; generous notches keep clubs distinct from spades.
        disk(x, cy, z + d * .6f, d * .42f, d * .42f, ink);
        disk(x - d * .6f, cy, z - d * .05f, d * .42f, d * .42f, ink);
        disk(x + d * .6f, cy, z - d * .05f, d * .42f, d * .42f, ink);
        disk(x, cy, z - d * .03f, d * .24f, d * .24f, ink);
        tri({x, cy, z}, {x + d * .35f, cy, z - d}, {x - d * .35f, cy, z - d}, ink);
    }
}
void PokerView::draw(float x0, float y0, float x1, float y1, float, float dt) {
    m_time += std::max(0.0f, dt);
    m_faces.clear();
    // Industrial lounge floor, metal supports, padded rail, purple felt.
    quad({-12, -1.9f, -5}, {12, -1.9f, -5}, {12, -1.9f, 12}, {-12, -1.9f, 12}, 0xFF201E2B);
    for (int i = -6; i <= 6; ++i) {
        const float x = i * 1.5f;
        quad({x, -1.89f, -5}, {x + .025f, -1.89f, -5}, {x + .025f, -1.89f, 12}, {x, -1.89f, 12}, 0xFF383141);
    }
    for (float x : {-2.1f, 2.1f})
        box(x, -1.8f, 0, 0.14f, 1.65f, 0.65f, 0xFF51515B);
    cylinder(0, -.1f, 0, 3.8f, 2.05f, .28f, 0xFF16131D, 0xFF15121C);
    cylinder(0, 0, 0, 3.65f, 1.9f, .12f, 0xFF36263F, 0xFFB57A41);
    disk(0, .012f, 0, 3.3f, 1.6f, 0xFF665078);
    disk(0, .014f, 0, 3.23f, 1.53f, 0xFF342744);
    for (int i = 0; i < 6; ++i)
        card(.95f, 1.10f + i * .012f, 0, false, .75f);
    const auto *game = m_game.game();
    if (game != m_lastHand) {
        m_lastHand = game;
        m_dealTime = m_time;
        m_boardTime = m_time;
        m_boardCount = m_previousBoardCount = 0;
    }
    if (game) {
        const auto &hand = game->hand;
        if (m_boardCount != hand.board().size()) {
            m_previousBoardCount = m_boardCount;
            m_boardCount = hand.board().size();
            m_boardTime = m_time;
        }
        const auto fly = [&](float x, float z, poker::Card c, bool face, float scale, float elapsed) {
            const float t = std::clamp(elapsed / .35f, 0.0f, 1.0f);
            const float eased = 1 - (1 - t) * (1 - t);
            card(.95f + (x - .95f) * eased, 1.1f + (z - 1.1f) * eased, c, face && t > .4f, scale);
        };
        for (size_t i = 0; i < hand.board().size(); ++i)
            fly(-1.05f + i * .53f, .2f, hand.board()[i], true, 1,
                i < m_previousBoardCount ? 1 : m_time - m_boardTime - (i - m_previousBoardCount) * .10f);
        fly(-.34f, -1.05f, hand.seats()[0].hole[0], true, 1.15f, m_time - m_dealTime);
        fly(.27f, -1.05f, hand.seats()[0].hole[1], true, 1.15f, m_time - m_dealTime - .15f);
        for (int i = 1; i < m_game.players(); ++i) {
            const float x = -2.65f + (i - 1) * 1.75f;
            if (!hand.seats()[i].folded) {
                card(x, 1.0f, hand.seats()[i].hole[0], hand.finished(), .65f);
                card(x + .34f, 1.0f, hand.seats()[i].hole[1], hand.finished(), .65f);
            }
        }
        for (int i = 0; i < 5; ++i) {
            const int count = std::clamp(hand.pot() / std::max(1, game->buyIn / 20) / 5, 1, 8);
            for (int n = 0; n < count; ++n)
                cylinder(-.7f + i * .32f, .045f + n * .045f, .65f, .13f, .13f, .035f, i % 2 ? 0xFFB14C48 : 0xFFDED4A6,
                         i % 2 ? 0xFF63332F : 0xFF827D68);
        }
    }
    const int button = game ? game->hand.dealer() : 0;
    const float buttonX = button == 0 ? -.9f : -2.95f + (button - 1) * 1.75f;
    const float buttonZ = button == 0 ? -1.05f : 1.0f;
    cylinder(buttonX, .04f, buttonZ, .17f, .17f, .04f, 0xFFF6E9CF, 0xFFBDAF91);
    glyph(buttonX - .05f, .044f, buttonZ + .07f, 'D', 0xFF25202E, 1);
    if (m_gallery) {
        // Inspection sheet uses the exact face mesh drawn in gameplay, all 52 cards.
        m_faces.clear();
        quad({-7, 0, -3}, {7, 0, -3}, {7, 0, 3}, {-7, 0, 3}, 0xFF342744);
        for (int suit = 0; suit < 4; ++suit)
            for (int rank = 0; rank < 13; ++rank)
                card((rank - 6) * .79f, 1.35f - suit * .9f, suit * 13 + rank, true, 1.15f);
    }
    struct Vertex {
        float x, y, z;
        uint32_t colour;
    };
    std::vector<Vertex> verts;
    verts.reserve(m_faces.size() * 3);
    const float w = x1 - x0, h = y1 - y0, focal = std::min(w, h * 1.6f) * .99f;
    for (const auto &face : m_faces)
        for (const auto &p : face.points) {
            const float z = depth(p);
            const float y = p.y * .8f + p.z * .6f;
            const float sx = m_gallery ? x0 + w * .5f + p.x * w / 10.8f : x0 + w * .43f + p.x * focal / z;
            const float sy = m_gallery ? y0 + h * .5f - p.z * w / 10.8f : y0 + h * .59f - y * focal / z;
            const float light = .72f + .35f / (1 + .10f * p.x * p.x + .12f * p.z * p.z);
            uint32_t colour = 0xFF000000;
            for (int shift : {0, 8, 16})
                colour |= static_cast<uint32_t>(std::clamp(((face.colour >> shift) & 255) * light, 0.f, 255.f))
                          << shift;
            verts.push_back({sx / m_device.width() * 2 - 1, 1 - sy / m_device.height() * 2,
                             m_gallery ? .5f - p.y : 1 - 1 / z, colour});
        }
    using namespace render;
    static constexpr float identity[] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    Viewport viewport;
    viewport.width = m_device.width();
    viewport.height = m_device.height();
    m_device.setViewport(viewport);
    const ClipRect clip{x0, y0, x1, y1};
    m_device.setClip(&clip);
    m_device.clear(2, 0, 1, 0);
    m_device.setVertexShader(0);
    m_device.setPixelShader(0);
    m_device.setTransform(TransformState::World, identity);
    m_device.setTransform(TransformState::View, identity);
    m_device.setTransform(TransformState::Projection, identity);
    m_device.setRenderState(RenderState::ZEnable, 1);
    m_device.setRenderState(RenderState::ZWriteEnable, 1);
    m_device.setRenderState(RenderState::ZFunc, 4);
    m_device.setRenderState(RenderState::CullMode, Cull::None);
    m_device.setRenderState(RenderState::Lighting, 0);
    m_device.setRenderState(RenderState::AlphaBlendEnable, 0);
    m_device.setRenderState(RenderState::AlphaTestEnable, 0);
    m_device.setRenderState(RenderState::FogEnable, 0);
    m_device.setRenderState(RenderState::StencilEnable, 0);
    m_device.setRenderState(RenderState::ColourWriteEnable, 15);
    m_device.setTexture(0, 0);
    m_device.setTextureStageState(0, TextureStageState::ColourOp, TextureOp::SelectArg1);
    m_device.setTextureStageState(0, TextureStageState::ColourArg1, TextureArg::Diffuse);
    m_device.setTextureStageState(0, TextureStageState::AlphaOp, TextureOp::SelectArg1);
    m_device.setTextureStageState(0, TextureStageState::AlphaArg1, TextureArg::Diffuse);
    m_device.setTextureStageState(1, TextureStageState::ColourOp, TextureOp::Disable);
    m_device.setFvf(0x42); // XYZ + diffuse: retain depth testing on all native backends.
    m_device.drawUp(4, static_cast<uint32_t>(verts.size() / 3), verts.data(), sizeof(Vertex));
    m_device.setClip(nullptr);
    m_device.setRenderState(RenderState::ZEnable, 0);
    m_device.setRenderState(RenderState::ZWriteEnable, 0);
    m_device.setTextureStageState(0, TextureStageState::ColourOp, TextureOp::Modulate);
    m_device.setTextureStageState(0, TextureStageState::ColourArg1, TextureArg::Texture);
    m_device.setTextureStageState(0, TextureStageState::ColourArg2, TextureArg::Diffuse);
    m_device.setTextureStageState(0, TextureStageState::AlphaOp, TextureOp::Modulate);
    m_device.setTextureStageState(0, TextureStageState::AlphaArg1, TextureArg::Texture);
    m_device.setTextureStageState(0, TextureStageState::AlphaArg2, TextureArg::Diffuse);
}
} // namespace

std::unique_ptr<ui::View3d> makePokerView(render::IRenderDevice &device, const Gamble &game) {
    return std::make_unique<PokerView>(device, game);
}
std::unique_ptr<ui::View3d> makePokerCardGallery(render::IRenderDevice &device, const Gamble &game) {
    return std::make_unique<PokerView>(device, game, true);
}
} // namespace oj::frontend
