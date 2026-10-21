#pragma once
// The functions of the game's binkw32.dll (RAD Game Tools' Bink 1) that the stock code imports, loaded from the
// game's folder; only a 32-bit process can load it (OpenJuice's 32-bit build; the 64-bit one decodes the movies itself,
// Game/Media/BinkNative.h). Movies are kept by number: their BINK structure and the file handle it reads.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace oj::bink {

class Binkw32 {
public:
    ~Binkw32();
    bool load(const std::filesystem::path& folder, std::string* why);
    bool loaded() const { return m_module != nullptr; }

    // BinkSetSoundSystem(BinkOpenDirectSound, param).
    int32_t setSoundSystem(uint32_t param);
    // BinkSetSoundTrack(count, tracks).
    void setSoundTrack(uint32_t count, const uint32_t* tracks);
    // BinkOpen: with BINKFILEHANDLE (0x00800000) in the flags the file is opened and its handle passed, else its
    // name; 0 when it does not open.
    uint32_t open(const std::filesystem::path& file, uint32_t flags);
    void close(uint32_t movie);
    int32_t doFrame(uint32_t movie);
    void nextFrame(uint32_t movie);
    int32_t wait(uint32_t movie);
    int32_t pause(uint32_t movie, int32_t on);
    void setVolume(uint32_t movie, uint32_t track, int32_t volume);
    int32_t copyToBufferRect(uint32_t movie, void* dest, int32_t pitch, uint32_t destHeight, uint32_t destX,
                             uint32_t destY, uint32_t srcX, uint32_t srcY, uint32_t srcW, uint32_t srcH, uint32_t flags);
    // The BINK structure of an open movie; nullptr for none.
    uint8_t* fields(uint32_t movie) const;

private:
    struct Movie {
        void* bink = nullptr;
        void* file = nullptr;  // INVALID_HANDLE_VALUE or the file's handle
    };
    void* bink(uint32_t movie) const;

    void* m_module = nullptr;
    void* (__stdcall* m_open)(const char*, uint32_t) = nullptr;
    void(__stdcall* m_close)(void*) = nullptr;
    int32_t(__stdcall* m_doFrame)(void*) = nullptr;
    void(__stdcall* m_nextFrame)(void*) = nullptr;
    int32_t(__stdcall* m_wait)(void*) = nullptr;
    int32_t(__stdcall* m_pause)(void*, int32_t) = nullptr;
    void(__stdcall* m_setVolume)(void*, uint32_t, int32_t) = nullptr;
    void(__stdcall* m_setSoundTrack)(uint32_t, uint32_t*) = nullptr;
    int32_t(__stdcall* m_copyToBufferRect)(void*, void*, int32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                                           uint32_t, uint32_t, uint32_t) = nullptr;
    int32_t(__stdcall* m_setSoundSystem)(void*, uint32_t) = nullptr;
    void* m_openDirectSound = nullptr;
    std::vector<Movie> m_movies;  // number - 1
};

}  // namespace oj::bink
