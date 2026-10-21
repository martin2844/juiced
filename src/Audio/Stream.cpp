#include "Audio/Stream.h"

#include "Audio/SoundSystem.h"
#include "Core/Base/Log.h"

#include <windows.h>
#include <dshow.h>

#include <cmath>
#include <vector>

namespace oj::audio {

namespace {

std::vector<Stream*>& streams() {
    static std::vector<Stream*> s;
    return s;
}

template <class T>
void release(T*& p) {
    if (p != nullptr) p->Release();
    p = nullptr;
}

}  // namespace

Stream::Stream(const std::filesystem::path& file) {
    streams().push_back(this);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_IGraphBuilder,
                                reinterpret_cast<void**>(&m_graph)))) {
        log::warning("sound: no filter graph for {}", file.string());
        return;
    }
    if (FAILED(m_graph->QueryInterface(IID_IMediaControl, reinterpret_cast<void**>(&m_control))) ||
        FAILED(m_graph->QueryInterface(IID_IMediaSeeking, reinterpret_cast<void**>(&m_seeking))) ||
        FAILED(m_graph->QueryInterface(IID_IBasicAudio, reinterpret_cast<void**>(&m_audio)))) {
        return;
    }
    IBaseFilter* source = nullptr;
    const std::wstring name = file.wstring();
    if (SUCCEEDED(m_graph->AddSourceFilter(name.c_str(), name.c_str(), &source))) {
        IPin* pin = nullptr;
        if (FAILED(source->FindPin(L"Output", &pin)) && FAILED(source->FindPin(L"Raw Audio 0", &pin))) pin = nullptr;
        if (pin != nullptr) {
            if (SUCCEEDED(m_control->Stop())) {
                if (FAILED(m_graph->Render(pin))) log::warning("sound: {} not rendered", file.string());
            }
            pin->Release();
        } else {
            log::warning("sound: no pin of {}", file.string());
        }
        source->Release();
    } else {
        log::warning("sound: {} not opened", file.string());
    }
    if (SUCCEEDED(m_seeking->SetTimeFormat(&TIME_FORMAT_MEDIA_TIME))) {
        LONGLONG zero = 0;
        m_seeking->SetPositions(&zero, AM_SEEKING_AbsolutePositioning, &zero, AM_SEEKING_NoPositioning);
    }
    m_state = 1;
}

Stream::~Stream() {
    if (m_control != nullptr) m_control->Stop();
    release(m_audio);
    release(m_seeking);
    release(m_control);
    release(m_graph);
    auto& all = streams();
    for (size_t i = 0; i < all.size(); ++i) {
        if (all[i] == this) {
            all.erase(all.begin() + static_cast<ptrdiff_t>(i));
            break;
        }
    }
}

void Stream::stop() {
    if (m_control != nullptr) m_control->Stop();
    m_state = 1;
}

void Stream::play(float start, float length) {
    constexpr int32_t units = 10000;  // [0x00725844]
    // fild, fmul, fistp with the control word cut: 10000 x start in extended precision, cut.
    const int32_t a = static_cast<int32_t>(static_cast<double>(units) * static_cast<double>(start));
    const int64_t scale = 10000000 / units;
    LONGLONG current = static_cast<int64_t>(a) * scale;
    const int64_t b = static_cast<int64_t>(static_cast<double>(units) * static_cast<double>(length));
    LONGLONG stopAt = (static_cast<int64_t>(static_cast<int32_t>(b) + a)) * scale;
    if (m_seeking != nullptr &&
        SUCCEEDED(m_seeking->SetPositions(&current, AM_SEEKING_AbsolutePositioning | AM_SEEKING_ReturnTime, &stopAt,
                                          AM_SEEKING_AbsolutePositioning))) {
        if (m_control != nullptr) m_control->Run();
        m_stop = stopAt;
        m_stopAsked = true;
    }
    m_state = 3;
}

void Stream::setVolume(int32_t mb) {
    m_volume = mb;
    if (m_audio != nullptr) m_audio->put_Volume(mb);
}

void Stream::pause(int paused) {
    if (paused == 1) {
        if (m_audio != nullptr) m_audio->put_Volume(-10000);
        if (m_control != nullptr) m_control->Stop();
        m_state = 2;
        return;
    }
    if (!active()) return;
    if (m_control != nullptr) m_control->Run();
    if (m_audio != nullptr) m_audio->put_Volume(m_volume);
    m_state = 3;
}

int Stream::state() {
    if (m_seeking == nullptr) return m_state;
    LONGLONG current = 0;
    LONGLONG end = 0;
    m_seeking->GetPositions(&current, &end);
    if (current == end) m_state = 1;
    if (current >= m_stop) m_state = 1;
    return m_state;
}

void Stream::focus(bool active) {
    for (Stream* s : streams()) {
        if (!active) {
            if (s->m_state == 3) {
                s->m_focusPaused = true;
                s->pause(1);
            }
        } else if (s->m_state == 2 && s->m_focusPaused) {
            s->m_focusPaused = false;
            s->pause(0);
        }
    }
}

}  // namespace oj::audio
