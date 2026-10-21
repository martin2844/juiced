#pragma once
// The race's sounds that the race's own code asks for where the stock game calls the sound library (so that their
// rand() comes in its place): the game gives them (Game/Race/TrackView/TrackViewSounds.cpp); none without the sound.

namespace oj {

class CrtRandom;
struct RaceCar;

class RaceSounds {
public:
    virtual ~RaceSounds() = default;
    // 0x0049E334 (the AI's grid, 0x0049DBB0): gridskid played once at the car's place x 100 (0x005B9B00, its owner
    // one global, [0x0079E804]) and, made, its frequency drawn (+0x24 with 8000: rand()). True when made.
    virtual bool gridSkid(const RaceCar& car, CrtRandom& random) = 0;
    // 0x00497E8E (the AI) and 0x0041E2CF (the player): the car's horn (+0x4C0), from 1300, played or stopped.
    virtual void horn(const RaceCar& car, bool on) = 0;
    // 0x005A8860 with the race front end's manager ([G+0x3C]) while the race is not paused ([G+0x34]+0x20), at
    // [0x007187DC] (1): the cruise's warning, count and swoosh.
    virtual void raceInterface(const char* name) = 0;
    // 0x004AAD1B: in a cruise with the settings' effects volume over 0.5 (the stock's test), the game's music (with
    // its handle) at once at the settings' music volume x the cruise's (+0x400): the player's, +0x18 and +0x1C.
    virtual void cruiseMusic(float volume) = 0;
    // The race's speech object ([G+0x4C]): whether there is one, whether it speaks (0x0040B5A0: a line waiting, or
    // its handle neither stopped nor ended), and its lines queued with the sounds ducked (+0x14 0x0040B7B0, +0x18
    // 0x0040B7F0).
    virtual bool hasSpeech() = 0;
    virtual bool speaking() = 0;
    virtual void say(int a, int b) = 0;
    virtual void say2(int a, int b) = 0;
};

// The game's (none: no sounds).
RaceSounds*& raceSounds();

// The speech object's stop (0x0040B600) as code of the career asks it (the phone made again, 0x004B5FD0): the game's
// (none: no speech).
void (*&speechStop())();

}  // namespace oj
