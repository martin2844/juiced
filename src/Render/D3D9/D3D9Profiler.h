#pragma once
// The frame's layers timed (the debug builds' --profile; the user, 2026-10-28: each layer's cost, to see what to make
// cheaper): a mark starts a layer, named, ending at the next mark or the frame's end. Each layer's time on the GPU
// (D3DQUERYTYPE_TIMESTAMP at each mark within a TIMESTAMPDISJOINT pair a frame, read some frames later) and on the CPU
// (the performance counter) and its draws and primitives, summed by name over the frames timed.

#include <windows.h>
#include <d3d9.h>

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace oj::render::d3d9 {

class Profiler {
public:
    Profiler() = default;
    ~Profiler();
    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;

    // On from the next frame, or off (the queries let go); false when the device has no timestamps.
    bool setOn(IDirect3DDevice9* device, bool on);
    bool on() const { return m_device != nullptr; }
    // The frame's start (its first layer "frame") and a layer's start.
    void beginFrame();
    void mark(const char* layer);
    // The frame's end, after the present.
    void endFrame();
    // A draw of the device's of `primitives` primitives.
    void count(uint32_t primitives) {
        ++m_draws;
        m_primitives += primitives;
    }
    // The queries let go and made again (the device reset).
    void release();
    // The layers' times per frame, the longest on the GPU first, and the frames' whole.
    std::string report();

private:
    struct Mark {
        const char* layer = nullptr;
        IDirect3DQuery9* stamp = nullptr;
        int64_t cpu = 0;
        uint64_t draws = 0;
        uint64_t primitives = 0;
    };
    struct Frame {
        IDirect3DQuery9* disjoint = nullptr;
        IDirect3DQuery9* frequency = nullptr;
        IDirect3DQuery9* end = nullptr;
        std::vector<Mark> marks;
        std::vector<IDirect3DQuery9*> pool;  // the timestamps made, used again
        int64_t cpuEnd = 0;
        uint64_t drawsEnd = 0;
        uint64_t primitivesEnd = 0;
        bool pending = false;
    };
    struct Sum {
        double gpu = 0.0;
        double cpu = 0.0;
        uint64_t marks = 0;
        uint64_t draws = 0;
        uint64_t primitives = 0;
    };
    IDirect3DQuery9* query(D3DQUERYTYPE type);
    void collect(Frame& f, bool wait);
    void drop(Frame& f);

    IDirect3DDevice9* m_device = nullptr;
    std::array<Frame, 6> m_frames{};
    size_t m_current = 0;
    bool m_inFrame = false;
    std::map<std::string, Sum> m_sums;
    uint64_t m_counted = 0;
    double m_gpuFrames = 0.0;
    double m_cpuFrames = 0.0;
    double m_cpuFrequency = 0.0;
    uint64_t m_draws = 0;  // the device's draws and primitives so far
    uint64_t m_primitives = 0;
    int64_t m_lastBegin = 0;  // the last frame's start: the frames' whole from start to start
    double m_wall = 0.0;
    uint64_t m_wallFrames = 0;
};

}  // namespace oj::render::d3d9
