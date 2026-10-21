#pragma once
// The music's playlist window (opts_songlist): the songs turned on or off, each heard as a preview.

#include "Game/Frontend/Options/OptionsHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>

namespace oj::frontend {

// The window of the record 0x00721B60 (kept from visit to visit).
class SongList {
public:
    // 0x004828A0
    void enter(OptionsHost& host, ui::Screen& s);
    // 0x00483130
    void update(OptionsHost& host, ui::Screen& s, uint32_t event);
    // 0x00482D80
    void leave(OptionsHost& host);

private:
    // 0x00482E10: the preview's fades.
    void preview(OptionsHost& host);

    int32_t m_item = 0;           // +0x28: SONGMENU's item asked for the preview
    int32_t m_fadeIn = 0;         // +0x2C: the front end's music out, the preview in
    int32_t m_fadeBack = 0;       // +0x30: the preview out, the front end's music back in
    int32_t m_asked = 0;          // +0x34: a preview to start
    int32_t m_playing = 0;        // +0x38: the preview plays
    float m_front = 0.0f;         // +0x3C: the front end's music's volume
    float m_music = 0.0f;         // +0x40: the settings' music volume
    float m_preview = 0.0f;       // +0x44: the preview's volume
    uint32_t m_song = 0;          // +0x48: the song previewed
    std::array<uint32_t, 5> m_bits{};  // the music object's +0x24 .. +0x34: the settings' song bits and count
};

}  // namespace oj::frontend
