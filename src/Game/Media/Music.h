#pragma once
// The game's music ([game + 0x48], 0x00408B60, vtable 0x006EE8DC, docs/spec/26-sound.md "The music"): a player of the
// stream bank audio\music (its songs pieces of music.dsb), the playlist of the songs the settings allow
// (GameData::playlist, 0x00408D20), the song playing and its volume going to a target.

#include "Audio/Player.h"
#include "Audio/Voices.h"
#include "Core/Base/GameFiles.h"
#include "Core/Saves/SaveEntries.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace oj {

class Jukebox {
public:
    // 0x0040D200, once ([0x0079E78C]): the player of audio\music (0x0040B150: kind 0, all its slots resumed).
    void open(const GameFiles& files);
    bool opened() const { return m_player != nullptr; }
    // 0x00483720's part: the object freed (0x00408C80: its player and its song with it) and made again (0x00408B60:
    // the player of audio\music, no song, the volumes 0, not paused).
    void reopen(const GameFiles& files);

    // 0x00408E90: the next song of the playlist (none: -1).
    void next(const std::vector<uint32_t>& playlist);
    // 0x00408EC0: with the music on, the next song when none is chosen or force asks; then, not playing, the song
    // started (0x0040B330: bank 0, at the volume +0x18, 48000, not looping, masks 0xF), the system stepped (1.0), with
    // wait half a second slept (Sleep(500)), and the system stepped again.
    void play(const std::vector<uint32_t>& playlist, bool wait, bool force);
    // 0x00408FA0: the song stopped (the system stepped, with wait 300 ms slept before and after); the game's music
    // unpaused and resumed.
    void stop(bool wait);
    // 0x004090D0, each frame: with the music on and a song not paused: unless a preview runs ([0x0075EF28]) a song
    // silent with a target of 0 stops; the volume goes to the target by (target - volume) / the frame's time / 500
    // in [0.001, 1], stopping at 0.001 going down; a song that ended, with next, gives the next.
    void update(const std::vector<uint32_t>& playlist, float frameTime, bool next);
    // 0x00409230: the song is playing (or paused, or looping).
    bool playing();

    float volume = 0.0f;        // +0x18
    float target = 0.0f;        // +0x1C
    bool paused = false;        // +0x58
    bool previewing = false;    // [0x0075EF28]
    int32_t song = -1;          // +0x20
    uint32_t index = 0xFFFFFFFF;  // +0x14: -1, the first next() takes the playlist's first
    uint32_t handle = 0;        // +0x10
    audio::Player* player() const { return m_player.get(); }

private:
    std::unique_ptr<audio::Player> m_player;
};

// [game + 0x48]: the one the game makes (0x0040D200).
Jukebox& jukebox();

// The game object's other sound objects the windows reach: the race's 3D world ([G+0x50], while a race runs) and the
// speech's player ([G+0x4C] + 0x08).
class Speech;
struct GameSounds {
    audio::World* world = nullptr;
    Speech* speech = nullptr;
    bool raceFrontEnd = false;  // [G+0x3C] with its manager: the race's front end is up
    // The driver's settings' volumes ([G+0x64]+0x54 .. +0x5C: music, effects, speech); a new driver's without one.
    std::function<float(uint32_t offset)> setting;
    float music() const { return setting ? setting(save::Settings::kMusicVolume) : 0.0f; }
    float effects() const { return setting ? setting(save::Settings::kEffectsVolume) : 0.0f; }
    float speechVolume() const { return setting ? setting(save::Settings::kSpeechVolume) : 0.0f; }
};
GameSounds& gameSounds();

// A speech object ([G+0x4C], 0x2C bytes; 0x0040B150 on the vtable 0x006EE9F4: +0x04 the system, +0x08 a player of
// audio\speech\<name>, +0x0C paused; 0x0040BA40 the interface's (vtable 0x006EEA0C) or 0x0040BA80 the race's
// (0x006EEA34): +0x10 .. +0x18 the set to say (bank, set, volume), +0x1C its delay, +0x20 waiting, +0x24 the 3D
// world's volume left alone, +0x28 the handle saying).
class Speech {
public:
    enum Kind { kInterface = 1, kRace = 2 };
    Speech(const GameFiles& files, const std::string& name, Kind kind);
    ~Speech();
    Speech(const Speech&) = delete;
    Speech& operator=(const Speech&) = delete;

    // +0x04 (0x0040B330): with the player and not paused: the set prepared (masks), its frequency, volume and play
    // flags, started. The handle, else 0.
    uint32_t play(const audio::SetRef& ref, float volume, uint32_t hz, uint32_t flags, uint32_t slotMask,
                  uint32_t voiceMask);
    // +0x08 (0x0040B2B0): the set of the name found (+0x74), then +0x04 with the voice mask 0xF.
    uint32_t play(const char* name, float volume, uint32_t hz, uint32_t flags, uint32_t slotMask);
    // +0x14: the interface's 0x0040B640 (at once: a line saying stopped first; the set (group - 1) x 33 + index) or
    // the race's 0x0040B7B0 (waiting: (group - 1) x 16 + index, the sounds made quieter 0x0040B420).
    void say(int group, int index);
    // +0x18: the interface's 0x0040B750 (at once: 6 a + 33 x 8 + b) or the race's 0x0040B7F0 (waiting: 35 a +
    // ([0x0075EC90] x [0x0075EC8C] (both never set) + 16) x 8 + b).
    void say2(int a, int b);
    // +0x1C (the interface's 0x0040B6D0): the set of the name said at once.
    void sayName(const char* name);
    // 0x0040B600: a line saying stopped (the system stepped), nothing waiting.
    void stop();
    // 0x0040B5A0: a line waiting, or its handle not stopped nor ended (states 0, 1); else the handle 0.
    bool speaking();
    // 0x0040B420: with the settings' speech volume over 0, the 3D world (down 20, up 50) to 0.5 x the effects' and
    // its player's master at its volume, the music to 0.5 x its own; the line waits 0.3 s.
    void duck();
    // 0x0040B4D0, each frame: a line waiting is said once its delay passes; else not speaking, the 3D world (unless
    // +0x24) back to 1 x the effects' (down 20, up 50, the master at its volume) and, with the race's front end, the
    // music to its own.
    void update(float dt);

    audio::Player* player() const { return m_player.get(); }
    Kind kind;
    bool paused = false;          // +0x0C
    audio::SetRef pending{0, 0};  // +0x10, +0x14
    float pendingVolume = 0.0f;   // +0x18
    float delay = 0.0f;           // +0x1C
    bool waiting = false;         // +0x20
    bool holdWorld = false;       // +0x24
    uint32_t handle = 0;          // +0x28

private:
    std::unique_ptr<audio::Player> m_player;
};

// 0x0040D330 (none with the configuration's flag 0x10): the speech object freed, then of the kind (1 the interface's
// "ui<letter>", 2 the race's "race<letter>", the letter of "efgiscpr" by the language, 0 none) made with the
// settings' speech volume as its player's master.
void makeSpeech(int kind, int language, const GameFiles& files);

// 0x005B9260 by a window of the front end (CMSCutScene 0x004C6B80, car_rolling 0x004DD490): the 3D world of audio\3d
// with 256 voices made [G+0x50] (the one before released), at the effects' volume (now and as its target, down 20,
// up 50, its player's master), its voices on. car_rolling's leave frees it (0x004DDF90); the race's start replaces it.
audio::World* makeWindowWorld();
void freeWindowWorld();
// 0x005B9430 for the window's world: audio\cars\<name> added unless a bank of the name is.
void addWindowBank(const std::string& name);

class UiSound;
// 0x0040D8E0: with the sound system ([G+0x44]) the game's sounds paused or resumed: the speech's +0x0C and its
// player's slots, the 3D world's voices (its manager's +0x20), the system stepped, the music's +0x58 and its song, the
// system stepped again; then the interface manager's player (the front end's, else the race front end's).
void pauseGameSounds(bool paused, UiSound* ui);

}  // namespace oj
