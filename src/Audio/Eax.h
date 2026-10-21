#pragma once
// The EAX 4.0 property sets the stock sound library uses through IKsPropertySet (docs/spec/26-sound.md "EAX"): the
// public identifiers of Creative's EAX 4.0 interface, as the game's table 0x006CC778 .. 0x006CC808 holds them.

#include <guiddef.h>

#include <cstdint>

namespace oj::audio::eax {

inline constexpr GUID kNull{};  // 0x006CC778: EAX_NULL_GUID
// 0x006CC798: EAXPROPERTYID_EAX40_Context
inline constexpr GUID kContext{0x1D4870AD, 0x0DEF, 0x43C0, {0xA4, 0x0C, 0x52, 0x36, 0x32, 0x29, 0x63, 0x42}};
// 0x006CC7A8 .. 0x006CC7D8: EAXPROPERTYID_EAX40_FXSlot0 .. FXSlot3
inline constexpr GUID kFxSlot0{0xC4D79F1E, 0xF1AC, 0x436B, {0xA8, 0x1D, 0xA7, 0x38, 0xE7, 0x04, 0x54, 0x69}};
inline constexpr GUID kFxSlot1{0x08C00E96, 0x74BE, 0x4491, {0x93, 0xAA, 0xE8, 0xAD, 0x35, 0xA4, 0x91, 0x17}};
inline constexpr GUID kFxSlot2{0x1D433B88, 0xF0F6, 0x4637, {0x91, 0x9F, 0x60, 0xE7, 0xE0, 0x6B, 0x5E, 0xDD}};
inline constexpr GUID kFxSlot3{0xEFFF08EA, 0xC7D8, 0x44AB, {0x93, 0xAD, 0x6D, 0xBD, 0x5F, 0x91, 0x00, 0x64}};
// 0x006CC7E8: EAXPROPERTYID_EAX40_Source
inline constexpr GUID kSource{0x1B86B823, 0x22DF, 0x4EAE, {0x8B, 0x3C, 0x12, 0x78, 0xCE, 0x54, 0x42, 0x27}};
// 0x006CC7F8: EAX_REVERB_EFFECT
inline constexpr GUID kReverbEffect{0x0CF95C8F, 0xA3CC, 0x4849, {0xB0, 0xB6, 0x83, 0x2E, 0xCC, 0x18, 0x22, 0xDF}};
// 0x006CC908: an interface of the device eax.dll makes; its method +0x10 gives the device's EAX flags (the system's
// +0x0C).
inline constexpr GUID kDeviceFlags{0xFA82931B, 0xDE98, 0x4A68, {0xA7, 0xFE, 0xD6, 0x83, 0xE3, 0x8A, 0x65, 0x02}};

// The property ids of those sets the library sets or reads.
enum : uint32_t {
    kContextAllParameters = 1,      // EAXCONTEXT_ALLPARAMETERS
    kContextPrimaryFxSlotId = 2,    // EAXCONTEXT_PRIMARYFXSLOTID
    kFxSlotAllParameters = 0x10001, // EAXFXSLOT_ALLPARAMETERS: {guidLoadEffect, lVolume, lLock, ulFlags}, 0x1C bytes
    kFxSlotLoadEffect = 0x10002,    // EAXFXSLOT_LOADEFFECT
    kReverbEnvironment = 2,         // EAXREVERB_ENVIRONMENT of the effect in the slot
    kReverbRoom = 5,                // EAXREVERB_ROOM
    kSourceAllParameters = 1,       // EAXSOURCE_ALLPARAMETERS
    kSourceRoom = 7,                // EAXSOURCE_ROOM
    kSourceActiveFxSlotId = 0x1B,   // EAXSOURCE_ACTIVEFXSLOTID: two slot GUIDs, 0x20 bytes
};

// EAXFXSLOT_ALLPARAMETERS' data.
struct FxSlotProperties {
    GUID loadEffect;
    int32_t volume;
    int32_t lock;
    uint32_t flags;
};

}  // namespace oj::audio::eax
