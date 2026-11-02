#include "Render/D3D9/D3D9Profiler.h"

#include <algorithm>
#include <format>
#include <utility>

namespace oj::render::d3d9 {

namespace {

int64_t counter() {
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}

void releaseQuery(IDirect3DQuery9*& q) {
    if (q != nullptr) q->Release();
    q = nullptr;
}

}  // namespace

Profiler::~Profiler() {
    release();
}

bool Profiler::setOn(IDirect3DDevice9* device, bool on) {
    release();
    m_device = nullptr;
    m_sums.clear();
    m_counted = 0;
    m_gpuFrames = 0.0;
    m_cpuFrames = 0.0;
    m_lastBegin = 0;
    m_wall = 0.0;
    m_wallFrames = 0;
    if (!on || device == nullptr) return false;
    for (const D3DQUERYTYPE type :
         {D3DQUERYTYPE_TIMESTAMP, D3DQUERYTYPE_TIMESTAMPDISJOINT, D3DQUERYTYPE_TIMESTAMPFREQ}) {
        if (FAILED(device->CreateQuery(type, nullptr))) return false;
    }
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    m_cpuFrequency = static_cast<double>(frequency.QuadPart);
    m_device = device;
    return true;
}

IDirect3DQuery9* Profiler::query(D3DQUERYTYPE type) {
    IDirect3DQuery9* q = nullptr;
    if (FAILED(m_device->CreateQuery(type, &q))) return nullptr;
    return q;
}

void Profiler::beginFrame() {
    if (m_device == nullptr) return;
    const int64_t now = counter();
    if (m_lastBegin != 0) {
        m_wall += static_cast<double>(now - m_lastBegin) * 1000.0 / m_cpuFrequency;
        ++m_wallFrames;
    }
    m_lastBegin = now;
    Frame& f = m_frames[m_current];
    if (f.pending) collect(f, true);
    f.marks.clear();
    if (f.disjoint == nullptr) f.disjoint = query(D3DQUERYTYPE_TIMESTAMPDISJOINT);
    if (f.frequency == nullptr) f.frequency = query(D3DQUERYTYPE_TIMESTAMPFREQ);
    if (f.end == nullptr) f.end = query(D3DQUERYTYPE_TIMESTAMP);
    if (f.disjoint == nullptr || f.frequency == nullptr || f.end == nullptr) return;
    f.disjoint->Issue(D3DISSUE_BEGIN);
    m_inFrame = true;
    mark("frame");
}

void Profiler::mark(const char* layer) {
    if (!m_inFrame) return;
    Frame& f = m_frames[m_current];
    const size_t used = f.marks.size();
    if (used == f.pool.size()) {
        IDirect3DQuery9* q = query(D3DQUERYTYPE_TIMESTAMP);
        if (q == nullptr) return;
        f.pool.push_back(q);
    }
    IDirect3DQuery9* stamp = f.pool[used];
    stamp->Issue(D3DISSUE_END);
    f.marks.push_back({layer, stamp, counter(), m_draws, m_primitives});
}

void Profiler::endFrame() {
    if (!m_inFrame) return;
    m_inFrame = false;
    Frame& f = m_frames[m_current];
    f.end->Issue(D3DISSUE_END);
    f.disjoint->Issue(D3DISSUE_END);
    f.frequency->Issue(D3DISSUE_END);
    f.cpuEnd = counter();
    f.drawsEnd = m_draws;
    f.primitivesEnd = m_primitives;
    f.pending = true;
    m_current = (m_current + 1) % m_frames.size();
}

void Profiler::collect(Frame& f, bool wait) {
    const auto get = [&](IDirect3DQuery9* q, void* data, DWORD size) {
        HRESULT hr = S_FALSE;
        while ((hr = q->GetData(data, size, D3DGETDATA_FLUSH)) == S_FALSE) {
            if (!wait) return false;
        }
        return hr == S_OK;
    };
    BOOL disjoint = TRUE;
    UINT64 frequency = 0, end = 0;
    if (!get(f.disjoint, &disjoint, sizeof disjoint) || !get(f.frequency, &frequency, sizeof frequency) ||
        !get(f.end, &end, sizeof end)) {
        if (wait) f.pending = false;
        return;
    }
    std::vector<UINT64> stamps(f.marks.size());
    for (size_t i = 0; i < f.marks.size(); ++i) {
        if (!get(f.marks[i].stamp, &stamps[i], sizeof stamps[i])) {
            f.pending = false;
            return;
        }
    }
    f.pending = false;
    if (disjoint || frequency == 0 || f.marks.empty()) return;
    const double gpuScale = 1000.0 / static_cast<double>(frequency);
    const double cpuScale = 1000.0 / m_cpuFrequency;
    for (size_t i = 0; i < f.marks.size(); ++i) {
        const UINT64 next = i + 1 < f.marks.size() ? stamps[i + 1] : end;
        const int64_t cpuNext = i + 1 < f.marks.size() ? f.marks[i + 1].cpu : f.cpuEnd;
        const bool last = i + 1 == f.marks.size();
        Sum& s = m_sums[f.marks[i].layer];
        s.gpu += static_cast<double>(next - stamps[i]) * gpuScale;
        s.cpu += static_cast<double>(cpuNext - f.marks[i].cpu) * cpuScale;
        s.draws += (last ? f.drawsEnd : f.marks[i + 1].draws) - f.marks[i].draws;
        s.primitives += (last ? f.primitivesEnd : f.marks[i + 1].primitives) - f.marks[i].primitives;
        ++s.marks;
    }
    ++m_counted;
    m_gpuFrames += static_cast<double>(end - stamps.front()) * gpuScale;
    m_cpuFrames += static_cast<double>(f.cpuEnd - f.marks.front().cpu) * cpuScale;
}

void Profiler::drop(Frame& f) {
    releaseQuery(f.disjoint);
    releaseQuery(f.frequency);
    releaseQuery(f.end);
    for (IDirect3DQuery9*& q : f.pool) releaseQuery(q);
    f.pool.clear();
    f.marks.clear();
    f.pending = false;
}

void Profiler::release() {
    for (Frame& f : m_frames) drop(f);
    m_inFrame = false;
}

std::string Profiler::report() {
    for (Frame& f : m_frames) {
        if (f.pending) collect(f, true);
    }
    if (m_counted == 0) return "profile: no frame timed";
    const double n = static_cast<double>(m_counted);
    std::vector<std::pair<std::string, Sum>> layers(m_sums.begin(), m_sums.end());
    std::stable_sort(layers.begin(), layers.end(),
                     [](const auto& a, const auto& b) { return a.second.gpu > b.second.gpu; });
    const double wall = m_wallFrames != 0 ? m_wall / static_cast<double>(m_wallFrames) : 0.0;
    std::string out = std::format("profile: {} frames, a frame {:.3f} ms from start to start ({:.0f} a second), "
                                  "{:.3f} ms on the GPU and {:.3f} ms on the CPU from its start to its present",
                                  m_counted, wall, wall > 0.0 ? 1000.0 / wall : 0.0, m_gpuFrames / n, m_cpuFrames / n);
    for (const auto& [name, s] : layers) {
        out += std::format("\n  {:<28} GPU {:7.3f} ms  CPU {:7.3f} ms  {:6.0f} draws {:8.0f} triangles {:.1f} a frame",
                           name, s.gpu / n, s.cpu / n, static_cast<double>(s.draws) / n,
                           static_cast<double>(s.primitives) / n, static_cast<double>(s.marks) / n);
    }
    return out;
}

}  // namespace oj::render::d3d9
