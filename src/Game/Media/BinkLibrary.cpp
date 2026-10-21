#include "Game/Media/BinkLibrary.h"

#include "Core/Base/Log.h"
#include "Game/Media/BinkNative.h"
#include "Game/Media/Binkw32.h"

namespace oj::bink {

namespace {

#if defined(_M_IX86)

// The 32-bit build: the library in this process.
class Direct final : public Library {
public:
    bool start(const std::filesystem::path& folder, std::string* why) { return m_dll.load(folder, why); }

    int32_t setSoundSystem(uintptr_t directSound) override {
        return m_dll.setSoundSystem(static_cast<uint32_t>(directSound));
    }
    void setSoundTrack(uint32_t count, const uint32_t* tracks) override { m_dll.setSoundTrack(count, tracks); }
    uint32_t open(const std::filesystem::path& file, uint32_t flags) override { return m_dll.open(file, flags); }
    void close(uint32_t movie) override { m_dll.close(movie); }
    int32_t doFrame(uint32_t movie) override { return m_dll.doFrame(movie); }
    void nextFrame(uint32_t movie) override { m_dll.nextFrame(movie); }
    int32_t wait(uint32_t movie) override { return m_dll.wait(movie); }
    int32_t pause(uint32_t movie, int32_t on) override { return m_dll.pause(movie, on); }
    void setVolume(uint32_t movie, uint32_t track, int32_t volume) override { m_dll.setVolume(movie, track, volume); }
    int32_t copyToBufferRect(uint32_t movie, void* dest, int32_t pitch, uint32_t destHeight, uint32_t destX,
                             uint32_t destY, uint32_t srcX, uint32_t srcY, uint32_t srcW, uint32_t srcH,
                             uint32_t flags) override {
        return m_dll.copyToBufferRect(movie, dest, pitch, destHeight, destX, destY, srcX, srcY, srcW, srcH, flags);
    }
    const uint8_t* fields(uint32_t movie) override { return m_dll.fields(movie); }
    void setField(uint32_t movie, uint32_t offset, uint32_t value) override {
        if (uint8_t* f = m_dll.fields(movie)) std::memcpy(f + offset, &value, 4);
    }

private:
    Binkw32 m_dll;
};

using Chosen = Direct;
constexpr const char* kReady = "bink: the game's binkw32.dll ready";

#else

// The 64-bit build: OpenJuice's own decoder.
using Chosen = Native;
constexpr const char* kReady = "bink: movies decoded by OpenJuice (Bink 1)";

#endif

}  // namespace

Library* Library::get(const std::filesystem::path& gameFolder) {
    static std::unique_ptr<Chosen> library;
    static bool tried = false;
    if (!tried) {
        tried = true;
        auto made = std::make_unique<Chosen>();
        std::string why;
        if (made->start(gameFolder, &why)) {
            library = std::move(made);
            log::info("{}", kReady);
        } else {
            log::warning("bink: no movies: {}", why);
        }
    }
    return library.get();
}

}  // namespace oj::bink
