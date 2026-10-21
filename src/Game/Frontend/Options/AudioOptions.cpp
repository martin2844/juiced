// The audio options (opts_audio).

#include "Game/Frontend/Options/AudioOptions.h"

#include "Audio/Voices.h"
#include "Core/Input/Input.h"
#include "Game/Media/Music.h"

#include <bit>
#include <cmath>
#include <cstring>
#include <utility>

namespace oj::frontend {

// opts_audio (record 0x00721A90; from a race's pause opts_ingame_audio), enter 0x00480960: the meters MUSIC, SFX and
// SPEECH (+0x0C .. +0x14) made sliders (+0xFC 1) not taking the direction (+0xF8 0) at the settings' music, effects and
// speech volumes (the save system's +0x54, +0x58, +0x5C; their value and shown value), kept for BACK with the speaker
// set (+0x60 .. +0x68, +0x58 its +0x60); the speakers' ticks (DOLBY/STEREO/MONO TICK and TITLE, 0x0047FD50: none in
// the windows); +0x4C, +0x50, +0x54 the volumes heard; +0x18 none.
void AudioOptions::enter(OptionsHost& host, ui::Screen& s) {
    save::Settings& set = host.optionSettings();
    m_music = std::bit_cast<float>(set.u32(save::Settings::kMusicVolume));
    m_sfx = std::bit_cast<float>(set.u32(save::Settings::kEffectsVolume));
    m_speech = std::bit_cast<float>(set.u32(save::Settings::kSpeechVolume));
    m_speakerKept = set.u32(save::Settings::kSpeakers);
    for (const auto& [meter, value] : {std::pair{"MUSIC", m_music}, std::pair{"SFX", m_sfx},
                                       std::pair{"SPEECH", m_speech}}) {
        if (s.find(meter) < 0) continue;
        s.setMeterSlider(meter, true);
        s.setMeterActive(meter, false);
        s.setMeter(meter, value, true);
    }
    m_sfxHeard = m_sfx;
    m_speaker = m_speakerKept;
    m_shownButton.clear();
    m_speechHeard = m_speech;
    m_musicHeard = m_music;
}

// 0x00480AD0, for each player (none with the race's players at 0). Every frame on MUSIC_BUT the music is unpaused
// and its song resumed. A highlighted button other than +0x18 becomes it: on MUSIC_BUT the MUSIC meter takes the
// direction (not with [0x0071E718] & 8) and in a race's pause ([G+0x3C]) the music unpaused, resumed, at the volume
// kept (+0x54) and played (0x00408EC0); on SFX_BUT SFX, on SPEECH_BUT SPEECH (not with & 0x10), the others not, on
// the speakers' titles, playlist and DON_BUT none: these in a race's pause pause the music and its song. Each frame
// the settings take the meters' shown values (+0x54, +0x58, +0x5C) and the speaker set (+0x60); in the front end
// ([G+0x38]) its music takes the music's volume (and the target), the effects' volume (0x005A89F0) and the speech's
// player's master; in a race's pause the game's music takes it (played again when over 0.001), the effects' and the
// speech's likewise; the race's 3D player its master at 1 x the effects'. The effects moved over 0.07 play "Warning";
// the speech over 0.01 "SPEECH" at the speech's volume (the effects' then given back); the music's kept over 0.07.
// BACK: the music unpaused and resumed in the front end, paused in a race's pause; the kept values back with their
// volumes as above (the music not played, the 3D world not), CLICK and the options' menu (mode 7). SELECT on DON_BUT:
// CLICK and the options' menu; on playlist opts_songlist (CLICK; NO with [0x0071E718] & 8); else a speaker's title
// (0x0047FDB0).
void AudioOptions::update(OptionsHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    if (data == nullptr) return;
    // Each player in turn with its event (the window's +0x88, +0x8C); a window asked for ends the frame.
    for (int32_t player = 0; player < data->race.players && player < 2; ++player) {
        if (updatePlayer(host, s, player == 0 ? event : host.secondPlayerEvent())) return;
    }
}

bool AudioOptions::updatePlayer(OptionsHost& host, ui::Screen& s, uint32_t event) {
    GameData& data = *host.gameData();
    save::Settings& set = host.optionSettings();
    const uint32_t flags = data.audioFlags;
    const std::string current = s.currentButton();
    const auto is1 = [&](const char* name) { return _stricmp(current.c_str(), name) == 0; };
    Jukebox& music = jukebox();
    const auto pauseMusic = [&](bool paused) {
        if (music.player() == nullptr) return;
        music.paused = paused;
        if (music.handle == 0) return;
        if (paused) {
            music.player()->pause(music.handle);
        } else {
            music.player()->resume(music.handle);
        }
    };
    // The game's music at a volume (+0x18, +0x1C) with its player and handle.
    const auto musicVolume = [&](float v) {
        if (music.player() == nullptr || music.handle == 0) return;
        music.player()->setVolume(music.handle, v);
        music.volume = v;
        music.target = v;
    };
    if (is1("MUSIC_BUT")) pauseMusic(false);
    if (m_shownButton != current) {
        m_shownButton = current;
        if (is1("MUSIC_BUT")) {
            s.setMeterActive("MUSIC", (flags & 8) == 0);
            s.setMeterActive("SFX", false);
            s.setMeterActive("SPEECH", false);
            if (host.racePause()) {
                pauseMusic(false);
                musicVolume(m_musicHeard);
                music.play(data.playlist, false, false);
            }
        } else if (is1("SFX_BUT")) {
            s.setMeterActive("MUSIC", false);
            s.setMeterActive("SFX", true);
            s.setMeterActive("SPEECH", false);
            if (host.racePause()) pauseMusic(true);
        } else if (is1("SPEECH_BUT")) {
            s.setMeterActive("MUSIC", false);
            s.setMeterActive("SFX", false);
            s.setMeterActive("SPEECH", (flags & 0x10) == 0);
            if (host.racePause()) pauseMusic(true);
        } else if (is1("DOLBYTITLE") || is1("STEREOTITLE") || is1("MONOTITLE") || is1("playlist") ||
                   is1("DON_BUT")) {
            s.setMeterActive("MUSIC", false);
            s.setMeterActive("SFX", false);
            s.setMeterActive("SPEECH", false);
            if (host.racePause()) pauseMusic(true);
        }
    }
    const float musicShown = s.meterShown("MUSIC");
    const float sfx = s.meterShown("SFX");
    const float speech = s.meterShown("SPEECH");
    set.setU32(save::Settings::kMusicVolume, std::bit_cast<uint32_t>(musicShown));
    set.setU32(save::Settings::kEffectsVolume, std::bit_cast<uint32_t>(sfx));
    set.setU32(save::Settings::kSpeechVolume, std::bit_cast<uint32_t>(speech));
    set.setU32(save::Settings::kSpeakers, m_speaker);
    audio::Player* speechPlayer = gameSounds().speech != nullptr ? gameSounds().speech->player() : nullptr;
    if (!host.racePause()) {
        host.setMusicVolume(musicShown, false);
        host.setEffectsVolume(sfx);
        if (speechPlayer != nullptr) speechPlayer->setMasterVolume(speech);
    } else {
        musicVolume(musicShown);
        if (0.001f < musicShown) music.play(data.playlist, false, false);  // [0x0071169C]
        host.setEffectsVolume(sfx);
        if (speechPlayer != nullptr) speechPlayer->setMasterVolume(speech);
    }
    if (audio::World* world = gameSounds().world) world->player().setMasterVolume(1.0f * sfx);  // [0x007116A0]
    // Both meters' sounds with the masks 1 (0x00480F34, 0x00480FBB): only a free voice of the set, so a sound still
    // playing is not started again (its one voice is not taken back) and the next plays when it is over.
    if (std::fabs(sfx - m_sfxHeard) > 0.07f) {
        host.sound("Warning", 1.0f, 1);
        m_sfxHeard = sfx;
    }
    if (std::fabs(speech - m_speechHeard) > 0.01f) {
        host.setEffectsVolume(speech);
        host.sound("SPEECH", 1.0f, 1);
        m_speechHeard = speech;
        host.setEffectsVolume(sfx);
    }
    if (std::fabs(musicShown - m_musicHeard) > 0.07f) m_musicHeard = musicShown;
    if (event == input::kBack) {
        pauseMusic(host.racePause());
        set.setU32(save::Settings::kMusicVolume, std::bit_cast<uint32_t>(m_music));
        set.setU32(save::Settings::kEffectsVolume, std::bit_cast<uint32_t>(m_sfx));
        set.setU32(save::Settings::kSpeechVolume, std::bit_cast<uint32_t>(m_speech));
        set.setU32(save::Settings::kSpeakers, m_speakerKept);
        if (!host.racePause()) {
            host.setMusicVolume(m_music, false);
        } else {
            musicVolume(m_music);
        }
        host.setEffectsVolume(m_sfx);
        if (speechPlayer != nullptr) speechPlayer->setMasterVolume(m_speech);
        host.sound("CLICK");
        host.request(host.optionsMenu(), SwitchMode::Swap);
        return true;
    }
    if (event != input::kSelect) return false;
    if (is1("DON_BUT")) {
        host.sound("CLICK");
        host.request(host.optionsMenu(), SwitchMode::Swap);
        return true;
    }
    if (is1("playlist")) {
        if ((flags & 8) == 0) {
            host.sound("CLICK");
            host.request("opts_songlist", SwitchMode::Swap);
            return true;
        }
        host.sound("NO");
    }
    return false;
}

}  // namespace oj::frontend
