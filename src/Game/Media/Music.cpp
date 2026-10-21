#include "Game/Media/Music.h"

#include "Audio/SoundSystem.h"
#include "Audio/Bank.h"
#include "Core/Base/Log.h"
#include "Core/Base/Random.h"
#include "Game/UI/UiSound.h"

#include <windows.h>

#include <cstring>

namespace oj {

Jukebox& jukebox() {
    static Jukebox j;
    return j;
}

GameSounds& gameSounds() {
    static GameSounds g;
    return g;
}

void Jukebox::open(const GameFiles& files) {
    if (m_player || !audio::system().on()) return;
    m_player = std::make_unique<audio::Player>(files, std::filesystem::path("audio"), "music");
    m_player->resumeAll();
}

void Jukebox::reopen(const GameFiles& files) {
    m_player.reset();
    handle = 0;
    index = 0xFFFFFFFF;
    volume = 0.0f;
    target = 0.0f;
    song = -1;
    paused = false;
    open(files);
}

void Jukebox::next(const std::vector<uint32_t>& playlist) {
    const uint32_t count = static_cast<uint32_t>(playlist.size());
    if (count == 0) {
        song = -1;
        return;
    }
    index = (index + 1) % count;
    song = static_cast<int32_t>(playlist[index]);
}

bool Jukebox::playing() {
    if (!m_player || handle == 0) return false;
    int state = 0;
    m_player->state(handle, state);
    return state >= 2 && state <= 4;
}

void Jukebox::play(const std::vector<uint32_t>& playlist, bool wait, bool force) {
    if ((audio::system().flags() & audio::kNoMusic) != 0 || !m_player) return;
    if ((handle == 0 && song == -1) || force) next(playlist);
    if (playing() || song == -1) return;
    // 0x0040B330
    const float v = volume;
    int how = 0;
    uint32_t h = 0;
    if (m_player->prepare({0, song}, 0xF, 0xF, how, h) == 0) {
        m_player->setFrequency(h, 48000);
        m_player->setVolume(h, v);
        m_player->setPlayFlags(h, 0);
        m_player->start(h);
    }
    handle = h;
    log::info("music: song {} (handle {:#x}) at {}", song, h, v);
    audio::system().update();
    // With wait half a second slept and then one job of the main thread's queue ([0x0075F10C] + 0x204, 0x0051D200)
    // run when it holds any (none here).
    if (wait) Sleep(500);
    audio::system().update();
}

void Jukebox::stop(bool wait) {
    if ((audio::system().flags() & audio::kNoMusic) != 0 || !m_player || handle == 0) return;
    audio::system().update();
    if (wait) Sleep(300);
    audio::system().update();
    m_player->stop(handle);
    handle = 0;
    if (wait) Sleep(300);
    audio::system().update();
    paused = false;
}

void Jukebox::update(const std::vector<uint32_t>& playlist, float frameTime, bool nextSong) {
    if ((audio::system().flags() & audio::kNoMusic) != 0 || !m_player || handle == 0 || paused) return;
    constexpr float kLow = 0.001f;  // [0x0071169C]
    if (!previewing && volume < kLow && target == 0.0f) {
        stop(false);
        return;
    }
    if (target != volume) {
        float d = (target - volume) / frameTime;
        d = d / 500.0f;  // [0x00711694] down, [0x00711698] up
        volume = d + volume;
        volume = volume < kLow ? kLow : (1.0f < volume ? 1.0f : volume);  // 0x00407EC0
        m_player->setVolume(handle, volume);
        if (d < 0.0f && volume <= kLow) {
            volume = target;
            stop(false);
            return;
        }
        if (0.0f < d && target - kLow <= volume) volume = target;
    }
    if (!playing() && nextSong) {
        stop(false);
        song = -1;
        play(playlist, false, false);
    }
}

namespace {
std::unique_ptr<Speech> g_speech;  // [G+0x4C]
std::vector<std::unique_ptr<audio::Bank>> g_windowBanks;
std::unique_ptr<audio::World> g_windowWorld;
}

audio::World* makeWindowWorld() {
    freeWindowWorld();
    if (!audio::system().on()) return nullptr;
    g_windowWorld = std::make_unique<audio::World>(GameFiles(std::filesystem::current_path()), 256, gameRandom());
    audio::World& w = *g_windowWorld;
    const float v = gameSounds().effects();
    w.down = 20.0f;
    w.target = v;
    w.volume = v;
    w.up = 50.0f;
    w.player().setMasterVolume(v);
    w.manager().pauseAll(false);
    gameSounds().world = g_windowWorld.get();
    return g_windowWorld.get();
}

void freeWindowWorld() {
    if (!g_windowWorld) return;
    if (gameSounds().world == g_windowWorld.get()) gameSounds().world = nullptr;
    g_windowWorld.reset();
    g_windowBanks.clear();
}

void addWindowBank(const std::string& name) {
    if (!g_windowWorld) return;
    audio::Player& p = g_windowWorld->player();
    for (size_t i = 0; i < p.banks(); ++i) {
        if (_stricmp(p.bank(static_cast<int>(i))->name().c_str(), name.c_str()) == 0) return;
    }
    const GameFiles files(std::filesystem::current_path());
    if (auto bank = audio::Bank::load(files, std::filesystem::path("audio") / "cars" / name, name, nullptr)) {
        p.addBank(bank.get());
        g_windowBanks.push_back(std::move(bank));
    }
}

Speech::Speech(const GameFiles& files, const std::string& name, Kind k) : kind(k) {
    if (!audio::system().on()) return;
    m_player = std::make_unique<audio::Player>(files, std::filesystem::path("audio") / "speech", name);
    paused = false;
    m_player->resumeAll();
}

Speech::~Speech() {
    if (gameSounds().speech == this) gameSounds().speech = nullptr;
}

uint32_t Speech::play(const audio::SetRef& ref, float volume, uint32_t hz, uint32_t flags, uint32_t slotMask,
                      uint32_t voiceMask) {
    if (!m_player || paused) return 0;
    int how = 0;
    uint32_t h = 0;
    m_player->prepare(ref, slotMask, voiceMask, how, h);
    m_player->setFrequency(h, hz);
    m_player->setVolume(h, volume);
    m_player->setPlayFlags(h, flags);
    m_player->start(h);
    return h;
}

uint32_t Speech::play(const char* name, float volume, uint32_t hz, uint32_t flags, uint32_t slotMask) {
    if (!m_player || paused) return 0;
    audio::SetRef ref{-1, -1};
    m_player->find(name, ref);
    if (ref.set == -1) return 0;
    return play(ref, volume, hz, flags, slotMask, 0xF);
}

void Speech::say(int group, int index) {
    if ((audio::system().flags() & audio::kNoSpeech) != 0) return;
    if (kind == kInterface) {
        stop();
        const int set = (group - 1) * 0x21 + index;  // [0x007116B0]
        pending = {0, set};
        waiting = false;
        pendingVolume = 1.0f;
        handle = play({0, set}, 1.0f, 48000, 0, 0xF, 0xF);
    } else {
        pending = {0, (group - 1) * 0x10 + index};  // [0x007116C0]
        pendingVolume = 1.0f;
        duck();
    }
}

void Speech::say2(int a, int b) {
    if ((audio::system().flags() & audio::kNoSpeech) != 0) return;
    if (kind == kInterface) {
        const int set = 6 * a + 0x21 * 8 + b;  // [0x007116B8], [0x007116B0], [0x007116AC]
        pending = {0, set};
        waiting = false;
        pendingVolume = 1.0f;
        handle = play({0, set}, 1.0f, 48000, 0, 0xF, 0xF);
    } else {
        constexpr int kNever = 0;  // [0x0075EC90] x [0x0075EC8C]: never written
        pending = {0, 0x23 * a + (kNever + 0x10) * 8 + b};  // [0x007116C8], [0x007116C0], [0x007116BC]
        pendingVolume = 1.0f;
        duck();
    }
}

void Speech::sayName(const char* name) {
    if ((audio::system().flags() & audio::kNoSpeech) != 0 || !m_player) return;
    audio::SetRef ref{-1, -1};
    m_player->find(name, ref);
    if (ref.set == -1) return;
    pending = ref;
    pendingVolume = 1.0f;
    waiting = false;
    handle = play(ref, 1.0f, 48000, 0, 0xF, 0xF);
}

void Speech::stop() {
    if (handle != 0) {
        if (m_player) m_player->stop(handle);
        audio::system().update();
    }
    waiting = false;
    handle = 0;
}

bool Speech::speaking() {
    if (!m_player) return false;
    if (handle == 0) {
        if (waiting) return true;
    } else {
        int state = 0;
        m_player->state(handle, state);
        if (state != 0 && state != 1) return true;
    }
    handle = 0;
    return false;
}

void Speech::duck() {
    GameSounds& g = gameSounds();
    if (!(g.speechVolume() > 0.0f)) return;
    if (audio::World* w = g.world) {
        w->down = 20.0f;
        w->up = 50.0f;
        w->target = 0.5f * g.effects();  // [0x007116A4]
        w->player().setMasterVolume(w->volume);
    }
    Jukebox& music = jukebox();
    if (music.player() != nullptr && music.handle != 0) music.target = 0.5f * g.music();
    waiting = true;
    delay = 0.3f;  // [0x007116A8]
}

void Speech::update(float dt) {
    if (waiting) {
        delay = delay - dt;
        if (delay < 0.0f) {
            waiting = false;
            handle = play(pending, pendingVolume, 48000, 0, 0xF, 0xF);
        }
        return;
    }
    if (speaking()) return;
    GameSounds& g = gameSounds();
    if (audio::World* w = g.world; w != nullptr && !holdWorld) {
        w->down = 20.0f;
        w->up = 50.0f;
        w->target = 1.0f * g.effects();  // [0x007116A0]
        w->player().setMasterVolume(w->volume);
    }
    Jukebox& music = jukebox();
    if (g.raceFrontEnd && music.player() != nullptr && music.handle != 0) music.target = g.music();
}

void makeSpeech(int kind, int language, const GameFiles& files) {
    if ((audio::system().flags() & audio::kNoSpeech) != 0) return;
    gameSounds().speech = nullptr;
    g_speech.reset();
    if (kind == 0 || !audio::system().on()) return;
    if (language < 0 || language > 7) language = 0;
    const char letter = "efgiscpr"[language];
    if (kind == Speech::kInterface) {
        g_speech = std::make_unique<Speech>(files, std::string("ui") + letter, Speech::kInterface);
    } else if (kind == Speech::kRace) {
        g_speech = std::make_unique<Speech>(files, std::string("race") + letter, Speech::kRace);
    } else {
        return;
    }
    gameSounds().speech = g_speech.get();
    if (audio::Player* p = g_speech->player()) p->setMasterVolume(gameSounds().speechVolume());
}

void pauseGameSounds(bool paused, UiSound* ui) {
    if (!audio::system().on()) return;
    GameSounds& g = gameSounds();
    if (g.speech != nullptr && g.speech->player() != nullptr) {
        g.speech->paused = paused;
        if (paused) {
            g.speech->player()->pauseAll();
        } else {
            g.speech->player()->resumeAll();
        }
    }
    if (g.world != nullptr) g.world->manager().pauseAll(paused);
    audio::system().update();
    Jukebox& music = jukebox();
    if (music.player() != nullptr) {
        music.paused = paused;
        if (music.handle != 0) {
            if (paused) {
                music.player()->pause(music.handle);
            } else {
                music.player()->resume(music.handle);
            }
        }
    }
    audio::system().update();
    if (ui == nullptr || ui->player() == nullptr) return;
    if (paused) {
        ui->player()->pauseAll();
    } else {
        ui->player()->resumeAll();
    }
}

}  // namespace oj
