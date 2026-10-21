// The music's playlist window (opts_songlist).

#include "Game/Frontend/Options/SongList.h"

#include "Audio/SoundSystem.h"
#include "Core/Base/Random.h"
#include "Core/Base/X87.h"
#include "Core/Data/GameTexts.h"
#include "Core/Input/Input.h"
#include "Game/Media/Music.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace oj::frontend {

// 0x004828A0 (the record 0x00721B60): [0x0075EF28] 1 (the music previews); SONGMENU (+0x20) and TICKMENU (+0x24);
// +0x28 .. +0x38 and +0x44 cleared, +0x3C and +0x40 the settings' music volume; the game's music unpaused and its song
// resumed; both menus emptied (0x005AEEF0). With the music bank: the songs from the settings' known count (+0x218) on
// get their bit (+0x208) and the count becomes the bank's (more than 128 songs abort the game: "bitset"); the music
// object keeps the bits and the count ([0x0075EF1C] +0x24 .. +0x34, 0x00408E10) for BACK; for each song with a name
// SONGMENU gets the name (its value the song) and TICKMENU a picture item ui_no / ui_no of the same value, ui_yes /
// ui_yes for a song on; then SONGMENU "DONE" (552) and TICKMENU an item without a picture, both of the songs' count and
// named done. Each menu's first item selected (+0x13C, +0x148 0; its text element +0x184, none here).
void SongList::enter(OptionsHost& host, ui::Screen& s) {
    save::Settings& set = host.optionSettings();
    const GameData* data = host.gameData();
    Jukebox& music = jukebox();
    music.previewing = true;
    m_item = 0;
    m_fadeIn = 0;
    m_fadeBack = 0;
    m_asked = 0;
    m_playing = 0;
    m_preview = 0.0f;
    m_music = host.settingsMusic();
    m_front = m_music;
    if (music.player() != nullptr) {
        music.paused = false;
        if (music.handle != 0) music.player()->resume(music.handle);
    }
    s.clearMenu("SONGMENU");
    s.clearMenu("TICKMENU");
    if (data != nullptr) {
        const auto songs = static_cast<uint32_t>(std::min<size_t>(data->songCount, 128));
        for (uint32_t song = set.u32(save::Settings::kSongsKnown); song < songs; ++song) {
            const uint32_t at = save::Settings::kSongs + (song / 32) * 4;
            set.setU32(at, set.u32(at) | (1u << (song % 32)));
        }
        set.setU32(save::Settings::kSongsKnown, static_cast<uint32_t>(data->songCount));
        for (uint32_t k = 0; k < 5; ++k) m_bits[k] = set.u32(save::Settings::kSongs + k * 4);
        for (uint32_t song = 0; song < songs; ++song) {
            const auto value = static_cast<float>(song);
            const std::wstring name = song < data->songNames.size() ? data->songNames[song] : std::wstring();
            s.addMenuItem("SONGMENU", name, value);
            const bool on = (set.u32(save::Settings::kSongs + (song / 32) * 4) & (1u << (song % 32))) != 0;
            s.addMenuItem("TICKMENU", L"", value, on ? "ui_yes" : "ui_no", on ? "ui_yes" : "ui_no");
        }
        const auto end = static_cast<float>(songs);
        s.addCommandItem("SONGMENU", host.gameTexts().at(kTextSongsDone), end, "done");
        s.addCommandItem("TICKMENU", L"", end, "done");
    }
    s.selectMenuItem("SONGMENU", 0);
    s.selectMenuItem("TICKMENU", 0);
}

// 0x00483130: the game's music's frame (0x004090D0 with 0: no next song) and the preview (0x00482E10). UP and DOWN:
// +0x2C 0, +0x30 1, +0x34 0 (the preview fades back). SELECT on TICKMENU's highlighted item: done CLICK and opts_audio
// (mode 7); a song CLICK, its bit of the settings turned (0x00408CC0), its pictures ui_yes when it came on, else ui_no
// (0x004475D0, 0x00447590). SELECT2 on SONGMENU's highlighted item (+0x28) of a song of the bank: another song than
// +0x48 stops the music (0x00408FA0) and steps the system; +0x48 the song; fading (+0x30 or +0x2C) the preview is
// asked for (+0x34), else unless one plays (+0x38) asked for with the fade in. BACK: CLICK, opts_audio (mode 7),
// opts_playlist's chosen playlist (+0x48, +0x4C: [0x007688F8], [0x007688FC]) back to its own (+0x24, +0x28), the
// settings' song bits and count back to the music object's (0x00408E60).
void SongList::update(OptionsHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    Jukebox& music = jukebox();
    if (music.opened() && data != nullptr) music.update(data->playlist, host.gameFrameTime(), false);
    preview(host);
    if (event == input::kUp || event == input::kDown) {
        m_fadeIn = 0;
        m_fadeBack = 1;
        m_asked = 0;
    } else if (event == input::kSelect2) {
        const size_t item = s.selectedMenuItem("SONGMENU");
        m_item = item < s.menuSize("SONGMENU") ? 1 : 0;
        if (m_item != 0 && data != nullptr) {
            const auto song = static_cast<uint32_t>(x87::chop(s.menuItemValue("SONGMENU", item)));
            if (song < data->songCount) {
                if (song != m_song) {
                    music.stop(false);
                    audio::system().update();
                }
                m_song = song;
                if (m_fadeBack != 0 || m_fadeIn != 0) {
                    m_asked = 1;
                } else if (m_playing == 0) {
                    m_asked = 1;
                    m_fadeIn = 1;
                }
            }
        }
    }
    if (event == input::kSelect) {
        const size_t item = s.selectedMenuItem("TICKMENU");
        if (item >= s.menuSize("TICKMENU")) return;
        host.sound("CLICK");
        if (_stricmp(s.menuItemCommand("TICKMENU", item).c_str(), "done") == 0) {
            host.request("opts_audio", SwitchMode::Swap);
            return;
        }
        const auto song = static_cast<uint32_t>(x87::chop(s.menuItemValue("TICKMENU", item)));
        if (song >= 128) return;
        save::Settings& set = host.optionSettings();
        const uint32_t at = save::Settings::kSongs + (song / 32) * 4;
        const uint32_t bit = 1u << (song % 32);
        const bool wasOn = (set.u32(at) & bit) != 0;
        set.setU32(at, wasOn ? set.u32(at) & ~bit : set.u32(at) | bit);
        const char* picture = wasOn ? "ui_no" : "ui_yes";
        s.setMenuItemTextures("TICKMENU", item, picture, picture);
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("opts_audio", SwitchMode::Swap);
        host.restorePlaylistChoice();
        save::Settings& set = host.optionSettings();
        for (uint32_t k = 0; k < 5; ++k) set.setU32(save::Settings::kSongs + k * 4, m_bits[k]);
    }
}

// 0x00482E10, with the music on: a preview asked for (+0x34) with nothing playing and no fade back: the fade in
// (+0x2C), the system stepped, the song +0x48 played by the music object at the preview's volume (+0x44; its +0x04,
// 0x00408B20: its handle kept), +0x38 1, +0x34 0. Fading in: the front end's music (+0x3C) down by 3.15 x the frame's
// time to 0 and the preview up to the settings' volume (+0x40); both there ends it. Else, not fading back, the preview
// over (0x00409230) clears +0x38 and fades back: the front end's music up to the settings' volume, the preview down;
// below 0 the music stopped (0x00408FA0 with 0), +0x28, +0x38 and +0x44 0 and the system stepped; both there ends it.
void SongList::preview(OptionsHost& host) {
    if ((audio::system().flags() & audio::kNoMusic) != 0) return;
    Jukebox& music = jukebox();
    const float dt = host.gameFrameTime();
    constexpr float kRate = 3.15f;  // [0x006FFE18]
    const auto previewVolume = [&](float v) {
        if (music.player() != nullptr && music.handle != 0) {
            music.player()->setVolume(music.handle, v);
            music.volume = v;
            music.target = v;
        }
    };
    if (m_asked != 0 && !music.playing() && m_fadeBack == 0) {
        m_fadeIn = 1;
        audio::system().update();
        uint32_t h = 0;
        if (audio::Player* p = music.player()) {
            int how = 0;
            if (p->prepare({0, static_cast<int>(m_song)}, 0xF, 0xF, how, h) == 0) {
                p->setFrequency(h, 48000);
                p->setVolume(h, m_preview);
                p->setPlayFlags(h, 0);
                p->start(h);
            }
        }
        music.handle = h;
        m_playing = 1;
        m_asked = 0;
    }
    if (m_fadeIn == 0) {
        if (m_fadeBack == 0) {
            if (!music.playing()) {
                m_playing = 0;
                m_fadeBack = 1;
            }
            return;
        }
        if (m_front > m_music) {
            m_front = m_music;
            host.setMusicVolume(m_front, false);
        } else {
            host.setMusicVolume(m_front, false);
            m_front = dt * kRate + m_front;
        }
        if (m_preview < 0.0f) {
            music.stop(false);
            m_item = 0;
            m_playing = 0;
            m_preview = 0.0f;
            audio::system().update();
        } else {
            previewVolume(m_preview);
            m_preview = m_preview - dt * kRate;
        }
        if (m_music <= m_front && m_preview == 0.0f) m_fadeBack = 0;
        return;
    }
    if (m_front < 0.0f) {
        m_front = 0.0f;
        host.setMusicVolume(0.0f, false);
    } else {
        host.setMusicVolume(m_front, false);
        m_front = m_front - dt * kRate;
    }
    if (m_preview > m_music) {
        m_preview = m_music;
    } else {
        previewVolume(m_preview);
        m_preview = dt * kRate + m_preview;
    }
    if (m_front == 0.0f && m_preview == m_music) m_fadeIn = 0;
}

// 0x00482D80: [0x0075EF28] 0; both menus emptied; the game's music stopped (0x00408FA0 with 1: 300 ms slept before
// and after); the front end's music at the settings' volume kept (+0x40); the playlist made again from the settings
// (0x00408D20, rand()).
void SongList::leave(OptionsHost& host) {
    Jukebox& music = jukebox();
    music.previewing = false;
    music.stop(true);
    host.setMusicVolume(m_music, false);
    if (GameData* data = host.gameData()) data->shuffleMusic(host.optionSettings(), gameRandom());
}

}  // namespace oj::frontend
