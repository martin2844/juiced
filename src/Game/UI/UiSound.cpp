#include "Game/UI/UiSound.h"

#include "Audio/SoundSystem.h"

namespace oj {

void UiSound::open(const GameFiles& files, const std::string& bank) {
    if (!audio::system().on()) return;
    m_player = std::make_unique<audio::Player>(files, std::filesystem::path("audio") / "ui", bank);
    volume = 0.5f;
    if (m_player->banks() != 0) m_last.assign(m_player->bank(static_cast<int>(m_player->banks()) - 1)->count(), 0);
}

int UiSound::prepare(const char* name, uint32_t& handle) {
    handle = 0;
    if (!m_player) return 1;
    audio::SetRef ref{-1, -1};
    if (m_player->find(name, ref) != 0) return 1;
    int how = 0;
    return m_player->prepare(ref, 0xF, 0xF, how, handle);
}

void UiSound::addBank(const GameFiles& files, const std::string& name) {
    if (!m_player) return;
    for (size_t i = 0; i < m_player->banks(); ++i) {
        if (_stricmp(m_player->bank(static_cast<int>(i))->name().c_str(), name.c_str()) == 0) return;
    }
    if (auto bank = audio::Bank::load(files, std::filesystem::path("audio") / "ui" / name, name, nullptr)) {
        m_player->addBank(bank.get());
        m_banks.push_back(std::move(bank));
    }
}

void UiSound::frame(float dt) {
    m_frameTime = dt < 0.1f ? dt : 0.1f;
    ++m_frames;
}

uint32_t UiSound::play(const char* name, float vol, uint32_t mask) {
    if (!m_player) return 0;
    audio::SetRef ref{-1, -1};
    if (m_player->find(name, ref) != 0) return 0;
    if (ref.set != 0) {
        uint32_t& last = m_last[static_cast<size_t>(ref.set)];
        const float since = static_cast<float>(m_frames - last);  // unsigned, as a float
        if (0.05f / m_frameTime > since) return 0;  // [0x00725810]
        last = m_frames;
    } else {
        if (m_last.empty() || m_last[0] == m_frames) return 0;
        m_last[0] = m_frames;
    }
    int how = 0;
    uint32_t handle = 0;
    if (m_player->prepare(ref, mask, mask, how, handle) != 0) return handle;
    m_player->setFrequency(handle, 48000);
    m_player->setVolume(handle, vol * volume);
    m_player->setPlayFlags(handle, 0);
    m_player->start(handle);
    return handle;
}

}  // namespace oj
