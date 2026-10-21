#include "Core/Tracks/Atmosphere.h"

#include "Core/Base/Script.h"
#include "Core/Base/X87.h"

#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace oj {

namespace {

const float kLightDegrees = std::bit_cast<float>(0x3C8EF921u);  // 0.0174528 (0x006FFCD0)
const float kSkyDegrees = std::bit_cast<float>(0x3C8EFA35u);    // pi / 180 (0x006FF9B0)

// A word of the line as atof gives it (double); 0 when the line has no such word.
double number(const ScriptLine& line, size_t i) {
    return i < line.words.size() ? std::atof(line.words[i].c_str()) : 0.0;
}

float real(const ScriptLine& line, size_t i) {
    return static_cast<float>(number(line, i));
}

// The low byte of atol.
uint32_t byteOf(const ScriptLine& line, size_t i) {
    return i < line.words.size() ? static_cast<uint32_t>(std::atol(line.words[i].c_str())) & 0xFF : 0;
}

// "r g b" as 0xFFRRGGBB.
uint32_t colourOf(const ScriptLine& line) {
    return 0xFF000000u | byteOf(line, 0) << 16 | byteOf(line, 1) << 8 | byteOf(line, 2);
}

void floats3(const ScriptLine& line, float (&out)[4]) {
    out[0] = real(line, 0);
    out[1] = real(line, 1);
    out[2] = real(line, 2);
}

// Elevation and azimuth in degrees (0x005324D0): (sin az cos el, -sin el, cos az cos el). The elevation is rounded
// to a float before the factor, the azimuth is not; sin and cos keep the x87's extended precision.
void direction(const ScriptLine& line, float (&out)[4]) {
    const float elevation = static_cast<float>(real(line, 0) * kLightDegrees);
    const float azimuth = static_cast<float>(number(line, 1) * kLightDegrees);
    const double ce = std::cos(static_cast<double>(elevation));
    out[0] = static_cast<float>(std::sin(static_cast<double>(azimuth)) * ce);
    out[1] = static_cast<float>(-std::sin(static_cast<double>(elevation)));
    out[2] = static_cast<float>(std::cos(static_cast<double>(azimuth)) * ce);
}

std::string word32(const ScriptLine& line) {
    // strncpy of 32 characters into a 32-byte field.
    std::string w = line.word(0);
    if (w.size() > 32) w.resize(32);
    return w;
}

bool is(const ScriptLine& line, const char* key) {
    return _stricmp(line.keyword.c_str(), key) == 0;
}

}  // namespace

std::vector<Atmosphere> parseAtmospheres(std::string_view text) {
    std::vector<Atmosphere> list;
    for (const ScriptLine& line : parseScript(text)) {
        if (is(line, "[BeginAtmosphere]")) {
            Atmosphere a;
            if (!line.words.empty() && line.words[0].size() < 32) a.name = line.words[0];
            list.push_back(std::move(a));
            continue;
        }
        if (list.empty()) continue;
        Atmosphere& a = list.back();
        if (is(line, "FogColour")) {
            a.fogFront = a.fogBack = colourOf(line);
        } else if (is(line, "FogColourFront")) {
            a.fogFront = colourOf(line);
        } else if (is(line, "FogColourBack")) {
            a.fogBack = colourOf(line);
        } else if (is(line, "FogMin")) {
            a.fogMin = real(line, 0);
        } else if (is(line, "FogMax")) {
            a.fogMax = real(line, 0);
        } else if (is(line, "FogNear")) {
            a.fogNear = real(line, 0);
        } else if (is(line, "FogFar")) {
            a.fogFar = real(line, 0);
        } else if (is(line, "AmbientColour")) {
            floats3(line, a.ambient);
        } else if (is(line, "LightColour1")) {
            floats3(line, a.lightColour[0]);
        } else if (is(line, "LightColour2")) {
            floats3(line, a.lightColour[1]);
        } else if (is(line, "LightColour3")) {
            floats3(line, a.lightColour[2]);
        } else if (is(line, "LightDirection1")) {
            direction(line, a.lightDirection[0]);
        } else if (is(line, "LightDirection2")) {
            direction(line, a.lightDirection[1]);
        } else if (is(line, "LightDirection3")) {
            direction(line, a.lightDirection[2]);
        } else if (is(line, "SkyColour")) {
            a.skyColour = colourOf(line);
        } else if (is(line, "SkyAngle")) {
            a.skyAngle = static_cast<float>(number(line, 0) * kSkyDegrees);
        } else if (is(line, "HorizonColourFront")) {
            a.horizonFront = colourOf(line);
        } else if (is(line, "HorizonColourBack")) {
            a.horizonBack = colourOf(line);
        } else if (is(line, "SkyColourFront")) {
            a.skyFront = colourOf(line);
        } else if (is(line, "SkyColourBack")) {
            a.skyBack = colourOf(line);
        } else if (is(line, "ZenithColour")) {
            a.zenith = colourOf(line);
        } else if (is(line, "SkyHeightFactor")) {
            a.skyHeightFactor = real(line, 0);
        } else if (is(line, "SkyHeightManual")) {
            a.skyHeightManual = real(line, 0);
        } else if (is(line, "SunShape")) {
            a.sunShape = word32(line);
        } else if (is(line, "SunReflection")) {
            a.sunReflection = word32(line);
        } else if (is(line, "SunFlare")) {
            a.sunFlare = word32(line);
        } else if (is(line, "Envmap")) {
            a.envmap = word32(line);
        } else if (is(line, "SunSize")) {
            a.sunSize = real(line, 0);
        } else if (is(line, "SunFlareSize")) {
            a.sunFlareSize = real(line, 0);
        } else if (is(line, "SunReflectionSize")) {
            a.sunReflectionSize = real(line, 0);
        } else if (is(line, "SunAngle")) {
            a.sunAngle = static_cast<float>(number(line, 0) * kSkyDegrees);
        } else if (is(line, "ExposureBias")) {
            a.exposureBias = real(line, 0);
        } else if (is(line, "ExposureEnable")) {
            a.exposureEnable = line.words.empty() ? 0 : static_cast<uint32_t>(std::atol(line.words[0].c_str()));
        } else if (is(line, "ExposureLimits")) {
            a.exposureMax = real(line, 0);
            a.exposureMin = real(line, 1);
        } else if (is(line, "Night")) {
            a.night = 1;
        } else if (is(line, "SkyLuminosity")) {
            a.skyLuminosity = real(line, 0);
        } else if (is(line, "Wet")) {
            a.wet = 1;
        } else if (is(line, "LightBlur")) {
            a.lightBlur = real(line, 0);
        } else if (is(line, "ShadowSet")) {
            a.shadowSet = line.words.empty() ? 0 : static_cast<int32_t>(std::atol(line.words[0].c_str()));
        } else if (is(line, "WorldShadowColour")) {
            a.worldShadowColour = byteOf(line, 3) << 24 | byteOf(line, 0) << 16 | byteOf(line, 1) << 8 | byteOf(line, 2);
        } else if (is(line, "WorldShadowAdditive")) {
            a.worldShadowAdditive = 1;
        } else if (is(line, "TestMipmapBias")) {
            a.testMipmapBias = real(line, 0);
        } else if (is(line, "NightRoadShininess")) {
            a.nightRoadShininess = real(line, 0);
        } else if (is(line, "MinRoadWetness")) {
            a.roadWetness = a.minRoadWetness = real(line, 0);
        } else if (is(line, "MaxRoadWetness")) {
            a.maxRoadWetness = real(line, 0);
        } else if (is(line, "RoadWetnessTime")) {
            a.roadWetnessRate = static_cast<float>(1.0 / number(line, 0));
        } else if (is(line, "MinRainOnTime")) {
            // 0x00533130 .. 0x00533280: the timer of the state it is in (raining at first) is brought within the
            // times as they are read (the value as atof gives it against the timer).
            const double v = number(line, 0);
            a.minRainOnTime = static_cast<float>(v);
            if (a.raining != 0 && v > a.rainTimer) a.rainTimer = static_cast<float>(v);
        } else if (is(line, "MaxRainOnTime")) {
            const double v = number(line, 0);
            a.maxRainOnTime = static_cast<float>(v);
            if (a.raining != 0 && v < a.rainTimer) a.rainTimer = static_cast<float>(v);
        } else if (is(line, "MinRainOffTime")) {
            const double v = number(line, 0);
            a.minRainOffTime = static_cast<float>(v);
            if (a.raining == 0 && v > a.rainTimer) a.rainTimer = static_cast<float>(v);
        } else if (is(line, "MaxRainOffTime")) {
            const double v = number(line, 0);
            a.maxRainOffTime = static_cast<float>(v);
            if (a.raining == 0 && v < a.rainTimer) a.rainTimer = static_cast<float>(v);
        } else if (is(line, "PuddleColour")) {
            a.puddleColour = colourOf(line);
        } else if (is(line, "PS2BuildingGlassColour")) {
            a.ps2BuildingGlassColour = colourOf(line);
        }
    }
    return list;
}

void stepRain(Atmosphere& a, float dt, CrtRandom& random) {
    a.rainTimer = a.rainTimer - dt;
    if (a.rainTimer < 0.0f) {
        a.raining = a.raining == 0 ? 1u : 0u;
        const float u = x87::unit(random.next());
        if (a.raining != 0) {
            a.rainTimer = (a.maxRainOnTime - a.minRainOnTime) * u + a.minRainOnTime;
        } else {
            a.rainTimer = (a.maxRainOffTime - a.minRainOffTime) * u + a.minRainOffTime;
        }
    }
    if (a.raining != 0) {
        if (a.rain < 1.0f) a.rain = dt * 0.5f + a.rain;
        if (a.rain > 1.0f) a.rain = 1.0f;
    } else {
        if (a.rain > 0.0f) a.rain = a.rain - dt * 0.5f;
        if (a.rain < 0.0f) a.rain = 0.0f;
    }
    a.roadWetness = a.rain * a.roadWetnessRate * dt + a.roadWetness;
    if (a.roadWetness < a.minRoadWetness) a.roadWetness = a.minRoadWetness;
    if (a.roadWetness > a.maxRoadWetness) a.roadWetness = a.maxRoadWetness;
}

}  // namespace oj
