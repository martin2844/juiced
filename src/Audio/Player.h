#pragma once
// A player of the stock game's sound library (0x44 bytes, vtable 0x006FF064, made by 0x005BEE80 through its handle
// 0x006FEAE0 / 0x006FEBE0): the banks it loaded or was given, and slots ("channels") whose handles the game keeps.
// A handle is a slot's id: id % slots is the slot; an id that is no longer the slot's is stale (4); slot 0 is never
// used (3); a slot without a set (stopped) answers 1.

#include "Audio/Bank.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace oj::audio {

// A sound's state (the player's +0x1C; state() gives it): none, stopped, paused, playing, fading out.
enum SoundState : int { kSoundNone = 0, kSoundStopped = 1, kSoundPaused = 2, kSoundPlaying = 3, kSoundFadingOut = 5 };

struct SetRef {
    int bank = -1;
    int set = -1;
};

class Player {
public:
    // 0x005BEE80: the bank of audio\...\<name>\<name>.cfg (its CHANNELS + 1 slots); no file leaves a player of no
    // bank and no slot.
    Player(const GameFiles& files, const std::filesystem::path& folder, const std::string& name);
    Player();
    ~Player();
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;
    // 0x005BA110: every player made (the list [0x0075F874]) stepped (+0xBC).
    static void updateAll();

    enum Result : int { kOk = 0, kStopped = 1, kSlotZero = 3, kStale = 4, kNoSet = 6, kNoSlot = 0xC };

    // +0x74 (0x005C0130): the first bank holding a set of the name.
    int find(const char* name, SetRef& ref) const;
    // +0x04 (0x005BF900): a slot and a voice for the set. The voice by the strategies of voiceMask (0x005BF2D0: 1 a
    // free one, 2 the quietest, 4 the oldest, tried in that order; a voice taken from a slot stops that slot and takes
    // its place); the slot by those of slotMask (0x005BF260: 1 the first not playing, 2 the set of the highest
    // priority, 4 the quietest, 8 the oldest; one other than the first stops the slot it chose). handle: the slot's
    // new id; how: the voice strategy that answered. kOk, or why not.
    int prepare(const SetRef& ref, uint32_t slotMask, uint32_t voiceMask, int& how, uint32_t& handle);
    int start(uint32_t handle);        // +0x18 (0x005BFA20)
    int restart(uint32_t handle);      // +0x1C (0x005BFB00)
    int stop(uint32_t handle);         // +0x20 (0x005BFB90)
    int pause(uint32_t handle);        // +0x24 (0x005BFC30)
    int resume(uint32_t handle);       // +0x28 (0x005BFC90)
    // +0x30 (0x005BFCF0): the voice's status (0x005C3DD0); a fading slot (5) still sounding is 5.
    int state(uint32_t handle, int& state);
    int hold(uint32_t handle);         // +0x34 (0x005BFD70): the slot's bit (+0x28) and the voice's
    int unhold(uint32_t handle);       // +0x38 (0x005BFDD0)
    void unholdAll();                  // +0x3C (0x005BFE30)
    int setVolume(uint32_t handle, float volume);    // +0x40 (0x005BFE80): now and as the target
    int valid(uint32_t handle) const;                // +0x44
    int setFrequency(uint32_t handle, uint32_t hz);  // +0x48 (0x005BFF00): 48000 plays the sample at its rate
    int frequency(uint32_t handle, uint32_t& hz);    // +0x4C
    int setPosition(uint32_t handle, uint32_t bytes);  // +0x50 / +0x54
    int setPlayFlags(uint32_t handle, uint32_t flags);  // +0x5C (0x005C00C0): DSBPLAY_LOOPING
    int setPan(uint32_t handle, float pan);          // +0xF4 (0x005C1710)
    int setUser(uint32_t handle, uint32_t value);    // +0x80
    int user(uint32_t handle, uint32_t& value);      // +0x84
    // +0x10 (0x005BF790): the master volume ([0, 1], +0x20) on every slot's voice.
    void setMasterVolume(float volume);
    float masterVolume() const { return m_master; }
    void pauseAll();     // +0xB4 (0x005C0230)
    void resumeAll();    // +0xB8 (0x005C0280)
    // +0xBC (0x005C0300), every frame: the slots' volumes go to their targets (up at once, down by 0.3 of the way a
    // frame, 0.0001 and below silence); a voice that ended frees its slot; a fading slot at silence is held.
    void update();
    // 0x005C1A90: the slot fades out (its target 0, state 5) and stops when silent.
    int fadeOut(uint32_t handle);
    // +0xE8 (0x005BD840): a bank loaded elsewhere (0x005B9430) added to the banks.
    void addBank(Bank* bank) { m_banks.push_back(bank); }
    Bank* bank(int i) const { return i >= 0 && static_cast<size_t>(i) < m_banks.size() ? m_banks[i] : nullptr; }
    size_t banks() const { return m_banks.size(); }
    uint32_t slots() const { return static_cast<uint32_t>(m_slots.size()); }
    SoundSet* setOf(const SetRef& ref) const;

    // For the manager of virtual voices (the 3D player's handle 0x006FEBE0):
    void renew(uint32_t& handle);                                  // +0xD4 (0x005BF8A0)
    void assign(uint32_t handle, SoundSet* set, uint32_t voice);   // +0xD8 / +0xDC
    int findSlot(uint32_t mask, uint32_t& id, int& how);           // +0xE4 (0x005BF260)
    // +0x100 (0x005C1930 → the set's +0x84): the voice placed at the emitter.
    void place3d(uint32_t handle, const class Emitter& emitter, float doppler);
    // +0x108 (0x005C19A0): the slot's voice set's occlusion (VoiceSet::setOcclusion, EAX only); kOk, or why not.
    int setOcclusion(uint32_t handle, float v);

    // +0xC4 (0x005BDD40 → 0x005C0580), called by nothing: with the debug pages ([0x0075F7A0]; here a line drawer,
    // none without them) a red line per slot from 1: "%02d" of its id % slots without a set, else "%02d : %1d : %12s
    // %0.3f : %0.3f" with its state, its set's name, its target volume and its volume x the master; from (25, 90),
    // each line (font height x 2 / 3) below, a second column from (380, 90) after every 24th.
    using DebugLine = std::function<void(const char* text, float x, float y, uint32_t argb)>;
    int debugSlots(const DebugLine& line, uint32_t fontHeight) const;

private:
    struct Slot {
        uint32_t id = 0;            // +0x00
        uint32_t previous = 0;      // +0x04
        SoundSet* set = nullptr;    // +0x08
        int32_t voice = -1;         // +0x0C
        float volume = 0.0f;        // +0x10
        float target = 0.0f;        // +0x14
        uint32_t started = 0;       // +0x18 (seconds)
        int state = 0;              // +0x1C: a SoundState
        uint32_t user = 0;          // +0x20
        uint32_t unused = 0;        // +0x24: cleared when played, read by nothing
        uint32_t fades = 0;         // +0x28
    };
    Slot* slotOf(uint32_t handle, int& result);
    const Slot* slotOf(uint32_t handle, int& result) const;
    void initSlots(uint32_t count);  // 0x005BF6A0
    int findVoice(const SetRef& ref, uint32_t mask, uint32_t& voice, int& how);  // 0x005BF2D0
    void freeSlot(Slot& s, uint32_t index);

    std::vector<std::unique_ptr<Bank>> m_own;
    std::vector<Bank*> m_banks;   // +0x0C .. +0x14
    std::vector<Slot> m_slots;    // +0x18, +0x1C
    float m_master = 1.0f;        // +0x20
    uint32_t m_maxId = 0xFFFFFFFF;  // +0x24
    uint32_t m_held = 0;          // +0x28
};

}  // namespace oj::audio
