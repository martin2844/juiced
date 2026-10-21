#pragma once
#include "windows.h"
#define WAVE_FORMAT_PCM 1
#pragma pack(push, 1)
struct tWAVEFORMATEX {
    WORD wFormatTag, nChannels;
    DWORD nSamplesPerSec, nAvgBytesPerSec;
    WORD nBlockAlign, wBitsPerSample, cbSize;
};
#pragma pack(pop)
using WAVEFORMATEX = tWAVEFORMATEX;
