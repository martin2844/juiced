#pragma once
// binkw32's calls for OpenJuice: the game's binkw32.dll in the 32-bit build, OpenJuice's own Bink 1 decoder
// (Game/Media/BinkNative.h) in the 64-bit one, which cannot load a 32-bit library. The stock code reads a movie's BINK
// structure (Width +0x00, Height +0x04, Frames +0x08, FrameNum +0x0C, NumTracks +0xF0, ...); fields() gives it as it
// stands after the last call.

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

namespace oj::bink {

class Library {
public:
    virtual ~Library() = default;

    // The library of the game's folder, started at the first call; nullptr when it cannot be (the reason logged
    // once).
    static Library* get(const std::filesystem::path& gameFolder);

    // BinkSetSoundSystem(BinkOpenDirectSound, the IDirectSound8 the movies play through).
    virtual int32_t setSoundSystem(uintptr_t directSound) = 0;
    virtual void setSoundTrack(uint32_t count, const uint32_t* tracks) = 0;
    // 0 when it does not open.
    virtual uint32_t open(const std::filesystem::path& file, uint32_t flags) = 0;
    virtual void close(uint32_t movie) = 0;
    virtual int32_t doFrame(uint32_t movie) = 0;
    virtual void nextFrame(uint32_t movie) = 0;
    virtual int32_t wait(uint32_t movie) = 0;
    virtual int32_t pause(uint32_t movie, int32_t on) = 0;
    virtual void setVolume(uint32_t movie, uint32_t track, int32_t volume) = 0;
    // The rectangle of the current frame into `dest` (rows of `pitch` bytes, destHeight of them).
    virtual int32_t copyToBufferRect(uint32_t movie, void* dest, int32_t pitch, uint32_t destHeight, uint32_t destX,
                                     uint32_t destY, uint32_t srcX, uint32_t srcY, uint32_t srcW, uint32_t srcH,
                                     uint32_t flags) = 0;
    // The movie's BINK structure (kFieldBytes of it); nullptr for none.
    virtual const uint8_t* fields(uint32_t movie) = 0;
    // A word of the structure written.
    virtual void setField(uint32_t movie, uint32_t offset, uint32_t value) = 0;

    uint32_t field(uint32_t movie, uint32_t offset) {
        const uint8_t* f = fields(movie);
        uint32_t v = 0;
        if (f != nullptr) std::memcpy(&v, f + offset, 4);
        return v;
    }
};

}  // namespace oj::bink
