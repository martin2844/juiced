#include "Game/Media/Binkw32.h"

#include <windows.h>

namespace oj::bink {

namespace {

template <typename F>
bool find(HMODULE module, const char* name, F& function) {
    function = reinterpret_cast<F>(reinterpret_cast<void*>(GetProcAddress(module, name)));
    return function != nullptr;
}

}  // namespace

Binkw32::~Binkw32() {
    for (uint32_t i = 0; i < m_movies.size(); ++i) close(i + 1);
    if (m_module != nullptr) FreeLibrary(static_cast<HMODULE>(m_module));
}

bool Binkw32::load(const std::filesystem::path& folder, std::string* why) {
    const std::filesystem::path dll = folder / L"binkw32.dll";
    HMODULE module = LoadLibraryExW(dll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (module == nullptr) {
        if (why != nullptr) *why = "binkw32.dll of the game not loaded (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    const bool all = find(module, "_BinkOpen@8", m_open) && find(module, "_BinkClose@4", m_close) &&
                     find(module, "_BinkDoFrame@4", m_doFrame) && find(module, "_BinkNextFrame@4", m_nextFrame) &&
                     find(module, "_BinkWait@4", m_wait) && find(module, "_BinkPause@8", m_pause) &&
                     find(module, "_BinkSetVolume@12", m_setVolume) &&
                     find(module, "_BinkSetSoundTrack@8", m_setSoundTrack) &&
                     find(module, "_BinkCopyToBufferRect@44", m_copyToBufferRect) &&
                     find(module, "_BinkSetSoundSystem@8", m_setSoundSystem);
    m_openDirectSound = reinterpret_cast<void*>(GetProcAddress(module, "_BinkOpenDirectSound@4"));
    if (!all || m_openDirectSound == nullptr) {
        FreeLibrary(module);
        if (why != nullptr) *why = "binkw32.dll lacks a function the game calls";
        return false;
    }
    m_module = module;
    return true;
}

int32_t Binkw32::setSoundSystem(uint32_t param) {
    return m_setSoundSystem(m_openDirectSound, param);
}

void Binkw32::setSoundTrack(uint32_t count, const uint32_t* tracks) {
    m_setSoundTrack(count, const_cast<uint32_t*>(tracks));
}

uint32_t Binkw32::open(const std::filesystem::path& file, uint32_t flags) {
    Movie m;
    m.file = INVALID_HANDLE_VALUE;
    if ((flags & 0x00800000u) != 0) {
        m.file = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (m.file == INVALID_HANDLE_VALUE) return 0;
        m.bink = m_open(static_cast<const char*>(m.file), flags);
    } else {
        m.bink = m_open(file.string().c_str(), flags);
    }
    if (m.bink == nullptr) {
        if (m.file != INVALID_HANDLE_VALUE) CloseHandle(m.file);
        return 0;
    }
    for (uint32_t i = 0; i < m_movies.size(); ++i) {
        if (m_movies[i].bink == nullptr) {
            m_movies[i] = m;
            return i + 1;
        }
    }
    m_movies.push_back(m);
    return static_cast<uint32_t>(m_movies.size());
}

void Binkw32::close(uint32_t movie) {
    if (movie == 0 || movie > m_movies.size() || m_movies[movie - 1].bink == nullptr) return;
    Movie& m = m_movies[movie - 1];
    m_close(m.bink);
    if (m.file != INVALID_HANDLE_VALUE) CloseHandle(m.file);
    m = {};
}

void* Binkw32::bink(uint32_t movie) const {
    return movie != 0 && movie <= m_movies.size() ? m_movies[movie - 1].bink : nullptr;
}

int32_t Binkw32::doFrame(uint32_t movie) {
    void* b = bink(movie);
    return b != nullptr ? m_doFrame(b) : 0;
}

void Binkw32::nextFrame(uint32_t movie) {
    if (void* b = bink(movie)) m_nextFrame(b);
}

int32_t Binkw32::wait(uint32_t movie) {
    void* b = bink(movie);
    return b != nullptr ? m_wait(b) : 0;
}

int32_t Binkw32::pause(uint32_t movie, int32_t on) {
    void* b = bink(movie);
    return b != nullptr ? m_pause(b, on) : 0;
}

void Binkw32::setVolume(uint32_t movie, uint32_t track, int32_t volume) {
    if (void* b = bink(movie)) m_setVolume(b, track, volume);
}

int32_t Binkw32::copyToBufferRect(uint32_t movie, void* dest, int32_t pitch, uint32_t destHeight, uint32_t destX,
                                  uint32_t destY, uint32_t srcX, uint32_t srcY, uint32_t srcW, uint32_t srcH,
                                  uint32_t flags) {
    void* b = bink(movie);
    return b != nullptr ? m_copyToBufferRect(b, dest, pitch, destHeight, destX, destY, srcX, srcY, srcW, srcH, flags)
                        : 0;
}

uint8_t* Binkw32::fields(uint32_t movie) const {
    return static_cast<uint8_t*>(bink(movie));
}

}  // namespace oj::bink
