#pragma once
// The interface manager's sounds (0x005A5F90 with a bank, docs/spec/26-sound.md "The interface's sounds"): +0xA8 the
// sound system, +0xAC a player of audio\ui\<bank>, +0xB0 the frame each set last played, +0xB4 the effects' volume
// (0.5; the race front end gives it the settings' effects volume), +0xB8 the interface's frames, +0x104 the frame's
// time (0x005A6AB0, at most 0.1 s).

#include "Audio/Player.h"
#include "Core/Base/GameFiles.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace oj {

class UiSound {
public:
    // audio\ui\<bank> (0x005B9B70) when the sound is on; the times one a set of the last bank.
    void open(const GameFiles& files, const std::string& bank);
    // 0x005A6AB0: a frame of the interface.
    void frame(float dt);
    // 0x005A8860: the set of the name (none: 0); the same set again within 0.05 s (the first set: within the frame)
    // plays nothing (0). Prepared with mask for both its slot and its voice, at 48000 (the sample's rate), at volume
    // times the effects' volume, not looping, started. The handle, or 0.
    uint32_t play(const char* name, float volume = 1.0f, uint32_t mask = 0xF);

    // 0x005A89F0: the effects' volume, clamped to [0, 1].
    void setVolume(float v) { volume = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    // 0x005A8980: a slot and a voice for the set of the name (masks 0xF); 1 without the set, else the player's
    // answer; handle 0 when none.
    int prepare(const char* name, uint32_t& handle);
    // 0x0046BAC0's load: the bank audio/ui/<name> added to the player unless a bank of the name is loaded.
    void addBank(const GameFiles& files, const std::string& name);

    float volume = 0.5f;  // +0xB4
    audio::Player* player() const { return m_player.get(); }

private:
    std::unique_ptr<audio::Player> m_player;
    std::vector<std::unique_ptr<audio::Bank>> m_banks;  // banks added (the list [0x0075F87C] keeps them)
    std::vector<uint32_t> m_last;  // +0xB0
    uint32_t m_frames = 0;         // +0xB8
    float m_frameTime = 0.0f;      // +0x104
};

}  // namespace oj
