// Verify the actual linked FFmpeg build, not just the intended Nix options.
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}
#include <iostream>
#include <string_view>

int main() {
    struct Library {const char* name;const char* license;const char* config;};
    const Library libraries[]={{"avcodec",avcodec_license(),avcodec_configuration()},
                               {"avformat",avformat_license(),avformat_configuration()},
                               {"avutil",avutil_license(),avutil_configuration()},
                               {"swresample",swresample_license(),swresample_configuration()}};
    bool valid=true;
    for(const auto& lib:libraries) {
        const std::string_view license=lib.license,config=lib.config;
        const bool ok=license=="LGPL version 2.1 or later"&&
            config.find("--enable-gpl")==config.npos&&config.find("--enable-nonfree")==config.npos;
        std::cout<<(ok?"PASS ":"FAIL ")<<lib.name<<": "<<license<<'\n';valid&=ok;
    }
    for(const auto codec:{AV_CODEC_ID_WMAV2,AV_CODEC_ID_MP3,AV_CODEC_ID_PCM_S16LE}) {
        const bool found=avcodec_find_decoder(codec)!=nullptr;
        std::cout<<(found?"PASS ":"FAIL ")<<"audio decoder "<<avcodec_get_name(codec)<<'\n';valid&=found;
    }
    for(const char* name:{"asf","mp3","wav"}) {
        const bool found=av_find_input_format(name)!=nullptr;
        std::cout<<(found?"PASS ":"FAIL ")<<"audio input "<<name<<'\n';valid&=found;
    }
    return valid?0:1;
}
