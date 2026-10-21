// The front end's sounds and music: the interface's sounds, the speech, the phone's ring and the music's volume.

#include "Game/Frontend/Frontend.h"
#include "Game/Frontend/FrontendInternal.h"

namespace oj {

using namespace frontend;

void Frontend::loadMusic() {
    if ((audio::system().flags() & audio::kNoMusic) != 0) return;
    gameRandom().next();
    m_ui.addBank(GameFiles(std::filesystem::current_path()), "music1");
}

void Frontend::startMusic() {
    if ((audio::system().flags() & audio::kNoMusic) != 0) return;
    audio::Player* p = m_ui.player();
    if (m_musicHandle != 0 && p != nullptr) {
        int state = 0;
        p->state(m_musicHandle, state);
        if (state != audio::kSoundStopped && state != audio::kSoundFadingOut && state != audio::kSoundNone) return;
    }
    if (m_ui.prepare("music1", m_musicHandle) != 0 || m_musicHandle == 0) return;
    const float volume = std::bit_cast<float>(settingsValue(save::Settings::kMusicVolume));
    p->setVolume(m_musicHandle, volume);
    m_musicTarget = volume;
    m_musicVolume = volume;
    p->setFrequency(m_musicHandle, 48000);
    p->setPlayFlags(m_musicHandle, 1);  // DSBPLAY_LOOPING
    p->hold(m_musicHandle);
    p->start(m_musicHandle);
}

void Frontend::sound(const char* name) {
    m_sounds->play(name);
}

void Frontend::stopSpeech() {
    if (Speech* speech = gameSounds().speech) speech->stop();
}

void Frontend::sound(const char* name, float volume, uint32_t mask) {
    m_sounds->play(name, volume, mask);
}

void Frontend::setMusicVolume(float volume, bool targetOnly) {
    if (m_musicHandle == 0) return;
    if (!targetOnly) {
        if (audio::Player* p = m_ui.player()) p->setVolume(m_musicHandle, volume);
        m_musicVolume = volume;
    }
    m_musicTarget = volume;
}

void Frontend::settingsVolumes() {
    setMusicVolume(std::bit_cast<float>(settingsValue(save::Settings::kMusicVolume)), false);
    m_sounds->setVolume(std::bit_cast<float>(settingsValue(save::Settings::kEffectsVolume)));
    if (gameSounds().speech != nullptr && gameSounds().speech->player() != nullptr) {
        const float speech = std::bit_cast<float>(settingsValue(save::Settings::kSpeechVolume));
        gameSounds().speech->player()->setMasterVolume(speech);
    }
}

void Frontend::musicFade(float frameTime) {
    constexpr float kLow = 0.001f;  // [0x0071169C]
    if (m_musicTarget != m_musicVolume) {
        m_musicVolume = (m_musicTarget - m_musicVolume) / (1000.0f * frameTime) + m_musicVolume;  // [0x00715C5C]
        m_musicVolume = m_musicVolume < kLow ? kLow : (1.0f < m_musicVolume ? 1.0f : m_musicVolume);
        if (m_musicVolume <= kLow) {
            m_musicVolume = m_musicTarget;
            return;
        }
    }
    if (audio::Player* p = m_ui.player()) p->setVolume(m_musicHandle, m_musicVolume);
}

}  // namespace oj
