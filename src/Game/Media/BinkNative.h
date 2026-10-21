#pragma once
// The 64-bit build's movies: the game's binkw32.dll is a 32-bit library, so OpenJuice reads the Bink 1 files
// ('BIKi') itself and answers binkw32's calls (BinkLibrary.h) as the library does. The pictures: each frame's planes
// (Y, the two chroma planes at half size, the alpha plane when the file has one) from their bundles of 8x8 blocks,
// turned into the surface type BinkCopyToBufferRect is asked for. The sound: the track BinkSetSoundTrack asked for
// (Bink audio, DCT or RDFT) played through the game's DirectSound.

#include "Game/Media/BinkLibrary.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace oj::bink {

// A Bink 1 file's header: the frames, the picture, the audio tracks and the frames' index.
struct MovieHeader {
    struct Track {
        uint32_t id = 0;
        uint32_t sampleRate = 0;
        uint32_t flags = 0;  // 0x1000 DCT (else RDFT), 0x2000 stereo, 0x4000 16-bit
    };
    uint8_t revision = 0;  // the fourth byte of the signature ('f' .. 'i')
    uint32_t frames = 0;
    uint32_t largestFrame = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t rate = 0;
    uint32_t rateDiv = 0;
    uint32_t videoFlags = 0;  // 0x00100000 alpha plane, 0x00020000 grayscale
    std::vector<Track> tracks;
    std::vector<uint32_t> index;  // frames + 1 file offsets, bit 0 a key frame

    // The fixed part (44 bytes): false when it is not a Bink 1 file. tableBytes() then tells how much follows.
    bool readStart(const uint8_t* bytes);
    size_t tableBytes() const;
    // The tracks and the index after the fixed part.
    bool readTables(const uint8_t* bytes, uint64_t fileSize);
};

// The pictures of a movie, frame after frame (each frame's video data, after its audio).
class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();
    void start(const MovieHeader& header);
    // False when the data ends early or is broken (the picture then keeps what was decoded).
    bool decode(const uint8_t* data, size_t bytes);
    // 0 Y, 1 U (Cb), 2 V (Cr): the chroma at half the size; 3 the alpha plane (nullptr without one).
    const uint8_t* plane(int index) const;
    int32_t stride(int index) const;

private:
    struct State;
    std::unique_ptr<State> m;
};

// A track's sound: each frame's packet of it into 16-bit samples.
class AudioDecoder {
public:
    AudioDecoder();
    ~AudioDecoder();
    void start(uint32_t sampleRate, uint32_t flags);
    uint32_t channels() const;
    uint32_t sampleRate() const;
    // A packet (its first 4 bytes the size of its samples): the samples, channels interleaved, added to out.
    void decode(const uint8_t* data, size_t bytes, std::vector<int16_t>& out);

private:
    struct State;
    std::unique_ptr<State> m;
};

// binkw32's calls answered by the decoders above. The movies' sound goes to secondary buffers of the DirectSound
// given to setSoundSystem, fed by a thread of its own as binkw32's sound timer does.
class Native final : public Library {
public:
    Native();
    ~Native() override;
    bool start(const std::filesystem::path& folder, std::string* why);

    int32_t setSoundSystem(uintptr_t directSound) override;
    void setSoundTrack(uint32_t count, const uint32_t* tracks) override;
    uint32_t open(const std::filesystem::path& file, uint32_t flags) override;
    void close(uint32_t movie) override;
    int32_t doFrame(uint32_t movie) override;
    void nextFrame(uint32_t movie) override;
    int32_t wait(uint32_t movie) override;
    int32_t pause(uint32_t movie, int32_t on) override;
    void setVolume(uint32_t movie, uint32_t track, int32_t volume) override;
    int32_t copyToBufferRect(uint32_t movie, void* dest, int32_t pitch, uint32_t destHeight, uint32_t destX,
                             uint32_t destY, uint32_t srcX, uint32_t srcY, uint32_t srcW, uint32_t srcH,
                             uint32_t flags) override;
    const uint8_t* fields(uint32_t movie) override;
    void setField(uint32_t movie, uint32_t offset, uint32_t value) override;

private:
    struct Movie;
    struct Sound;
    Movie* find(uint32_t movie) const;
    void decodeFrame(Movie& m, uint32_t frame);
    void startSound(Movie& m);
    void serviceSounds();

    void* m_directSound = nullptr;  // IDirectSound8
    std::vector<uint32_t> m_tracks;
    std::vector<std::unique_ptr<Movie>> m_movies;  // number - 1
    std::mutex m_soundLock;                        // the sounds' buffers and samples, shared with the thread
    std::vector<Sound*> m_sounds;
    std::thread m_soundThread;
    bool m_quit = false;
};

}  // namespace oj::bink
