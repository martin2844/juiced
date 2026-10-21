#pragma once
// The race's replay (the game object's +0x9C, 0xD14 bytes made once by 0x00428970; docs/spec/24-replay.md): every
// quarter of a second (0.14 s in a cruise) each car's frame is packed into 33 bytes of a ring of 48 blocks of 240;
// played back the frames are read ahead of the time, blended between and set on the cars, with a fade where the
// ring left a gap.

#include "Core/Cars/Car.h"
#include "Core/Race/RaceKinds.h"
#include "Core/Base/Matrix.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace oj {

struct RaceCar;
namespace save {
struct SaveSource;
}

// A car's frame (0x5C bytes unpacked; packed by 0x004283E0 into 33 bytes, unpacked by 0x004286E0).
struct ReplayFrame {
    float time = 0.0f;                   // +0x00: the replay's time
    int32_t car = 0;                     // +0x04: in the cars' manager's list (3 bits)
    Quat orientation{};                  // +0x08: the body's +0x148 (each part as 16 bits of -1 .. 1)
    Vec4 position{};                     // +0x18: the body's +0x138 (w not packed)
    std::array<float, 4> wheels{};       // +0x28: the wheels' spins (each a byte of -100 .. 100)
    int32_t nitro = 0;                   // +0x38: the car's +0x190
    int32_t brake = 0;                   // +0x3C: the body's brake over 0.1
    uint32_t gear = 0;                   // +0x40: 4 bits
    float steering = 0.0f;               // +0x44: 5 bits of -1 .. 1
    float rpm = 0.0f;                    // +0x48: the engine's speed (rpm, a byte of 0 .. 10000)
    float throttle = 0.0f;               // +0x4C: 2 bits of 0 .. 1
    uint8_t parts = 0;                   // +0x50: the model's +0x170 (6 bits)
    int32_t horn = 0;                    // +0x54: the car's horn (+0x4C0) sounding
    float engineSound = 0.5f;            // +0x58: the car's engine sound's +0x30 (7 bits of 0 .. 1)
};

using PackedFrame = std::array<uint8_t, 33>;

// 0x004283E0 / 0x004286E0.
PackedFrame packFrame(const ReplayFrame& f);
ReplayFrame unpackFrame(const uint8_t* p);

// 0x00428010: the car's frame as it is (its time and number left to the caller).
ReplayFrame captureFrame(const RaceCar& car);

// 0x00428180: a frame set on a car; at the normal speed (`blend`) the body goes 0.7 of the way to its place and turn.
void applyFrame(RaceCar& car, const ReplayFrame& f, bool blend, bool replayRace);

// The header the replay's files keep (100 bytes at +0x7BC; 0x00426870 writes it at the record's start, 0x00426F60
// completes it at its end, 0x004269A0 reads it back).
struct ReplayHeader {
    uint32_t lastBlock = 0;     // +0x7BC: the block of the last frames (+0x14)
    uint32_t lastFrames = 0;    // +0x7C0: the frames written in it (+0x20)
    float length = 0.0f;        // +0x7C4: the replay's time at its end (+0x50)
    uint32_t bytes = 0;         // +0x7C8: the blocks' bytes kept
    std::string track;          // +0x7CC (32 bytes): the race's track ([G+0x6C]+0x10)
    std::string route;          // +0x7EC (32 bytes): its route ([G+0x6C]+0x30)
    uint32_t timeOfDay = 0;     // +0x80C: [G+0x6C]+0x6C
    uint32_t weather = 0;       // +0x810: [G+0x6C]+0x70
    uint8_t cars = 0;           // +0x814
    uint8_t mode = 0;           // +0x815: [G+0x6C]+0x00
    uint8_t type = 0;           // +0x816: [G+0x6C]+0x04
    uint32_t carsBytes = 0;     // +0x818: the cars' stream's size
    uint32_t laps = 0;          // +0x81C: [G+0x6C]+0x50

    static constexpr size_t kSize = 100;
    std::array<uint8_t, kSize> write() const;
    static ReplayHeader read(const uint8_t* p);
};

// The replay's state (its +0x820).
enum class ReplayState : int32_t { Recording = 0, Playing = 1, Stopped = 2 };

// What the replay reads of the race each step.
struct ReplayRace {
    std::vector<RaceCar*> cars;   // the cars' manager's list (+0x848)
    RaceMode mode = RaceMode::Arcade;    // [G+0x6C]+0x00
    RaceType type = RaceType::TimedLap;  // [G+0x6C]+0x04
    bool allFinished = false;     // the race state's finishers as many as its competitors
    float cruiseTime = 0.0f;      // the cruise manager's time left (+0x18)
};

class Replay {
public:
    static constexpr uint32_t kBlocks = 48;      // +0x0C
    static constexpr uint32_t kFrames = 240;     // in a block
    static constexpr size_t kBlockSize = 0x1EFC;  // +0x00 its number, +0x04 the next block, +0x08 the marks' interest,
                                                  // +0x0C the frames

    Replay();

    // 0x00426B00: ready to record, the fade out.
    void reset();
    // 0x00426EA0 (off the network): the record begins: everything cleared, the buffer emptied, the header taken from
    // the race; the period 0.14 s in a cruise, else 0.25 s.
    void begin(const ReplayRace& race, const std::string& track, const std::string& route, uint32_t timeOfDay,
               uint32_t weather, uint32_t laps);
    // 0x004270C0 (not in type 3, off the network): the record's or the play's step of the race's frame. True when the
    // play's first frame wants the camera and the racers' places anew (0x00401F20, 0x00497BB0); `pause` set when the
    // finished play wants the race paused (0x004263E0 with 1).
    bool step(const ReplayRace& race, float dt, bool& pause);
    // 0x00426F60: the record ends: the last block and frame kept, the blocks put in order; false when it was not
    // recording.
    bool stop();
    // 0x00426FE0: the play begins from the first frame (false for an empty replay while its file was not read); the
    // race becomes a replay's ([G+0x6C]+0x68).
    bool play();
    // 0x00426B80: play on (or from the start once it ended), at the normal speed.
    void resume();
    // 0x00426C40: from the start.
    void restart();
    // 0x00426CF0: fast forward toggled (`speed` kept in +0x80); true when it turned on (the camera's views anew).
    bool fastForward(float speed);
    // 0x00426DF0: slow motion (a quarter of the speed while it plays).
    void slow();
    // 0x004270A0: the part played, at most 1.
    float progress() const;
    // 0x0044DA60: playing and finished.
    bool done() const { return playing != 0 && state == ReplayState::Stopped; }

    // 0x00428AD0: the blocks in the order they were written (unused ones last), each linked to the next.
    void sortBlocks();
    // 0x00429260: the bytes of the blocks written.
    uint32_t bytes() const;
    // 0x00426A50: the cars of the stream read back (0x0048E020 each, at most the header's count); false when one
    // does not read. `used`: the stream's bytes they took.
    bool readCars(size_t* used = nullptr);
    // 0x00429720 with the files read (<name>.hdr and <name>.dat): the header and its cars (0x004269A0), the blocks,
    // from a file (+0x78), ready to play from the start (0x00426FE0). False when a file is short.
    bool load(const std::vector<uint8_t>& hdr, const std::vector<uint8_t>& dat);

    // Slot 13 (+0x34, 0x004292C0), what the replays' save device's object does with a write's source (0x004FA600):
    // the files USERDATA (0x004288B0, 0x00429A10: u32 the cars, u32 the race's type, char[32] the track, char[32] the
    // route, char[17] the date's text, wchar_t[16] the driver; 0x79 bytes), replay.dat (the blocks written, 48 at
    // most: their bytes kept in the header's +0x7C8) and replay.hdr (the header with the cars' stream's size in
    // +0x818, the stream after it). True with replay.hdr made.
    bool saveTo(save::SaveSource& source, const std::string& date, const std::wstring& driver);
    // Slot 14 (+0x38, 0x00429550), what the replays' save device's object does with a read's source (0x004FA810):
    // saved (+0xD10); replay.hdr's header (100 bytes; a shorter file leaves the rest as it was) and the cars after it
    // (0x00426A50; the bytes they took copied into the cars' stream, 0x004FB630); replay.dat's blocks (the header's
    // bytes); the replay's part of the header taken (0x004269A0: the cars, the length, the last block and its frames;
    // the race's part to the caller), not from a file, the blocks written the header's last block (Stock problem: one
    // less than the blocks there, none for a replay in block 0: saved again it leaves its last block out), the play
    // from the start (0x00426FE0). False without replay.hdr (nothing more read) or replay.dat (the header and the cars
    // read).
    bool loadFrom(const save::SaveSource& source);

    ReplayHeader header;
    std::vector<uint8_t> buffer;   // +0xD0C ([0x0075ECE0]): 48 blocks
    uint32_t written = 0;          // +0x08: blocks
    uint32_t block = 0;            // +0x10: written to or read from
    uint32_t last = 0;             // +0x14
    uint32_t sequence = 0;         // +0x18: the next block's number
    uint32_t frame = 0;            // +0x1C: in the block
    uint32_t lastFrames = 0;       // +0x20
    float recordTimer = 0.0f;      // +0x28
    float period = 0.25f;          // +0x2C
    float timeScale = 1.0f;        // +0x30: the game's clock while it plays
    float time = 0.0f;             // +0x34
    float readTime = 0.0f;         // +0x38: of the newest frame read
    float fadeWait = 0.0f;         // +0x3C: until the fade out
    float fadeTime = 0.0f;         // +0x40
    float fadeLength = 0.0f;       // +0x44: at most 1.4 s
    float endWait = 0.0f;          // +0x48: the record goes on after the finish
    float nextTime = 0.0f;         // +0x4C: the time after the gap
    float length = 0.0f;           // +0x50
    uint32_t cars = 0;             // +0x54
    int32_t fadeDue = 0;           // +0x58
    int32_t fadingOut = 0;         // +0x5C
    int32_t fadingIn = 0;          // +0x60
    int32_t firstRead = 0;         // +0x64
    int32_t finishing = 0;         // +0x68: the finish seen, the record ends after +0x48
    int32_t ended = 1;             // +0x6C: the play read its last frame
    int32_t blockOpen = 0;         // +0x70
    int32_t enabled = 1;           // +0x74
    int32_t loaded = 0;            // +0x78: from a file
    int32_t fast = 0;              // +0x7C
    float fastSpeed = 1.0f;        // +0x80
    std::array<ReplayFrame, 6> previous{};  // +0x084
    std::array<ReplayFrame, 6> next{};      // +0x2AC
    std::array<Vec4, 6> before{};           // +0x4D4: the place before the previous frame
    std::array<Vec4, 6> after{};            // +0x534: the place after the next one
    std::array<ReplayFrame, 6> shown{};     // +0x594
    ReplayState state = ReplayState::Stopped;  // +0x820
    int32_t restartAsked = 0;      // +0x824
    int32_t playing = 0;           // +0x828: the replay plays
    // [G+0x6C]+0x68: the race is a replay's (set by the play's start and start_replay, cleared as a record begins).
    int32_t replayRace = 0;
    std::array<uint8_t, 0x18> marks{};  // +0xCB4: a cruise's frame of the showoff's lines
    int32_t saved = 0;             // +0xD10
    std::vector<uint8_t> carsStream;  // +0x82C: the cars as the record began (0x0048DD40 each)
    std::vector<Car> savedCars;       // +0x834: read back from it
    // [0x007209FC] (the hud_replay window's +0x0C): the fade to black, 0 .. 1.
    float fade = 0.0f;
    // The marks' interest of the frames since the last block began (the race manager's +0x1C: +0x00 summed by
    // 0x00429F50, +0x04 its lines).
    float interest = 0.0f;
    int32_t interestLines = 0;

private:
    uint8_t* blockAt(uint32_t b) { return buffer.data() + static_cast<size_t>(b) * kBlockSize; }
    const uint8_t* blockAt(uint32_t b) const { return buffer.data() + static_cast<size_t>(b) * kBlockSize; }
    uint32_t blockNumber(uint32_t b) const;
    uint32_t blockNext(uint32_t b) const;
    float frameTime(uint32_t b, uint32_t f) const;

    void record(const ReplayRace& race);           // 0x00427370
    void playFrames(const ReplayRace& race);       // 0x00427560
    ReplayFrame blend(size_t car, float t);        // 0x00427860
    bool read(uint8_t* out, uint32_t ahead) const;  // 0x00428B70
    bool readFrame(ReplayFrame& f, uint32_t ahead) const;  // [3] 0x00428C10
    bool readRaw(uint8_t* out, size_t size, uint32_t ahead) const;  // [2] 0x00428C60
    void advance();                                 // [1] 0x00428CD0
    void write(const uint8_t* bytes);               // 0x00428FE0
    uint32_t nextBlock();                           // 0x00429210
    void clear();                                   // [8] 0x00429290
};

}  // namespace oj
