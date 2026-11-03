#pragma once
#include <bit>
#include <cstdint>
namespace oj::render {
inline uint16_t nativeHalf(float value) {
    const auto u=std::bit_cast<uint32_t>(value);
    const uint32_t sign=(u>>16)&0x8000u;
    const int exponent=int((u>>23)&255)-127+15;
    const uint32_t mantissa=u&0x7fffffu;
    if(exponent>=31) return uint16_t(sign|0x7c00u|(((u>>23)&255u)==255u && mantissa?0x200u:0));
    if(exponent<=0) {
        if(exponent < -10) return uint16_t(sign);
        const uint32_t m=mantissa|0x800000u;const int shift=14-exponent;
        const uint32_t round=(1u<<(shift-1))-1+((m>>shift)&1u);
        return uint16_t(sign|((m+round)>>shift));
    }
    const auto rounded=mantissa+0xfffu+((mantissa>>13)&1u);
    return uint16_t(sign+(uint32_t(exponent)<<10)+(rounded>>13));
}
inline float nativeFloat(uint16_t h) {
    const uint32_t sign=uint32_t(h&0x8000u)<<16;
    const uint32_t e=(h>>10)&31u,m=h&1023u;
    if(e==31) return std::bit_cast<float>(sign|0x7f800000u|(m<<13));
    if(e) return std::bit_cast<float>(sign|((e+112u)<<23)|(m<<13));
    if(!m) return std::bit_cast<float>(sign);
    uint32_t mm=m;int ee=-14;
    while((mm&1024u)==0) {mm<<=1;--ee;}
    return std::bit_cast<float>(sign|(uint32_t(ee+127)<<23)|((mm&1023u)<<13));
}
}
