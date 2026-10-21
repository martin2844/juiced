#include "Core/Data/EffectDefs.h"

#include "Core/Base/Script.h"
#include "Core/Base/X87.h"

#include <bit>
#include <cmath>
#include <cstring>
#include <optional>

namespace oj {

namespace {

bool same(std::string_view a, const char* b) {
    return a.size() == std::strlen(b) && _strnicmp(a.data(), b, a.size()) == 0;
}

// 0x00507350: the BASE_TYPE's word; an unknown one leaves the type.
void setType(EffectDef& d, const std::string& word) {
    static const struct {
        const char* name;
        EffectType type;
    } kTypes[] = {{"SMOKE", EffectType::Smoke},          {"WHEELSPIN", EffectType::Wheelspin},
                  {"LEAVES", EffectType::Leaves},        {"TRACKS", EffectType::Tracks},
                  {"SPARKS", EffectType::Sparks},        {"FLAKES", EffectType::Flakes},
                  {"FRAMEBUFFER", EffectType::Framebuffer}, {"GROUNDRUSH", EffectType::GroundRush},
                  {"FLAT", EffectType::Flat},            {"RAIN", EffectType::Rain},
                  {"SPLASH", EffectType::Splash},        {"SMEAR", EffectType::Smear},
                  {"GRASS", EffectType::Grass}};
    for (const auto& t : kTypes) {
        if (_stricmp(word.c_str(), t.name) == 0) {
            d.type = t.type;
            return;
        }
    }
}

Vec4 colourOf(const ScriptLine& line) {
    return {line.real(0), line.real(1), line.real(2), line.real(3)};
}

// 0x00508340 before the definition is added, on the loading thread (the x87 unit in double precision): the count of
// points (and for smoke and sparks their sizes) by the particle density; the counts cut toward zero, at most 3000.
void applyDensity(EffectDef& d, float density) {
    if (density == 0.0f) return;
    const double dd = density;
    const double n = static_cast<double>(d.points);
    if (density >= 1.0f) {
        switch (d.type) {
        case EffectType::Smoke: {
            const double f = dd * static_cast<double>(0.125f) + 1.0;
            d.points = static_cast<uint32_t>(x87::chopDouble(n * f));
            d.growthRate = static_cast<float>((1.0 / f) * static_cast<double>(d.growthRate));
            break;
        }
        case EffectType::Leaves:
            d.points = static_cast<uint32_t>(x87::chopDouble(n * (dd + dd)));
            break;
        case EffectType::Sparks: {
            d.points = static_cast<uint32_t>(x87::chopDouble(n * dd * 0.5));
            const double over = dd - 1.0;
            d.startSize = static_cast<float>(static_cast<double>(d.startSize) /
                                             (static_cast<double>(0.1f) * over + 1.0));
            d.growthRate = static_cast<float>(static_cast<double>(d.growthRate) /
                                              (over * static_cast<double>(0.05f) + 1.0));
            break;
        }
        default:
            break;
        }
    } else if (d.points != 0) {
        d.points = static_cast<uint32_t>(x87::chopDouble(n * dd));
    }
    if (d.points > 3000) d.points = 3000;
}

}  // namespace

void EffectLibrary::read(std::string_view text, float density, bool sound) {
    std::optional<EffectDef> open;
    for (const ScriptLine& line : parseScript(text)) {
        const std::string& k = line.keyword;
        if (same(k, "[BeginEffect]")) {
            open.emplace();
            const std::string& name = line.word(0);
            if (name.size() < 0x40) open->name = name;
            continue;
        }
        if (!open) continue;
        EffectDef& d = *open;
        if (same(k, "[EndEffect]")) {
            applyDensity(d, density);
            m_defs.push_back(std::move(d));
            open.reset();
        } else if (same(k, "SAME_AS")) {
            // 0x00506ED0: everything of the first definition of the name but the name itself (its points already
            // scaled: they are scaled again at the end); a name with no definition is not looked for further.
            if (line.words.empty()) continue;
            if (const EffectDef* from = find(line.words[0])) {
                const std::string name = d.name;
                d = *from;
                d.name = name;
            }
        } else if (same(k, "MATERIAL_NAME")) {
            const std::string& m = line.word(0);
            if (m.size() < 0x40) d.material = m;
        } else if (same(k, "BASE_TYPE")) {
            setType(d, line.word(0));
        } else if (same(k, "NUM_POINTS")) {
            // 0x00507260: the number (as an unsigned one) times the reader's scale (the manager's +0xCC, 1).
            d.points = static_cast<uint32_t>(
                x87::chopDouble(static_cast<double>(static_cast<uint32_t>(line.integer(0))) * 1.0));
        } else if (same(k, "COLOUR_START")) {
            d.colourStart = colourOf(line);
        } else if (same(k, "COLOUR_END")) {
            d.colourEnd = colourOf(line);
        } else if (same(k, "COLOUR_VARY")) {
            d.colourVary = colourOf(line);
        } else if (same(k, "BRIGHTNESS_RANGE")) {
            d.brightnessRange = line.real(0);
        } else if (same(k, "EMIT_INTERVAL")) {
            d.emitInterval = line.real(0);
        } else if (same(k, "AGE_RANGE")) {
            d.ageRange = line.real(0);
        } else if (same(k, "START_SIZE")) {
            d.startSize = line.real(0);
        } else if (same(k, "GROWTH_RATE")) {
            d.growthRate = line.real(0);
        } else if (same(k, "GRAVITY")) {
            d.gravity = line.real(0);
        } else if (same(k, "FOLLOW_FRAC")) {
            d.followFrac = line.real(0);
        } else if (same(k, "FOLLOW_FADE")) {
            d.followFade = line.real(0);
        } else if (same(k, "ROTATION_RATE")) {
            d.rotationRate = line.real(0);
        } else if (same(k, "FADEIN_TIME")) {
            d.fadeinTime = line.real(0);
        } else if (same(k, "INITIAL_TEMP")) {
            d.initialTemp = line.real(0);
        } else if (same(k, "COOLING_RATE")) {
            d.coolingRate = line.real(0);
        } else if (same(k, "WIND_RESISTANCE")) {
            d.windResistance = line.real(0);
        } else if (same(k, "EMIT_DIRECTION")) {
            d.emitDirection = {line.real(0), line.real(1), line.real(2), d.emitDirection[3]};
        } else if (same(k, "EMIT_SPEED")) {
            d.emitSpeed = line.real(0);
        } else if (same(k, "SPEED_RANGE")) {
            d.speedRange = line.real(0);
        } else if (same(k, "RANDOM_DRIFT")) {
            d.randomDrift = line.real(0);
        } else if (same(k, "TRIGGER_LEVEL")) {
            d.triggerLevel = line.real(0);
        } else if (same(k, "MIN_AMOUNT")) {
            d.minAmount = line.real(0);
        } else if (same(k, "SOUND_EFFECT")) {
            // 0x00507E50: only with the sound on; the value 1 when the line has no second word.
            if (!sound) continue;
            EffectSound s;
            s.name = line.word(0).substr(0, 0x3F);
            s.value = line.words.size() > 1 ? line.integer(1) : 1;
            d.hasSound = true;
            d.sounds.push_back(s);
        } else if (same(k, "NO_SOUND_EFFECT")) {
            d.sounds.clear();
            d.hasSound = false;
        } else if (same(k, "SOUND_VOLUME")) {
            d.soundVolume = line.real(0);
        } else if (same(k, "SOUND_ROLLOFF")) {
            d.soundRolloff = line.real(0);
        } else if (same(k, "SOUND_MINDIST")) {
            d.soundMinDist = line.real(0);
        } else if (same(k, "SOUND_PITCHMUL")) {
            d.soundPitchMul = line.real(0);
        } else if (same(k, "SOUND_TRIGGER_LEVEL")) {
            d.soundTriggerLevel = line.real(0);
        } else if (same(k, "SOUND_TRIGGER")) {
            d.soundTrigger = line.integer(0);
        } else if (same(k, "SOUND_MIN_VOL")) {
            d.soundMinVol = line.real(0);
        } else if (same(k, "SOUND_DOPPLER")) {
            d.soundDoppler = line.real(0);
        } else if (same(k, "LOD_OFF_MUL")) {
            d.lodOffMul = line.real(0) * 1000000.0f;
        } else if (same(k, "NUM_TEXTURES")) {
            d.textures = line.integer(0);
        } else if (same(k, "BOUNCINESS")) {
            d.bounciness = line.real(0);
        } else if (same(k, "MAX_VIS_SIZE")) {
            d.maxVisSize = line.real(0);
        } else if (same(k, "LIFETIME")) {
            d.lifetime = line.real(0);
        }
        // NUM_POINTS_PS2 is read and dropped.
    }
}

const EffectDef* EffectLibrary::find(std::string_view name) const {
    for (const EffectDef& d : m_defs) {
        if (same(name, d.name.c_str())) return &d;
    }
    return nullptr;
}

SurfaceEffects parseSurfaceEffects(std::string_view text) {
    static const char* const kPlaces[] = {"TARMAC_DRY", "SAND_DRY", "GRASS_DRY", "TARMAC_WET", "SAND_WET",
                                          "GRASS_WET",  "CONCRETE", "METAL",    "WOOD",       "GLASS"};
    SurfaceEffects table;
    std::optional<SurfaceEffect> open;
    size_t place = 0;
    for (const ScriptLine& line : parseScript(text)) {
        const std::string& k = line.keyword;
        const std::string& word = line.word(0);
        auto take = [&word](std::string& field) {
            if (word.size() < 0x40) field = word;
        };
        if (same(k, "[BeginSurfaceEffect]")) {
            // 0x004929B0: WHEELSPIN, TRACKS and SKID are emptied (SPARKS and FLAKES keep what the memory held).
            open.emplace();
            take(open->name);
            place = 0;
            for (size_t i = 0; i < 10; ++i) {
                if (_stricmp(word.c_str(), kPlaces[i]) == 0) {
                    place = i;
                    break;
                }
            }
            continue;
        }
        if (!open) continue;
        if (same(k, "WHEELSPIN")) {
            take(open->wheelspin);
        } else if (same(k, "SKID")) {
            take(open->skid);
        } else if (same(k, "TRACKS")) {
            take(open->tracks);
        } else if (same(k, "SPARKS")) {
            take(open->sparks);
        } else if (same(k, "FLAKES")) {
            take(open->flakes);
        } else if (same(k, "RUSH")) {
            if (word.size() < 0x40) {
                if (_stricmp(word.c_str(), "ground_rush_street") == 0) {
                    open->rush = 0;
                } else if (_stricmp(word.c_str(), "ground_rush_scrub") == 0) {
                    open->rush = 1;
                }
            }
        } else if (same(k, "[EndSurfaceEffect]")) {
            table[place] = std::move(*open);
            open.reset();
        }
    }
    return table;
}

const EffectDef* makeEmitter(const EffectLibrary& library, std::string_view name, CrtRandom& random, bool sound,
                             int32_t* draw) {
    if (draw != nullptr) *draw = -1;
    const EffectDef* d = library.find(name);
    if (d == nullptr) return nullptr;
    if (d->type == EffectType::Flakes || d->type == EffectType::Flat || d->type == EffectType::Grass) return nullptr;
    if (sound && d->hasSound && d->soundTrigger == 1) {
        const int32_t r = random.next();
        if (draw != nullptr) *draw = r;
    }
    if (d->type == EffectType::Leaves) {
        LeafTables& t = leafTables();
        if (!t.made) {
            t.made = true;
            const auto n = static_cast<uint32_t>(d->textures);
            const float across = 1.0f / static_cast<float>(((n - 1) & 3) + 1);
            const float down = 1.0f / static_cast<float>(((n - 1) >> 2) + 1);
            const float angle = std::bit_cast<float>(0x3949116Du);  // [0x006FFDC4]: 2 pi / 32767
            const float half = std::bit_cast<float>(0x37800100u);   // [0x006FFB88]: 1 / 65534
            for (uint32_t i = 0; i < LeafTables::kEntries; ++i) {
                t.turnA[i] = static_cast<float>(random.next()) * angle;
                t.turnB[i] = static_cast<float>(random.next()) * angle;
                t.rate[i] = static_cast<float>(random.next()) * half + 0.25f;
                t.light[i] = 0.0f;
                float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
                if (n > 1) {
                    const uint32_t k = i % n;
                    u0 = static_cast<float>(k & 3) * across;
                    v0 = static_cast<float>(k >> 2) * down;
                    u1 = across + u0;
                    v1 = v0 + down;
                }
                t.u0[i] = u0;
                t.u1[i] = u1;
                t.v0[i] = v0;
                t.v1[i] = v1;
                const float c = std::cos(t.turnA[i]);
                const float s = std::sin(t.turnA[i]);
                t.flatA[i] = {c, 0.0f, s, 0.0f};
                t.flatB[i] = {-s, 0.0f, c, 0.0f};
            }
        }
    }
    return d;
}

LeafTables& leafTables() {
    static LeafTables tables;
    return tables;
}

float particleDensity(float commandLine, uint32_t shaderClasses, uint32_t threading, uint32_t processors) {
    float density = commandLine;
    if ((shaderClasses & 0x40000u) != 0 && threading != 0 && processors > 1 && density == 0.0f) density = 8.0f;
    if ((shaderClasses & 0xE0000u) == 0 && density == 0.0f) density = 0.5f;
    return density;
}

}  // namespace oj
