// The graphics options (opts_graphics).

#include "Game/Frontend/Options/GraphicsOptions.h"

#include "Core/Data/GameTexts.h"
#include "Core/Input/Input.h"
#include "Core/Saves/GraphicsConfig.h"
#include "Game/UI/WindowFixes.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <span>
#include <utility>
#include <vector>

namespace oj::frontend {

namespace {

// The ADVANCED page's rows (Game/UI/WindowFixes' FX1 .. FX9) top to bottom, each stepped; SHADOWS (the stock's own row,
// moved here: the user, 2026-10-29) with its levels, AMBIENT OCCLUSION its own (the user, 2026-10-29: SSAO out of WORLD
// LIGHT HI, OFF or HI), WORLD LIGHT, SKY and HEADLIGHTS as the reflections' levels: MED the stock's, HI the fixed
// build's, ULTRA more (the headlights' shadows).
enum class AdvancedRow : int {
    kAntialiasing,
    kTextureFilter,
    kCarPaint,
    kShadows,
    kWorldLight,
    kAmbientOcclusion,
    kSky,
    kHeadlights,
    kHeatHaze,
};
constexpr int rowIndex(AdvancedRow row) {
    return static_cast<int>(row);
}
static_assert(rowIndex(AdvancedRow::kHeatHaze) + 1 == ui::kEffectsRows);
static_assert(rowIndex(AdvancedRow::kHeatHaze) == ui::kEffectsMeterRow);
constexpr std::array<const wchar_t*, ui::kEffectsRows> kEffectLabels = {
    L"ANTI-ALIASING", L"TEXTURE FILTER", L"CAR MATERIALS", L"SHADOWS",
    L"WORLD LIGHT",   L"AMBIENT OCCLUSION", L"SKY",   L"HEADLIGHTS", L"HEAT HAZE"};
// HEAT HAZE's meter: ten steps of 5 % (the user, 2026-10-29: at most what 50 % was), none off.
constexpr uint32_t kHazeSteps = 10;
constexpr uint32_t kHazeStep = 5;
constexpr size_t kTextMed = 2, kTextHi = 3;  // kTextOffLowMedHi + 2 "MED", + 3 "HI"
constexpr uint32_t kSmaaKey = 0x100;         // the anti-aliasing's values: MSAA's samples, this added for SMAA
// The REFLECTION row's levels: OFF, LOW, MED, HI (the stock's 0 .. 3), then the fixed build's ULTRA.
constexpr int32_t kUltraReflections = 4;

int32_t reflectionLevel(const GraphicsConfig& c) {
    if (ultraReflections(c)) return kUltraReflections;
    return c.reflections >= 1 && c.reflections <= 3 ? c.reflections : 0;
}

// ULTRA keeps the stock's HI (3) in reg.cf1 and the effects' Reflections in the registry.
void setReflectionLevel(GraphicsConfig& c, int32_t level) {
    c.reflections = std::min(level, 3);
    c.effects.reflections = level == kUltraReflections ? 1 : 0;
}

// The LIGHT FLARES row's levels (the user, 2026-10-28): OFF (the glow 0), STOCK (1), the fixed build's CUSTOM (1 with
// the effects' LightFlares: the exposure at most kCustomExposure, between the two). reg.cf1 keeps the glow 0 or 1.
constexpr int32_t kCustomFlares = 2;

int32_t flaresLevel(const GraphicsConfig& c) {
    if (c.glow == 0) return 0;
    return c.effects.lightFlares != 0 ? kCustomFlares : 1;
}

void setFlaresLevel(GraphicsConfig& c, int32_t level) {
    c.glow = level != 0 ? 1 : 0;
    c.effects.lightFlares = level == kCustomFlares ? 1 : 0;
}

// The graphics presets (the stock page's own row, in the SHADOWS row's place: the user, 2026-10-29; the elements are the
// stock's SHADOWS_BUT and SHADOWS under their own names — the page's navigation is by name — with our texts). MED is
// the stock's own look (a fresh install, with no settings at all, is MED), HI the fixed build's middle, ULTRA its
// top; the row shows CUSTOM when the settings match none of them, and then steps to MED (RIGHT) or ULTRA (LEFT). The
// presets set the effects' rows and the stock's REFLECTION and SHADOWS; the brightness and the detail stay the
// player's, and so does HEAT HAZE (a small setting of its own, the user, 2026-10-29: a preset neither sets it nor
// counts it, see withoutHaze).
constexpr int kPresets = 3;

// The effects as the presets see them: HEAT HAZE left out.
FixedEffects withoutHaze(const FixedEffects& e) {
    FixedEffects copy = e;
    copy.heatHaze = 0;
    copy.heatHazeStrength = 0;
    return copy;
}

void applyPreset(GraphicsConfig& c, int preset) {
    GraphicsConfig stock = c;
    graphicsDefaults(stock);  // the stock's own values for the adapter's class
    c.reflections = stock.reflections;
    c.shadows = stock.shadows;
    c.glow = 1;  // LIGHT FLARES STOCK
    const uint32_t haze = c.effects.heatHaze, hazeStrength = c.effects.heatHazeStrength;
    c.effects = FixedEffects{};
    c.effects.heatHaze = haze;  // HEAT HAZE is the player's own
    c.effects.heatHazeStrength = hazeStrength;
    if (preset == 1) {  // HI
        c.effects.msaa = 2;
        c.effects.textureFilter = 1;
        c.effects.carPaint = 1;
        c.effects.shadows = 1;
        c.effects.worldLight = 1;
        c.effects.ambientOcclusion = 1;
        c.effects.sky = 1;
        c.effects.headlights = 1;
        c.reflections = 3;
    } else if (preset == 2) {  // ULTRA
        c.effects.msaa = 2;
        c.effects.smaa = 1;
        c.effects.textureFilter = 5;
        c.effects.carPaint = 1;
        c.effects.shadows = 2;
        c.effects.worldLight = 2;
        c.effects.ambientOcclusion = 1;
        c.effects.sky = 2;
        c.effects.headlights = 2;
        c.effects.lightFlares = 1;
        c.reflections = 3;
        c.effects.reflections = 1;  // REFLECTIONS ULTRA
    }
}
// The preset the settings are, -1 for none (the row's CUSTOM).
int matchingPreset(const GraphicsConfig& c) {
    for (int i = 0; i < kPresets; ++i) {
        GraphicsConfig copy = c;
        applyPreset(copy, i);
        if (withoutHaze(copy.effects) == withoutHaze(c.effects) && copy.reflections == c.reflections &&
            copy.shadows == c.shadows && copy.glow == c.glow) {
            return i;
        }
    }
    return -1;
}

// The preset row's value (the stock's SHADOWS element under its own name): MED, HI, ULTRA or CUSTOM.
std::wstring presetName(const GraphicsConfig& c) {
    // The game's own words for the levels, as the rows' values use them (its OFF, LOW, MED, HI and the fixed build's
    // ULTRA; the user, 2026-10-30: the page read HI and HIGH, MED and MID).
    static constexpr const wchar_t* kNames[kPresets] = {L"MED", L"HI", L"ULTRA"};
    const int at = matchingPreset(c);
    return at >= 0 ? std::wstring(kNames[at]) : std::wstring(L"CUSTOM");
}

// The stock page's elements (the ADVANCED page's names have none of these).
constexpr std::array<const char*, 10> kStockParts = {"BRIGHT", "DETAIL", "SHADOWS", "REFLECTION", "MOBLUR",
                                                     "GLOW",   "MAP_",   "RESET",   "ARROW",      "SIGN"};

// A value of the list, LEFT and RIGHT round it; a value off the list (set in OpenJuiceConfig) steps to its neighbours
// on it.
uint32_t stepValue(uint32_t value, std::span<const uint32_t> values, bool right) {
    const size_t n = values.size();
    size_t at = 0;
    bool below = true, exact = false;
    for (size_t k = 0; k < n; ++k) {
        if (values[k] <= value) {
            at = k;
            below = false;
        }
        if (values[k] == value) exact = true;
    }
    if (below) return right ? values[0] : values[n - 1];
    return values[right ? (at + 1) % n : exact ? (at + n - 1) % n : at];
}

// HEAT HAZE's steps: none while off, else its strength's nearest (a strength past the meter's end at its end).
uint32_t hazeSteps(const FixedEffects& e) {
    if (e.heatHaze == 0) return 0;
    return std::min((e.heatHazeStrength + kHazeStep / 2) / kHazeStep, kHazeSteps);
}

// A step up (RIGHT) or down, the meter's ends kept; none off.
void stepHaze(FixedEffects& e, bool right) {
    const uint32_t steps = hazeSteps(e);
    const uint32_t next = right ? std::min(steps + 1, kHazeSteps) : (steps > 0 ? steps - 1 : 0);
    e.heatHaze = next > 0 ? 1 : 0;
    e.heatHazeStrength = next * kHazeStep;
}

// A row of MED (0), HI (1) and ULTRA (2): RIGHT up, LEFT down, round.
uint32_t stepLevel(uint32_t value, bool right) {
    return (std::min(value, 2u) + (right ? 1u : 2u)) % 3u;
}

// The settings the programs are made with: the car's effects (CAR PAINT, REFLECTION ULTRA: render::carEffectsOf), the
// trees' (WORLD LIGHT ULTRA) and the shaders the device replaces (the headlights' HI and ULTRA the same).
bool programsDiffer(const FixedEffects& a, const FixedEffects& b) {
    return a.carPaint != b.carPaint || (a.reflections != 0) != (b.reflections != 0) || a.worldLight != b.worldLight ||
           (a.headlights != 0) != (b.headlights != 0);
}

// The anti-aliasing's values, rising: OFF, MSAA 2X, 4X and 8X (those the adapter takes), SMAA and SMAA + MSAA 2X (at
// shaders 3.0).
std::vector<uint32_t> antialiasingValues(const render::QualityCaps& caps) {
    const auto takes = [&](uint32_t samples) {
        return std::find(caps.multisampleCounts.begin(), caps.multisampleCounts.end(), samples) !=
               caps.multisampleCounts.end();
    };
    std::vector<uint32_t> values = {0};
    for (const uint32_t samples : {2u, 4u, 8u}) {
        if (takes(samples)) values.push_back(samples);
    }
    if (caps.smaa) {
        values.push_back(kSmaaKey);
        if (takes(2)) values.push_back(kSmaaKey | 2);
    }
    return values;
}

uint32_t antialiasingValue(const FixedEffects& e) {
    return (e.smaa != 0 ? kSmaaKey : 0) | std::min(e.msaa, 0xFFu);
}

std::wstring antialiasingText(const FixedEffects& e, const std::wstring& off) {
    const std::wstring msaa = e.msaa >= 2 ? std::format(L"MSAA {}X", e.msaa) : std::wstring();
    if (e.smaa != 0) return msaa.empty() ? std::wstring(L"SMAA") : L"SMAA + " + msaa;
    return msaa.empty() ? off : msaa;
}

// The texture filter's values: OFF (the game's), TRILINEAR, ANISO 2X .. 16X up to the adapter's anisotropy.
std::vector<uint32_t> textureFilterValues(const render::QualityCaps& caps) {
    std::vector<uint32_t> values = {0, 1};
    for (uint32_t filter = 2, anisotropy = 2; filter <= 5 && anisotropy <= caps.maxAnisotropy; ++filter) {
        values.push_back(filter);
        anisotropy *= 2;
    }
    return values;
}

std::wstring textureFilterText(uint32_t filter, const std::wstring& off) {
    if (filter == 0) return off;
    return filter == 1 ? std::wstring(L"TRILINEAR") : std::format(L"ANISO {}X", 1u << (filter - 1));
}

}  // namespace

// 0x00481F00: BRIGHT, DETAIL, SHADOWS, REFLECTION, MOBLUR, GLOW, DOF (+0x10 ..) found; the meters DETAIL and BRIGHT
// sliders (+0xFC) at the detail ([0x0079E650]) and the brightness ([0x0079E660]), value and shown value; kept for
// BACK: the brightness, the detail, the reflections, the shadows on (2; none without shaders), the motion blur, glow
// and depth of field (none without shaders or with no shaders on the adapter); RESET_BUT "RESET TO DEFAULTS" (1609);
// +0x0C 0.
void GraphicsOptions::enter(OptionsHost& host, ui::Screen& s) {
    const GraphicsConfig& c = graphicsConfig();
    for (const auto& [meter, value] : {std::pair{"DETAIL", c.detail}, std::pair{"BRIGHT", c.brightness}}) {
        if (s.find(meter) < 0) continue;
        s.setMeterSlider(meter, true);
        s.setMeter(meter, value, true);
    }
    const bool effects = c.noShaders == 0 && c.shaders != 0;
    m_brightness = c.brightness;
    m_detail = c.detail;
    m_reflections = c.reflections;
    m_motionBlur = effects ? c.motionBlur : 0;
    m_glow = effects ? c.glow : 0;
    m_caps = host.qualityCaps();
    s.setText("RESET_BUT", host.gameTexts().at(kTextResetDefaults));
    s.setText("SHADOWS_BUT", L"GRAPHICS");  // the stock's SHADOWS row is the graphics presets' now
    m_shownButton.clear();
    // The fixed build's ADVANCED page: the stock's page first.
    m_effects = c.effects;
    m_madeWith = c.effects;
    m_effectsPage = false;
    m_paged = s.find(ui::kMoreButton) >= 0;
    if (m_paged) {
        for (int i = 0; i < ui::kEffectsRows; ++i) s.setText(ui::effectsButton(i), kEffectLabels[static_cast<size_t>(i)]);
        setPage(host, s, false);
    }
}

void GraphicsOptions::done(OptionsHost& host, ui::Screen& s) {
    GraphicsConfig& c = graphicsConfig();
    // The effects changed (on the ADVANCED page, or the REFLECTION row's ULTRA on the stock's) in effect, the programs
    // made again when they were made with others.
    if (c.effects != m_effects || programsDiffer(m_madeWith, c.effects)) applyEffects(host, c.effects);
    const float detail = s.meterShown("DETAIL");
    c.brightness = s.meterShown("BRIGHT");
    leave(host, detail);
}

void GraphicsOptions::setPage(OptionsHost& host, ui::Screen& s, bool effects) {
    m_effectsPage = effects;
    for (const char* part : kStockParts) {
        if (effects) {
            s.hideContaining(part);
        } else {
            s.showContaining(part, false);
        }
    }
    if (effects) {
        s.showContaining(ui::kEffectsPart, false);
    } else {
        s.hideContaining(ui::kEffectsPart);
    }
    s.setText(ui::kMoreButton, effects ? host.gameTexts().at(kTextBack) : std::wstring(L"ADVANCED"));
    s.setNeighbour("DONE_BUT", input::kDown, effects ? ui::effectsButton(0) : "BRIGHT_BUT");
    s.setNeighbour("DONE_BUT", input::kUp, effects ? ui::kMoreButton : "RESET_BUT");
    s.setNeighbour(ui::kMoreButton, input::kUp, effects ? ui::effectsButton(ui::kEffectsRows - 1) : "GLOW_BUT");
    s.setNeighbour(ui::kMoreButton, input::kDown, effects ? "DONE_BUT" : "RESET_BUT");
}

void GraphicsOptions::showPage(OptionsHost& host, ui::Screen& s, bool effects) {
    FixedEffects& e = graphicsConfig().effects;
    if (m_effectsPage && !effects && programsDiffer(m_madeWith, e)) applyEffects(host, e);
    setPage(host, s, effects);
    if (effects) {
        s.setCurrentButton(ui::effectsButton(rowIndex(AdvancedRow::kAntialiasing)));
        showEffects(host, s);
    } else {
        s.setCurrentButton(ui::kMoreButton);
    }
    m_shownButton.clear();
}

void GraphicsOptions::showEffects(OptionsHost& host, ui::Screen& s) {
    const GraphicsConfig& c = graphicsConfig();
    const FixedEffects& e = c.effects;
    const std::wstring off = host.gameTexts().at(kTextOffLowMedHi);
    // MED (0), HI (1), ULTRA (2).
    const auto level = [&](uint32_t value) {
        if (value >= 2) return std::wstring(L"ULTRA");
        return host.gameTexts().at(kTextOffLowMedHi + (value != 0 ? kTextHi : kTextMed));
    };
    // SHADOWS: OFF (0), MED the stock's (1), HI (2), ULTRA (3).
    const int shadows = shadowLevel(c);
    const std::wstring shadowsValue =
        shadows >= 3 ? std::wstring(L"ULTRA")
                     : host.gameTexts().at(kTextOffLowMedHi + (shadows == 0 ? 0 : static_cast<size_t>(shadows) + 1));
    const std::wstring texts[ui::kEffectsRows] = {
        antialiasingText(e, off),
        textureFilterText(e.textureFilter, off),
        e.carPaint != 0 ? host.gameTexts().at(kTextOffLowMedHi + kTextHi)
                        : host.gameTexts().at(kTextOffLowMedHi + kTextMed),
        shadowsValue,
        level(e.worldLight),
        e.ambientOcclusion != 0 ? host.gameTexts().at(kTextOffLowMedHi + kTextHi) : off,
        level(e.sky),
        level(e.headlights),
        std::wstring()};
    for (int i = 0; i < ui::kEffectsRows; ++i) {
        if (i != ui::kEffectsMeterRow) s.setText(ui::effectsValue(i), texts[i]);
    }
    s.setMeter(ui::kEffectsMeter, static_cast<float>(hazeSteps(e)) / static_cast<float>(kHazeSteps), true);
}

// The ADVANCED page: LEFT and RIGHT step the rows round their values (the anti-aliasing, the texture filter and the
// heat haze in effect at once; MSAA's new samples reset the device as the next frame begins); BACK or SELECT on
// MORE_BUT turn back to the stock's page (the programs made again when the car paint, the world light or the
// headlights changed), DONE leaves as on the stock's page.
void GraphicsOptions::updateEffects(OptionsHost& host, ui::Screen& s, uint32_t event) {
    FixedEffects& e = graphicsConfig().effects;
    const FixedEffects before = e;
    const std::string current = s.currentButton();
    int row = -1;
    for (int i = 0; i < ui::kEffectsRows; ++i) {
        if (_stricmp(current.c_str(), ui::effectsButton(i).c_str()) == 0) row = i;
    }
    if (event == input::kBack || (event == input::kSelect && _stricmp(current.c_str(), ui::kMoreButton) == 0)) {
        host.sound("CLICK");
        if (e != before) host.applyEffects(e, false);
        showPage(host, s, false);
        return;
    }
    if (event == input::kSelect && _stricmp(current.c_str(), "DONE_BUT") == 0) {
        done(host, s);
        return;
    }
    if ((event == input::kLeft || event == input::kRight) && row >= 0) {
        const bool right = event == input::kRight;
        switch (static_cast<AdvancedRow>(row)) {
        case AdvancedRow::kAntialiasing: {
            const uint32_t value = stepValue(antialiasingValue(e), antialiasingValues(m_caps), right);
            e.msaa = value & 0xFF;
            e.smaa = (value & kSmaaKey) != 0 ? 1 : 0;
            break;
        }
        case AdvancedRow::kTextureFilter:
            e.textureFilter = stepValue(e.textureFilter, textureFilterValues(m_caps), right);
            break;
        case AdvancedRow::kCarPaint: e.carPaint = e.carPaint != 0 ? 0 : 1; break;
        case AdvancedRow::kShadows: {
            GraphicsConfig& c = graphicsConfig();
            const int level = shadowLevel(c) + (right ? 1 : -1);
            setShadowLevel(c, level < 0 ? 0 : (level > 3 ? 3 : level));
            break;
        }
        case AdvancedRow::kWorldLight: e.worldLight = stepLevel(e.worldLight, right); break;
        case AdvancedRow::kAmbientOcclusion: e.ambientOcclusion = e.ambientOcclusion != 0 ? 0 : 1; break;
        case AdvancedRow::kSky: e.sky = stepLevel(e.sky, right); break;
        case AdvancedRow::kHeadlights: e.headlights = stepLevel(e.headlights, right); break;
        case AdvancedRow::kHeatHaze: stepHaze(e, right); break;
        }
        host.sound("CLICK");
    }
    showEffects(host, s);
    if (e != before) host.applyEffects(e, false);
}

void GraphicsOptions::applyEffects(OptionsHost& host, const FixedEffects& e) {
    const bool programs = programsDiffer(m_madeWith, e);
    host.applyEffects(e, programs);
    if (programs) m_madeWith = e;
}

void GraphicsOptions::leave(OptionsHost& host, float detail) {
    GraphicsConfig& c = graphicsConfig();
    if (c.effects != m_effects) host.keepEffects(c.effects);
    c.detail = detail;
    c.lodBias = detailFactor(detail);
    host.setDetailFactor(c.lodBias);
    host.sound("CLICK");
    // From a race's pause ([game + 0x3C]) opts_ingame_menu.
    host.request(host.optionsMenu(), SwitchMode::Swap);
}

// 0x004820A0. A new highlighted button (+0x0C): REFLECTION "OFF", "LOW", "MED", "HI" (1486 ..) by the reflections
// (another value "OFF"; the fixed build's "ULTRA" after HI), SHADOWS "YES" for 2, else "NO", MOBLUR, GLOW, DOF "YES" or
// "NO" by theirs; "-" for each
// without shaders (and the three effects with no shaders on the adapter). The meters DETAIL and BRIGHT take the held
// direction while their buttons are highlighted (+0xF8). On BRIGHT_BUT the brightness follows BRIGHT's shown value.
// The fixed build has no DOF row (Game/UI/WindowFixes: the depth of field removed, the user, 2026-10-28). Its GLOW
// (LIGHT FLARES) reads OFF, STOCK or CUSTOM and steps round them.
// BACK: the kept values back (the reflections and the effects 0 without shaders, the shadows 2 or 1), the detail factor
// from the detail, CLICK, opts_menu (mode 7). SELECT on DONE_BUT: the detail DETAIL's shown value and the factor from
// it, the brightness BRIGHT's, CLICK, opts_menu; on RESET_BUT: CLICK, the adapter's defaults (0x005A26C0), the meters
// at the detail and the brightness, +0x0C 0. LEFT or RIGHT: SHADOWS_BUT turns the shadows over (1 or 2), MOBLUR_BUT,
// GLOW_BUT, DOF_BUT their effect (stays off without shaders), REFLECTION_BUT the reflections one down (LEFT) or up
// (RIGHT) round 0 .. 3 (0 without shaders; the fixed build's round OFF .. ULTRA); each CLICK and +0x0C 0. The fixed
// build's RESET takes ULTRA off.
void GraphicsOptions::update(OptionsHost& host, ui::Screen& s, uint32_t event) {
    if (m_effectsPage) {
        updateEffects(host, s, event);
        return;
    }
    GraphicsConfig& c = graphicsConfig();
    const std::string current = s.currentButton();
    const auto is1 = [&](const char* name) { return _stricmp(current.c_str(), name) == 0; };
    const bool shaderless = c.noShaders != 0;
    const bool effects = c.noShaders == 0 && c.shaders != 0;
    if (m_shownButton != current) {
        m_shownButton = current;
        const auto yesNo = [&](bool on) { return host.gameTexts().at(on ? kTextYes : kTextNo); };
        if (shaderless) {
            s.setText("REFLECTION", L"-");
        } else {
            const int32_t level = reflectionLevel(c);
            s.setText("REFLECTION", level == kUltraReflections
                                        ? std::wstring(L"ULTRA")
                                        : host.gameTexts().at(kTextOffLowMedHi + static_cast<size_t>(level)));
            s.setText("SHADOWS", presetName(c));
        }
        if (effects) {
            s.setText("MOBLUR", yesNo(c.motionBlur != 0));
            const int32_t flares = flaresLevel(c);
            s.setText("GLOW", flares == 0 ? host.gameTexts().at(kTextOffLowMedHi)
                                          : std::wstring(flares == kCustomFlares ? L"CUSTOM" : L"STOCK"));
        } else {
            for (const char* name : {"MOBLUR", "GLOW"}) s.setText(name, L"-");
        }
        s.setMeterActive("DETAIL", is1("DETAIL_BUT"));
        s.setMeterActive("BRIGHT", is1("BRIGHT_BUT"));
    }
    if (s.find("BRIGHT") >= 0 && is1("BRIGHT_BUT")) c.brightness = s.meterShown("BRIGHT");
    if (event == input::kBack) {
        c.brightness = m_brightness;
        c.reflections = shaderless ? 0 : m_reflections;
        c.motionBlur = effects ? m_motionBlur : 0;
        c.glow = effects ? m_glow : 0;
        if (c.effects != m_effects) {
            c.effects = m_effects;
            applyEffects(host, c.effects);
        }
        leave(host, m_detail);
        return;
    }
    if (event == input::kSelect) {
        if (is1("DONE_BUT")) {
            done(host, s);
        } else if (m_paged && is1(ui::kMoreButton)) {
            host.sound("CLICK");
            showPage(host, s, true);
        } else if (is1("RESET_BUT")) {
            // The stock's own look, the same every time (the user, 2026-10-29: it took whatever the effects were, so
            // ULTRA left a mixture of the two and HI a CUSTOM). graphicsDefaults is the stock's own 0x005A26C0 for
            // every value; MED (the presets' first) the same for the rows the presets own with the fixed build's
            // effects off, so the presets' row reads MED. The player's HEAT HAZE is left (a preset's own rule). The
            // shaders are made again at once, as the ADVANCED page's rows do.
            host.sound("CLICK");
            graphicsDefaults(c);
            applyPreset(c, 0);
            if (c.effects != m_effects || programsDiffer(m_madeWith, c.effects)) applyEffects(host, c.effects);
            if (s.find("DETAIL") >= 0) s.setMeter("DETAIL", c.detail, true);
            if (s.find("BRIGHT") >= 0) s.setMeter("BRIGHT", c.brightness, true);
            m_shownButton.clear();
        }
        return;
    }
    if (event != input::kLeft && event != input::kRight) return;
    const auto changed = [&] {
        host.sound("CLICK");
        m_shownButton.clear();
    };
    if (is1("SHADOWS_BUT")) {
        // The graphics presets' row (the stock's SHADOWS element under its own name): RIGHT and LEFT step the
        // presets round, from CUSTOM to MED (RIGHT) or ULTRA (LEFT).
        const int at = matchingPreset(c);
        const int next = at < 0 ? (event == input::kRight ? 0 : kPresets - 1)
                                : (at + (event == input::kRight ? 1 : kPresets - 1)) % kPresets;
        applyPreset(c, next);
        m_shownButton.clear();
        host.sound("CLICK");
    } else if (is1("GLOW_BUT")) {
        setFlaresLevel(c, effects ? (flaresLevel(c) + (event == input::kLeft ? 2 : 1)) % 3 : 0);
        changed();
    } else if (is1("MOBLUR_BUT")) {
        const bool on = effects && c.motionBlur != 0;
        c.motionBlur = !on && effects ? 1 : 0;
        changed();
    } else if (is1("REFLECTION_BUT")) {
        int32_t r = reflectionLevel(c) + (event == input::kLeft ? -1 : 1);
        if (r < 0) {
            r = kUltraReflections;
        } else if (r > kUltraReflections) {
            r = 0;
        }
        setReflectionLevel(c, shaderless ? 0 : r);
        changed();
    }
}

}  // namespace oj::frontend
