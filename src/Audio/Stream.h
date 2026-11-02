#pragma once
// A stream of the stock game's sound library (0x138 bytes, vtable 0x006FEA04, made by 0x005BB1F0 for a stream bank):
// the output's stream (Audio/Output.h; the stock's DirectShow) plays <bank>.dsb (one Windows Media Audio file; the
// bank's sets are pieces of it, their start and length in its .dat). Every stream is kept in a list ([0x0079E714])
// paused while the game is not the active application.

#include "Audio/Output.h"

#include <cstdint>
#include <filesystem>
#include <memory>

namespace oj::audio {

class Stream {
public:
    // 0x005BB290 → 0x005BB300: the output's stream of the file (the filter graph, its control, seeking and audio; the
    // file added as a source, 0x005BB380, its pin rendered, media time, at 0). State 1.
    explicit Stream(const std::filesystem::path& file);
    ~Stream();
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

    void stop();                         // +0x04 (0x005BB5A0): state 1
    // +0x08 (0x005BB660): from start for length (seconds): the positions in 100 ns units as the stock reckons them
    // (round(10000 start) x 1000, and the stop round(10000.0 length) added), then run. State 3.
    void play(float start, float length);
    void setVolume(int32_t mb);          // +0x0C (0x005BB640): kept (+0x12C) and given
    // +0x10 (0x005BB5C0): 1 silences and stops (state 2); 0 runs again at the kept volume while the game is active.
    void pause(int paused);
    // +0x14 (0x005BB510): 1 once the position reached the stop asked for (or the end).
    int state();

    // 0x005BB140, as the game becomes active or not ([0x0075F780]): playing streams are paused (+0x130), those
    // paused so are played again.
    static void focus(bool active);

private:
    std::unique_ptr<OutputStream> m_out;  // +0x04 .. +0x10: the graph, its audio, control and seeking
    int64_t m_stop = 0;                   // +0x120
    bool m_stopAsked = false;             // +0x124
    int m_state = 1;                      // +0x128: 1 stopped, 2 paused, 3 playing
    int32_t m_volume = 0;                 // +0x12C
    bool m_focusPaused = false;           // +0x130
};

}  // namespace oj::audio
