#include "Audio/SoundSystem.h"
#include "Audio/Stream.h"
#include "Core/Base/Log.h"
#include <cmath>
#include <dsound.h>
#include <map>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}
namespace oj::audio {
namespace {
std::vector<float> decodeSegment(const std::filesystem::path &path, double start, double duration) {
    std::vector<float> out;
    AVFormatContext *format = nullptr;
    if (avformat_open_input(&format, nativePath(path).c_str(), nullptr, nullptr) < 0)
        return out;
    AVCodecContext *codec = nullptr;
    SwrContext *resample = nullptr;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    auto cleanup = [&] {
        av_frame_free(&frame);
        av_packet_free(&packet);
        swr_free(&resample);
        avcodec_free_context(&codec);
        avformat_close_input(&format);
    };
    if (avformat_find_stream_info(format, nullptr) < 0) {
        cleanup();
        return out;
    }
    int index = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (index < 0) {
        cleanup();
        return out;
    }
    auto stream = format->streams[index];
    const AVCodec *decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    codec = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(codec, stream->codecpar);
    if (avcodec_open2(codec, decoder, nullptr) < 0) {
        cleanup();
        return out;
    }
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    swr_alloc_set_opts2(&resample, &stereo, AV_SAMPLE_FMT_FLT, 48000, &codec->ch_layout, codec->sample_fmt,
                        codec->sample_rate, 0, nullptr);
    if (swr_init(resample) < 0) {
        cleanup();
        return out;
    }
    av_seek_frame(format, index, (int64_t)(start / av_q2d(stream->time_base)), AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(codec);
    const size_t limit = (size_t)(std::clamp(duration, 0., 600.) * 48000) * 2;
    bool done = false;
    while (!done && out.size() < limit && av_read_frame(format, packet) >= 0) {
        if (packet->stream_index == index && avcodec_send_packet(codec, packet) >= 0) {
            while (avcodec_receive_frame(codec, frame) >= 0) {
                double when = frame->best_effort_timestamp == AV_NOPTS_VALUE
                                  ? start
                                  : frame->best_effort_timestamp * av_q2d(stream->time_base);
                int capacity = swr_get_out_samples(resample, frame->nb_samples);
                std::vector<float> pcm(capacity * 2);
                uint8_t *dst = (uint8_t *)pcm.data();
                int n =
                    swr_convert(resample, &dst, capacity, (const uint8_t **)frame->extended_data, frame->nb_samples);
                size_t skip = (size_t)std::clamp((start - when) * 48000, 0., (double)std::max(n, 0)) * 2;
                if (n > 0) {
                    size_t take = std::min((size_t)n * 2 - skip, limit - out.size());
                    out.insert(out.end(), pcm.begin() + skip, pcm.begin() + skip + take);
                }
                if (when >= start + duration) {
                    done = true;
                    break;
                }
            }
        }
        av_packet_unref(packet);
    }
    cleanup();
    return out;
}
} // namespace
namespace {
class NativeFileStream final : public OutputStream {
    std::filesystem::path file;
    IDirectSoundBuffer* buffer=nullptr;
    int64_t from=0,to=0;
    int32_t volume=0;
public:
    explicit NativeFileStream(std::filesystem::path p):file(std::move(p)) {}
    ~NativeFileStream() override { if(buffer) { buffer->Stop();buffer->Release(); } }
    void stop() override { if(buffer) buffer->Stop(); }
    void run() override { if(buffer) buffer->Play(0,0,0); }
    bool setPositions(int64_t current,int64_t end) override {
        if(current<0 || end<=current) return false;
        stop();if(buffer) { buffer->Release();buffer=nullptr; }
        auto pcm=decodeSegment(file,double(current)/1e7,double(end-current)/1e7);
        if(pcm.empty()) { log::warning("audio stream could not decode {}",file.string());return false; }
        WAVEFORMATEX fmt{1,2,48000,192000,4,16,0};DSBUFFERDESC d{};d.lpwfxFormat=&fmt;
        IDirectSound8 device;
        if(FAILED(device.CreateSoundBuffer(&d,&buffer,nullptr))) return false;
        buffer->samples=std::make_shared<std::vector<float>>(std::move(pcm));
        buffer->SetVolume(volume);from=current;to=end;return true;
    }
    bool seekable() const override { return true; }
    void positions(int64_t& current,int64_t& end) override {
        DWORD play=0,write=0;if(buffer) buffer->GetCurrentPosition(&play,&write);
        current=from+int64_t(play)*10000000/192000;end=to;
        if(buffer && buffer->samples && play>=buffer->samples->size()*2) current=end;
    }
    void setVolume(int32_t mb) override { volume=mb;if(buffer) buffer->SetVolume(mb); }
};
}
std::unique_ptr<OutputStream> openNativeStream(const std::filesystem::path& file) {
    return std::make_unique<NativeFileStream>(file);
}
} // namespace oj::audio
