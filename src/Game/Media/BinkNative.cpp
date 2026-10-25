#include "Game/Media/BinkNative.h"

#include "Core/Base/Log.h"

#include <windows.h>

#include <mmsystem.h>

#include <dsound.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <complex>
#include <condition_variable>
#include <cstring>
#include <numbers>

namespace oj::bink {

namespace {

// The format's tables, as binkw32 has them: the 16 Huffman trees (code lengths, then codes read from the lowest bit),
// the order of a block's DCT coefficients, the 16 scan orders of the run blocks and the intra and inter quantizers
// (16 levels, 64 coefficients each in the block's order, 11 bits of fraction).
constexpr uint8_t kTreeLengths[16][16] = {
    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4},
    {1, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
    {2, 2, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
    {2, 3, 3, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
    {3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5},
    {3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5},
    {2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5},
    {1, 3, 3, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
    {1, 2, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
    {1, 3, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6},
    {2, 2, 3, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6},
    {1, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6},
    {2, 2, 2, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
    {1, 3, 3, 3, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7},
    {1, 3, 3, 3, 5, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7},
    {2, 2, 3, 3, 3, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7},
};
constexpr uint8_t kTreeCodes[16][16] = {
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
    {0x00, 0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F, 0x13, 0x15, 0x17, 0x19, 0x1B, 0x1D, 0x1F},
    {0x00, 0x02, 0x01, 0x09, 0x05, 0x15, 0x0D, 0x1D, 0x03, 0x13, 0x0B, 0x1B, 0x07, 0x17, 0x0F, 0x1F},
    {0x00, 0x02, 0x06, 0x01, 0x09, 0x05, 0x0D, 0x1D, 0x03, 0x13, 0x0B, 0x1B, 0x07, 0x17, 0x0F, 0x1F},
    {0x00, 0x04, 0x02, 0x06, 0x01, 0x09, 0x05, 0x0D, 0x03, 0x13, 0x0B, 0x1B, 0x07, 0x17, 0x0F, 0x1F},
    {0x00, 0x04, 0x02, 0x0A, 0x06, 0x0E, 0x01, 0x09, 0x05, 0x0D, 0x03, 0x0B, 0x07, 0x17, 0x0F, 0x1F},
    {0x00, 0x02, 0x0A, 0x06, 0x0E, 0x01, 0x09, 0x05, 0x0D, 0x03, 0x0B, 0x1B, 0x07, 0x17, 0x0F, 0x1F},
    {0x00, 0x01, 0x05, 0x03, 0x13, 0x0B, 0x1B, 0x3B, 0x07, 0x27, 0x17, 0x37, 0x0F, 0x2F, 0x1F, 0x3F},
    {0x00, 0x01, 0x03, 0x13, 0x0B, 0x2B, 0x1B, 0x3B, 0x07, 0x27, 0x17, 0x37, 0x0F, 0x2F, 0x1F, 0x3F},
    {0x00, 0x01, 0x05, 0x0D, 0x03, 0x13, 0x0B, 0x1B, 0x07, 0x27, 0x17, 0x37, 0x0F, 0x2F, 0x1F, 0x3F},
    {0x00, 0x02, 0x01, 0x05, 0x0D, 0x03, 0x13, 0x0B, 0x1B, 0x07, 0x17, 0x37, 0x0F, 0x2F, 0x1F, 0x3F},
    {0x00, 0x01, 0x09, 0x05, 0x0D, 0x03, 0x13, 0x0B, 0x1B, 0x07, 0x17, 0x37, 0x0F, 0x2F, 0x1F, 0x3F},
    {0x00, 0x02, 0x01, 0x03, 0x13, 0x0B, 0x1B, 0x3B, 0x07, 0x27, 0x17, 0x37, 0x0F, 0x2F, 0x1F, 0x3F},
    {0x00, 0x01, 0x05, 0x03, 0x07, 0x27, 0x17, 0x37, 0x0F, 0x4F, 0x2F, 0x6F, 0x1F, 0x5F, 0x3F, 0x7F},
    {0x00, 0x01, 0x05, 0x03, 0x07, 0x17, 0x37, 0x77, 0x0F, 0x4F, 0x2F, 0x6F, 0x1F, 0x5F, 0x3F, 0x7F},
    {0x00, 0x02, 0x01, 0x05, 0x03, 0x07, 0x27, 0x17, 0x37, 0x0F, 0x2F, 0x6F, 0x1F, 0x5F, 0x3F, 0x7F},
};
constexpr uint8_t kScan[64] = {
    0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15,
    20, 21, 28, 29, 22, 23, 30, 31, 16, 17, 24, 25, 32, 33, 40, 41,
    34, 35, 42, 43, 48, 49, 56, 57, 50, 51, 58, 59, 18, 19, 26, 27,
    36, 37, 44, 45, 38, 39, 46, 47, 52, 53, 60, 61, 54, 55, 62, 63,
};
constexpr uint8_t kPatterns[16][64] = {
    {
        0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x39, 0x31, 0x29, 0x21, 0x19, 0x11, 0x09, 0x01,
        0x02, 0x0A, 0x12, 0x1A, 0x22, 0x2A, 0x32, 0x3A, 0x3B, 0x33, 0x2B, 0x23, 0x1B, 0x13, 0x0B, 0x03,
        0x04, 0x0C, 0x14, 0x1C, 0x24, 0x2C, 0x34, 0x3C, 0x3D, 0x35, 0x2D, 0x25, 0x1D, 0x15, 0x0D, 0x05,
        0x06, 0x0E, 0x16, 0x1E, 0x26, 0x2E, 0x36, 0x3E, 0x3F, 0x37, 0x2F, 0x27, 0x1F, 0x17, 0x0F, 0x07,
    },
    {
        0x3B, 0x3A, 0x39, 0x38, 0x30, 0x31, 0x32, 0x33, 0x2B, 0x2A, 0x29, 0x28, 0x20, 0x21, 0x22, 0x23,
        0x1B, 0x1A, 0x19, 0x18, 0x10, 0x11, 0x12, 0x13, 0x0B, 0x0A, 0x09, 0x08, 0x00, 0x01, 0x02, 0x03,
        0x04, 0x05, 0x06, 0x07, 0x0F, 0x0E, 0x0D, 0x0C, 0x14, 0x15, 0x16, 0x17, 0x1F, 0x1E, 0x1D, 0x1C,
        0x24, 0x25, 0x26, 0x27, 0x2F, 0x2E, 0x2D, 0x2C, 0x34, 0x35, 0x36, 0x37, 0x3F, 0x3E, 0x3D, 0x3C,
    },
    {
        0x19, 0x11, 0x12, 0x1A, 0x1B, 0x13, 0x0B, 0x03, 0x02, 0x0A, 0x09, 0x01, 0x00, 0x08, 0x10, 0x18,
        0x20, 0x28, 0x30, 0x38, 0x39, 0x31, 0x29, 0x2A, 0x32, 0x3A, 0x3B, 0x33, 0x2B, 0x23, 0x22, 0x21,
        0x1D, 0x15, 0x16, 0x1E, 0x1F, 0x17, 0x0F, 0x07, 0x06, 0x0E, 0x0D, 0x05, 0x04, 0x0C, 0x14, 0x1C,
        0x24, 0x2C, 0x34, 0x3C, 0x3D, 0x35, 0x2D, 0x2E, 0x36, 0x3E, 0x3F, 0x37, 0x2F, 0x27, 0x26, 0x25,
    },
    {
        0x03, 0x0B, 0x02, 0x0A, 0x01, 0x09, 0x00, 0x08, 0x10, 0x18, 0x11, 0x19, 0x12, 0x1A, 0x13, 0x1B,
        0x23, 0x2B, 0x22, 0x2A, 0x21, 0x29, 0x20, 0x28, 0x30, 0x38, 0x31, 0x39, 0x32, 0x3A, 0x33, 0x3B,
        0x3C, 0x34, 0x3D, 0x35, 0x3E, 0x36, 0x3F, 0x37, 0x2F, 0x27, 0x2E, 0x26, 0x2D, 0x25, 0x2C, 0x24,
        0x1C, 0x14, 0x1D, 0x15, 0x1E, 0x16, 0x1F, 0x17, 0x0F, 0x07, 0x0E, 0x06, 0x0D, 0x05, 0x0C, 0x04,
    },
    {
        0x18, 0x19, 0x10, 0x11, 0x08, 0x09, 0x00, 0x01, 0x02, 0x03, 0x0A, 0x0B, 0x12, 0x13, 0x1A, 0x1B,
        0x1C, 0x1D, 0x14, 0x15, 0x0C, 0x0D, 0x04, 0x05, 0x06, 0x07, 0x0E, 0x0F, 0x16, 0x17, 0x1E, 0x1F,
        0x27, 0x26, 0x2F, 0x2E, 0x37, 0x36, 0x3F, 0x3E, 0x3D, 0x3C, 0x35, 0x34, 0x2D, 0x2C, 0x25, 0x24,
        0x23, 0x22, 0x2B, 0x2A, 0x33, 0x32, 0x3B, 0x3A, 0x39, 0x38, 0x31, 0x30, 0x29, 0x28, 0x21, 0x20,
    },
    {
        0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0A, 0x0B, 0x10, 0x11, 0x12, 0x13, 0x18, 0x19, 0x1A, 0x1B,
        0x20, 0x21, 0x22, 0x23, 0x28, 0x29, 0x2A, 0x2B, 0x30, 0x31, 0x32, 0x33, 0x38, 0x39, 0x3A, 0x3B,
        0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F, 0x14, 0x15, 0x16, 0x17, 0x1C, 0x1D, 0x1E, 0x1F,
        0x24, 0x25, 0x26, 0x27, 0x2C, 0x2D, 0x2E, 0x2F, 0x34, 0x35, 0x36, 0x37, 0x3C, 0x3D, 0x3E, 0x3F,
    },
    {
        0x06, 0x07, 0x0F, 0x0E, 0x0D, 0x05, 0x0C, 0x04, 0x03, 0x0B, 0x02, 0x0A, 0x09, 0x01, 0x00, 0x08,
        0x10, 0x18, 0x11, 0x19, 0x12, 0x1A, 0x13, 0x1B, 0x14, 0x1C, 0x15, 0x1D, 0x16, 0x1E, 0x17, 0x1F,
        0x27, 0x2F, 0x26, 0x2E, 0x25, 0x2D, 0x24, 0x2C, 0x23, 0x2B, 0x22, 0x2A, 0x21, 0x29, 0x20, 0x28,
        0x31, 0x30, 0x38, 0x39, 0x3A, 0x32, 0x3B, 0x33, 0x3C, 0x34, 0x3D, 0x35, 0x36, 0x37, 0x3F, 0x3E,
    },
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2F, 0x2E, 0x2D, 0x2C, 0x2B, 0x2A, 0x29, 0x28,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x3F, 0x3E, 0x3D, 0x3C, 0x3B, 0x3A, 0x39, 0x38,
    },
    {
        0x00, 0x08, 0x09, 0x01, 0x02, 0x03, 0x0B, 0x0A, 0x12, 0x13, 0x1B, 0x1A, 0x19, 0x11, 0x10, 0x18,
        0x20, 0x28, 0x29, 0x21, 0x22, 0x23, 0x2B, 0x2A, 0x32, 0x31, 0x30, 0x38, 0x39, 0x3A, 0x3B, 0x33,
        0x34, 0x3C, 0x3D, 0x3E, 0x3F, 0x37, 0x36, 0x35, 0x2D, 0x2C, 0x24, 0x25, 0x26, 0x2E, 0x2F, 0x27,
        0x1F, 0x17, 0x16, 0x1E, 0x1D, 0x1C, 0x14, 0x15, 0x0D, 0x0C, 0x04, 0x05, 0x06, 0x0E, 0x0F, 0x07,
    },
    {
        0x18, 0x19, 0x10, 0x11, 0x08, 0x09, 0x00, 0x01, 0x02, 0x03, 0x0A, 0x0B, 0x12, 0x13, 0x1A, 0x1B,
        0x1C, 0x1D, 0x14, 0x15, 0x0C, 0x0D, 0x04, 0x05, 0x06, 0x07, 0x0E, 0x0F, 0x16, 0x17, 0x1E, 0x1F,
        0x26, 0x27, 0x2E, 0x2F, 0x36, 0x37, 0x3E, 0x3F, 0x3C, 0x3D, 0x34, 0x35, 0x2C, 0x2D, 0x24, 0x25,
        0x22, 0x23, 0x2A, 0x2B, 0x32, 0x33, 0x3A, 0x3B, 0x38, 0x39, 0x30, 0x31, 0x28, 0x29, 0x20, 0x21,
    },
    {
        0x00, 0x08, 0x01, 0x09, 0x02, 0x0A, 0x03, 0x0B, 0x13, 0x1B, 0x12, 0x1A, 0x11, 0x19, 0x10, 0x18,
        0x20, 0x28, 0x21, 0x29, 0x22, 0x2A, 0x23, 0x2B, 0x33, 0x3B, 0x32, 0x3A, 0x31, 0x39, 0x30, 0x38,
        0x3C, 0x34, 0x3D, 0x35, 0x3E, 0x36, 0x3F, 0x37, 0x2F, 0x27, 0x2E, 0x26, 0x2D, 0x25, 0x2C, 0x24,
        0x1F, 0x17, 0x1E, 0x16, 0x1D, 0x15, 0x1C, 0x14, 0x0C, 0x04, 0x0D, 0x05, 0x0E, 0x06, 0x0F, 0x07,
    },
    {
        0x00, 0x08, 0x10, 0x18, 0x19, 0x1A, 0x1B, 0x13, 0x0B, 0x03, 0x02, 0x01, 0x09, 0x11, 0x12, 0x0A,
        0x04, 0x0C, 0x14, 0x1C, 0x1D, 0x1E, 0x1F, 0x17, 0x0F, 0x07, 0x06, 0x05, 0x0D, 0x15, 0x16, 0x0E,
        0x24, 0x2C, 0x34, 0x3C, 0x3D, 0x3E, 0x3F, 0x37, 0x2F, 0x27, 0x26, 0x25, 0x2D, 0x35, 0x36, 0x2E,
        0x20, 0x28, 0x30, 0x38, 0x39, 0x3A, 0x3B, 0x33, 0x2B, 0x23, 0x22, 0x21, 0x29, 0x31, 0x32, 0x2A,
    },
    {
        0x00, 0x08, 0x09, 0x01, 0x02, 0x03, 0x0B, 0x0A, 0x13, 0x1B, 0x1A, 0x12, 0x11, 0x10, 0x18, 0x19,
        0x21, 0x20, 0x28, 0x29, 0x2A, 0x22, 0x23, 0x2B, 0x33, 0x3B, 0x3A, 0x32, 0x31, 0x39, 0x38, 0x30,
        0x34, 0x3C, 0x3D, 0x35, 0x36, 0x3E, 0x3F, 0x37, 0x2F, 0x27, 0x26, 0x2E, 0x2D, 0x2C, 0x24, 0x25,
        0x1D, 0x1C, 0x14, 0x15, 0x16, 0x1E, 0x1F, 0x17, 0x0E, 0x0F, 0x07, 0x06, 0x05, 0x0D, 0x0C, 0x04,
    },
    {
        0x18, 0x10, 0x08, 0x00, 0x01, 0x02, 0x03, 0x0B, 0x13, 0x1B, 0x1A, 0x19, 0x11, 0x0A, 0x09, 0x12,
        0x1C, 0x14, 0x0C, 0x04, 0x05, 0x06, 0x07, 0x0F, 0x17, 0x1F, 0x1E, 0x1D, 0x15, 0x0E, 0x0D, 0x16,
        0x3C, 0x34, 0x2C, 0x24, 0x25, 0x26, 0x27, 0x2F, 0x37, 0x3F, 0x3E, 0x3D, 0x35, 0x2E, 0x2D, 0x36,
        0x38, 0x30, 0x28, 0x20, 0x21, 0x22, 0x23, 0x2B, 0x33, 0x3B, 0x3A, 0x39, 0x31, 0x2A, 0x29, 0x32,
    },
    {
        0x00, 0x08, 0x09, 0x01, 0x02, 0x0A, 0x12, 0x11, 0x10, 0x18, 0x19, 0x1A, 0x1B, 0x13, 0x0B, 0x03,
        0x07, 0x06, 0x0E, 0x0F, 0x17, 0x16, 0x15, 0x0D, 0x05, 0x04, 0x0C, 0x14, 0x1C, 0x1D, 0x1E, 0x1F,
        0x3F, 0x3E, 0x36, 0x37, 0x2F, 0x2E, 0x2D, 0x35, 0x3D, 0x3C, 0x34, 0x2C, 0x24, 0x25, 0x26, 0x27,
        0x38, 0x30, 0x31, 0x39, 0x3A, 0x32, 0x2A, 0x29, 0x28, 0x20, 0x21, 0x22, 0x23, 0x2B, 0x33, 0x3B,
    },
    {
        0x00, 0x01, 0x08, 0x09, 0x10, 0x11, 0x18, 0x19, 0x20, 0x21, 0x28, 0x29, 0x30, 0x31, 0x38, 0x39,
        0x3A, 0x3B, 0x32, 0x33, 0x2A, 0x2B, 0x22, 0x23, 0x1A, 0x1B, 0x12, 0x13, 0x0A, 0x0B, 0x02, 0x03,
        0x04, 0x05, 0x0C, 0x0D, 0x14, 0x15, 0x1C, 0x1D, 0x24, 0x25, 0x2C, 0x2D, 0x34, 0x35, 0x3C, 0x3D,
        0x3E, 0x3F, 0x36, 0x37, 0x2E, 0x2F, 0x26, 0x27, 0x1E, 0x1F, 0x16, 0x17, 0x0E, 0x0F, 0x06, 0x07,
    },
};
constexpr int32_t kIntraQuant[16][64] = {
    {
        0x010000, 0x016315, 0x014E7B, 0x016577, 0x010000, 0x00EEDA, 0x00BE80, 0x00611E,
        0x01E83D, 0x02A535, 0x02F1E6, 0x02724C, 0x024102, 0x017F9B, 0x01083C, 0x00A552,
        0x02346F, 0x030EE5, 0x02C628, 0x027F20, 0x021F88, 0x01DC53, 0x014819, 0x00A743,
        0x01FBFA, 0x02C096, 0x0297B5, 0x023F32, 0x027FAD, 0x01F697, 0x015A31, 0x009688,
        0x01D000, 0x028396, 0x02346F, 0x01FBFA, 0x025000, 0x01AB6B, 0x012669, 0x008D43,
        0x019247, 0x01F9AA, 0x01DC53, 0x0231B8, 0x01D122, 0x0159B3, 0x00EE1F, 0x0075ED,
        0x012F12, 0x01E06C, 0x01C48C, 0x019748, 0x01490C, 0x010288, 0x00E0F1, 0x0072AD,
        0x00CB10, 0x0119A8, 0x014E86, 0x0122AF, 0x00F735, 0x00EF51, 0x00A4D8, 0x006517,
    },
    {
        0x015555, 0x01D971, 0x01BDF9, 0x01DC9F, 0x015555, 0x013E78, 0x00FE00, 0x00817D,
        0x028AFC, 0x0386F1, 0x03ED33, 0x034311, 0x030158, 0x01FF7A, 0x01604F, 0x00DC6D,
        0x02F095, 0x0413DC, 0x03B2E0, 0x03542A, 0x02D4B5, 0x027B19, 0x01B577, 0x00DF04,
        0x02A54E, 0x03AB73, 0x0374F1, 0x02FEEE, 0x0354E7, 0x029E1F, 0x01CD96, 0x00C8B6,
        0x026AAB, 0x035A1E, 0x02F095, 0x02A54E, 0x031555, 0x0239E4, 0x01888C, 0x00BC59,
        0x02185E, 0x02A238, 0x027B19, 0x02ECF5, 0x026C2D, 0x01CCEE, 0x013D7E, 0x009D3C,
        0x019418, 0x028090, 0x025B66, 0x021F0B, 0x01B6BB, 0x0158B5, 0x012BEC, 0x0098E6,
        0x010EC0, 0x01778A, 0x01BE09, 0x018394, 0x01499C, 0x013F17, 0x00DBCB, 0x0086C9,
    },
    {
        0x01AAAB, 0x024FCE, 0x022D78, 0x0253C7, 0x01AAAB, 0x018E16, 0x013D80, 0x00A1DC,
        0x032DBB, 0x0468AD, 0x04E87F, 0x0413D5, 0x03C1AE, 0x027F58, 0x01B863, 0x011388,
        0x03ACBA, 0x0518D3, 0x049F98, 0x042935, 0x0389E2, 0x0319DF, 0x0222D4, 0x0116C5,
        0x034EA1, 0x04964F, 0x04522D, 0x03BEA9, 0x042A21, 0x0345A7, 0x0240FC, 0x00FAE3,
        0x030555, 0x0430A5, 0x03ACBA, 0x034EA1, 0x03DAAB, 0x02C85D, 0x01EAAF, 0x00EB6F,
        0x029E76, 0x034AC5, 0x0319DF, 0x03A833, 0x030738, 0x02402A, 0x018CDE, 0x00C48A,
        0x01F91E, 0x0320B4, 0x02F23F, 0x02A6CE, 0x022469, 0x01AEE2, 0x0176E7, 0x00BF20,
        0x015270, 0x01D56D, 0x022D8B, 0x01E479, 0x019C02, 0x018EDD, 0x0112BE, 0x00A87B,
    },
    {
        0x020000, 0x02C62A, 0x029CF6, 0x02CAEF, 0x020000, 0x01DDB4, 0x017D01, 0x00C23C,
        0x03D07A, 0x054A69, 0x05E3CC, 0x04E499, 0x048204, 0x02FF36, 0x021077, 0x014AA3,
        0x0468DF, 0x061DCA, 0x058C50, 0x04FE3F, 0x043F0F, 0x03B8A6, 0x029032, 0x014E86,
        0x03F7F5, 0x05812C, 0x052F69, 0x047E65, 0x04FF5A, 0x03ED2E, 0x02B461, 0x012D11,
        0x03A000, 0x05072C, 0x0468DF, 0x03F7F5, 0x04A000, 0x0356D6, 0x024CD2, 0x011A85,
        0x03248D, 0x03F353, 0x03B8A6, 0x046370, 0x03A243, 0x02B365, 0x01DC3E, 0x00EBD9,
        0x025E24, 0x03C0D8, 0x038919, 0x032E91, 0x029218, 0x020510, 0x01C1E2, 0x00E559,
        0x019620, 0x02334F, 0x029D0D, 0x02455E, 0x01EE69, 0x01DEA2, 0x0149B0, 0x00CA2D,
    },
    {
        0x02AAAB, 0x03B2E3, 0x037BF2, 0x03B93E, 0x02AAAB, 0x027CF0, 0x01FC01, 0x0102FA,
        0x0515F8, 0x070DE2, 0x07DA65, 0x068621, 0x0602B1, 0x03FEF3, 0x02C09F, 0x01B8DA,
        0x05E129, 0x0827B8, 0x0765C0, 0x06A855, 0x05A96A, 0x04F632, 0x036AED, 0x01BE09,
        0x054A9C, 0x0756E5, 0x06E9E2, 0x05FDDB, 0x06A9CE, 0x053C3E, 0x039B2D, 0x01916B,
        0x04D555, 0x06B43B, 0x05E129, 0x054A9C, 0x062AAB, 0x0473C8, 0x031118, 0x0178B2,
        0x0430BC, 0x05446F, 0x04F632, 0x05D9EB, 0x04D85A, 0x0399DC, 0x027AFD, 0x013A77,
        0x032830, 0x050121, 0x04B6CC, 0x043E16, 0x036D76, 0x02B16A, 0x0257D8, 0x0131CC,
        0x021D80, 0x02EF14, 0x037C11, 0x030728, 0x029337, 0x027E2E, 0x01B796, 0x010D91,
    },
    {
        0x038000, 0x04DACA, 0x0492AE, 0x04E322, 0x038000, 0x0343FB, 0x029AC1, 0x0153E8,
        0x06ACD5, 0x094238, 0x0A4EA5, 0x08900C, 0x07E388, 0x053E9F, 0x039CD0, 0x02429E,
        0x07B786, 0x0AB421, 0x09B58C, 0x08BCEF, 0x076E5B, 0x068322, 0x047C57, 0x02496B,
        0x06F1ED, 0x09A20D, 0x0912F8, 0x07DD30, 0x08BEDE, 0x06DF11, 0x04BBAB, 0x020EDD,
        0x065800, 0x08CC8E, 0x07B786, 0x06F1ED, 0x081800, 0x05D7F7, 0x040670, 0x01EE69,
        0x057FF7, 0x06E9D2, 0x068322, 0x07AE04, 0x065BF6, 0x04B9F1, 0x03416C, 0x019CBC,
        0x0424BF, 0x06917B, 0x062FEB, 0x05917D, 0x047FAA, 0x0388DC, 0x03134C, 0x01915C,
        0x02C6B8, 0x03D9CB, 0x0492D7, 0x03F964, 0x036138, 0x03459C, 0x0240F5, 0x0161CF,
    },
    {
        0x040000, 0x058C54, 0x0539EC, 0x0595DD, 0x040000, 0x03BB68, 0x02FA01, 0x018477,
        0x07A0F4, 0x0A94D3, 0x0BC798, 0x09C932, 0x090409, 0x05FE6D, 0x0420EE, 0x029547,
        0x08D1BE, 0x0C3B94, 0x0B18A0, 0x09FC7F, 0x087E1F, 0x07714C, 0x052064, 0x029D0D,
        0x07EFEA, 0x0B0258, 0x0A5ED3, 0x08FCC9, 0x09FEB5, 0x07DA5D, 0x0568C3, 0x025A21,
        0x074000, 0x0A0E59, 0x08D1BE, 0x07EFEA, 0x094000, 0x06ADAC, 0x0499A5, 0x02350B,
        0x06491A, 0x07E6A7, 0x07714C, 0x08C6E0, 0x074487, 0x0566CA, 0x03B87B, 0x01D7B3,
        0x04BC48, 0x0781B1, 0x071232, 0x065D22, 0x052430, 0x040A20, 0x0383C5, 0x01CAB3,
        0x032C3F, 0x04669F, 0x053A1A, 0x048ABC, 0x03DCD3, 0x03BD45, 0x029361, 0x01945A,
    },
    {
        0x050000, 0x06EF69, 0x068867, 0x06FB55, 0x050000, 0x04AA42, 0x03B881, 0x01E595,
        0x098931, 0x0D3A07, 0x0EB97E, 0x0C3B7E, 0x0B450B, 0x077E08, 0x05292A, 0x033A99,
        0x0B062D, 0x0F4A78, 0x0DDEC8, 0x0C7B9F, 0x0A9DA7, 0x094D9F, 0x06687D, 0x034450,
        0x09EBE4, 0x0DC2EE, 0x0CF687, 0x0B3BFB, 0x0C7E62, 0x09D0F4, 0x06C2F4, 0x02F0AA,
        0x091000, 0x0C91EF, 0x0B062D, 0x09EBE4, 0x0B9000, 0x085917, 0x05C00E, 0x02C24D,
        0x07DB61, 0x09E050, 0x094D9F, 0x0AF898, 0x0915A8, 0x06C07D, 0x04A69A, 0x024D9F,
        0x05EB59, 0x09621D, 0x08D6BE, 0x07F46A, 0x066D3C, 0x050CA7, 0x0464B6, 0x023D5F,
        0x03F74F, 0x058046, 0x0688A0, 0x05AD6B, 0x04D407, 0x04AC96, 0x033839, 0x01F971,
    },
    {
        0x060000, 0x08527E, 0x07D6E1, 0x0860CC, 0x060000, 0x05991C, 0x047702, 0x0246B3,
        0x0B716E, 0x0FDF3C, 0x11AB63, 0x0EADCB, 0x0D860D, 0x08FDA3, 0x063165, 0x03DFEA,
        0x0D3A9C, 0x12595D, 0x10A4F0, 0x0EFABE, 0x0CBD2E, 0x0B29F1, 0x07B096, 0x03EB93,
        0x0BE7DF, 0x108384, 0x0F8E3C, 0x0D7B2E, 0x0EFE0F, 0x0BC78B, 0x081D24, 0x038732,
        0x0AE000, 0x0F1585, 0x0D3A9C, 0x0BE7DF, 0x0DE000, 0x0A0482, 0x06E677, 0x034F90,
        0x096DA8, 0x0BD9FA, 0x0B29F1, 0x0D2A50, 0x0AE6CA, 0x081A2F, 0x0594B9, 0x02C38C,
        0x071A6B, 0x0B4289, 0x0A9B4A, 0x098BB2, 0x07B649, 0x060F2F, 0x0545A7, 0x02B00C,
        0x04C25F, 0x0699EE, 0x07D727, 0x06D01A, 0x05CB3C, 0x059BE7, 0x03DD11, 0x025E87,
    },
    {
        0x080000, 0x0B18A8, 0x0A73D7, 0x0B2BBB, 0x080000, 0x0776CF, 0x05F402, 0x0308EF,
        0x0F41E8, 0x1529A5, 0x178F2F, 0x139264, 0x120812, 0x0BFCD9, 0x0841DC, 0x052A8E,
        0x11A37B, 0x187727, 0x163140, 0x13F8FE, 0x10FC3E, 0x0EE297, 0x0A40C8, 0x053A1A,
        0x0FDFD4, 0x1604B0, 0x14BDA5, 0x11F992, 0x13FD69, 0x0FB4B9, 0x0AD186, 0x04B442,
        0x0E8000, 0x141CB1, 0x11A37B, 0x0FDFD4, 0x128000, 0x0D5B58, 0x093349, 0x046A15,
        0x0C9235, 0x0FCD4D, 0x0EE297, 0x118DC0, 0x0E890D, 0x0ACD94, 0x0770F7, 0x03AF65,
        0x09788F, 0x0F0362, 0x0E2463, 0x0CBA43, 0x0A4861, 0x08143F, 0x070789, 0x039565,
        0x06587F, 0x08CD3D, 0x0A7434, 0x091577, 0x07B9A6, 0x077A89, 0x0526C2, 0x0328B4,
    },
    {
        0x0C0000, 0x10A4FD, 0x0FADC3, 0x10C198, 0x0C0000, 0x0B3237, 0x08EE03, 0x048D66,
        0x16E2DB, 0x1FBE78, 0x2356C7, 0x1D5B96, 0x1B0C1A, 0x11FB46, 0x0C62CA, 0x07BFD5,
        0x1A7539, 0x24B2BB, 0x2149E1, 0x1DF57D, 0x197A5D, 0x1653E3, 0x0F612C, 0x07D727,
        0x17CFBD, 0x210709, 0x1F1C78, 0x1AF65B, 0x1DFC1E, 0x178F16, 0x103A49, 0x070E64,
        0x15C000, 0x1E2B0A, 0x1A7539, 0x17CFBD, 0x1BC000, 0x140904, 0x0DCCEE, 0x069F20,
        0x12DB4F, 0x17B3F4, 0x1653E3, 0x1A54A0, 0x15CD94, 0x10345E, 0x0B2972, 0x058718,
        0x0E34D7, 0x168513, 0x153695, 0x131765, 0x0F6C91, 0x0C1E5E, 0x0A8B4E, 0x056018,
        0x0984BE, 0x0D33DC, 0x0FAE4E, 0x0DA033, 0x0B9678, 0x0B37CE, 0x07BA22, 0x04BD0E,
    },
    {
        0x110000, 0x179466, 0x16362A, 0x17BCED, 0x110000, 0x0FDC79, 0x0CA685, 0x0672FB,
        0x206C0C, 0x2CF87F, 0x321044, 0x299714, 0x265125, 0x19794E, 0x118BF4, 0x0AFA6D,
        0x257B66, 0x33FD33, 0x2F28A9, 0x2A711B, 0x241804, 0x1FA181, 0x15C9A9, 0x0B1B77,
        0x21BBA2, 0x2EC9F7, 0x2C12FF, 0x263256, 0x2A7A80, 0x21600A, 0x16FD3C, 0x09FF0D,
        0x1ED000, 0x2ABCF9, 0x257B66, 0x21BBA2, 0x275000, 0x1C621B, 0x138CFB, 0x09616E,
        0x1AB6B0, 0x219444, 0x1FA181, 0x254D38, 0x1EE33C, 0x16F4DB, 0x0FD00C, 0x07D4B7,
        0x142030, 0x1FE730, 0x1E0D52, 0x1B0BCF, 0x15D9CE, 0x112B06, 0x0EF004, 0x079D77,
        0x0D7C0E, 0x12B423, 0x1636EE, 0x134D9E, 0x106A80, 0x0FE464, 0x0AF25B, 0x06B67F,
    },
    {
        0x160000, 0x1E83CF, 0x1CBE90, 0x1EB842, 0x160000, 0x1486BA, 0x105F06, 0x085891,
        0x29F53D, 0x3A3286, 0x40C9C2, 0x35D293, 0x319630, 0x20F756, 0x16B51E, 0x0E3506,
        0x308193, 0x4347AC, 0x3D0771, 0x36ECBA, 0x2EB5AA, 0x28EF20, 0x1C3225, 0x0E5FC7,
        0x2BA786, 0x3C8CE5, 0x390986, 0x316E52, 0x36F8E1, 0x2B30FE, 0x1DC030, 0x0CEFB7,
        0x27E000, 0x374EE7, 0x308193, 0x2BA786, 0x32E000, 0x24BB33, 0x194D09, 0x0C23BB,
        0x229212, 0x2B7494, 0x28EF20, 0x3045D0, 0x27F8E4, 0x1DB557, 0x1476A6, 0x0A2256,
        0x1A0B89, 0x29494D, 0x26E410, 0x230039, 0x1C470A, 0x1637AD, 0x1354B9, 0x09DAD6,
        0x11735D, 0x183469, 0x1CBF8F, 0x18FB09, 0x153E87, 0x1490FA, 0x0E2A94, 0x08AFF0,
    },
    {
        0x1C0000, 0x26D64D, 0x249572, 0x27190E, 0x1C0000, 0x1A1FD6, 0x14D607, 0x0A9F44,
        0x3566AA, 0x4A11C2, 0x527525, 0x44805E, 0x3F1C3E, 0x29F4F9, 0x1CE683, 0x1214F0,
        0x3DBC30, 0x55A109, 0x4DAC61, 0x45E778, 0x3B72D9, 0x341911, 0x23E2BB, 0x124B5B,
        0x378F64, 0x4D1069, 0x4897C2, 0x3EE97F, 0x45F6F0, 0x36F889, 0x25DD54, 0x1076E9,
        0x32C000, 0x46646C, 0x3DBC30, 0x378F64, 0x40C000, 0x2EBFB5, 0x203380, 0x0F734B,
        0x2BFFB9, 0x374E8E, 0x341911, 0x3D7020, 0x32DFAE, 0x25CF86, 0x1A0B5F, 0x0CE5E2,
        0x2125F5, 0x348BD6, 0x317F5B, 0x2C8BEB, 0x23FD53, 0x1C46DC, 0x189A60, 0x0C8AE2,
        0x1635BC, 0x1ECE57, 0x2496B6, 0x1FCB22, 0x1B09C4, 0x1A2CE1, 0x1207A5, 0x0B0E77,
    },
    {
        0x220000, 0x2F28CC, 0x2C6C53, 0x2F79DA, 0x220000, 0x1FB8F1, 0x194D09, 0x0CE5F7,
        0x40D818, 0x59F0FE, 0x642089, 0x532E29, 0x4CA24B, 0x32F29C, 0x2317E8, 0x15F4DB,
        0x4AF6CC, 0x67FA67, 0x5E5152, 0x54E237, 0x483007, 0x3F4303, 0x2B9351, 0x1636EE,
        0x437743, 0x5D93EE, 0x5825FE, 0x4C64AD, 0x54F4FF, 0x42C014, 0x2DFA79, 0x13FE1A,
        0x3DA000, 0x5579F1, 0x4AF6CC, 0x437743, 0x4EA000, 0x38C437, 0x2719F7, 0x12C2DB,
        0x356D61, 0x432888, 0x3F4303, 0x4A9A70, 0x3DC678, 0x2DE9B5, 0x1FA018, 0x0FA96E,
        0x284060, 0x3FCE60, 0x3C1AA5, 0x36179D, 0x2BB39B, 0x22560C, 0x1DE007, 0x0F3AEE,
        0x1AF81B, 0x256845, 0x2C6DDD, 0x269B3C, 0x20D500, 0x1FC8C8, 0x15E4B7, 0x0D6CFE,
    },
    {
        0x2C0000, 0x3D079E, 0x397D20, 0x3D7083, 0x2C0000, 0x290D75, 0x20BE0C, 0x10B121,
        0x53EA79, 0x74650C, 0x819383, 0x6BA525, 0x632C61, 0x41EEAC, 0x2D6A3B, 0x1C6A0C,
        0x610326, 0x868F57, 0x7A0EE2, 0x6DD974, 0x5D6B54, 0x51DE40, 0x38644B, 0x1CBF8F,
        0x574F0B, 0x7919CA, 0x72130C, 0x62DCA3, 0x6DF1C2, 0x5661FB, 0x3B8060, 0x19DF6D,
        0x4FC000, 0x6E9DCE, 0x610326, 0x574F0B, 0x65C000, 0x497665, 0x329A12, 0x184776,
        0x452423, 0x56E928, 0x51DE40, 0x608BA0, 0x4FF1C9, 0x3B6AAE, 0x28ED4D, 0x1444AC,
        0x341713, 0x52929A, 0x4DC821, 0x460071, 0x388E14, 0x2C6F5A, 0x26A973, 0x13B5AD,
        0x22E6BA, 0x3068D2, 0x397F1E, 0x31F611, 0x2A7D0F, 0x2921F4, 0x1C5528, 0x115FDF,
    },
};
constexpr int32_t kInterQuant[16][64] = {
    {
        0x010000, 0x017946, 0x016363, 0x0152A7, 0x012000, 0x00E248, 0x00A486, 0x0053E0,
        0x01A5A9, 0x0248DC, 0x0243EC, 0x0209EA, 0x01BBDA, 0x015CBC, 0x00F036, 0x008095,
        0x01B701, 0x0260EB, 0x023D97, 0x020437, 0x01B701, 0x016959, 0x00F8E7, 0x007EE4,
        0x019DE9, 0x023E1B, 0x021CCC, 0x01E6B4, 0x01B0B9, 0x0153FD, 0x00EA30, 0x007763,
        0x017000, 0x01FE6E, 0x01E0D1, 0x01B0B9, 0x018000, 0x012DB5, 0x00CFD2, 0x006E5C,
        0x012DB5, 0x01A27B, 0x018A33, 0x01718D, 0x0146D9, 0x0100CE, 0x00B0E4, 0x005A2D,
        0x00D87A, 0x014449, 0x013178, 0x0112EA, 0x00E9CC, 0x00B7B1, 0x008337, 0x0042E5,
        0x007B9A, 0x00AB71, 0x00AD08, 0x009BB9, 0x00846F, 0x006B85, 0x004A10, 0x002831,
    },
    {
        0x015555, 0x01F708, 0x01D9D9, 0x01C389, 0x018000, 0x012DB5, 0x00DB5D, 0x006FD5,
        0x023237, 0x030BD0, 0x03053B, 0x02B7E3, 0x024FCE, 0x01D0FA, 0x014048, 0x00AB71,
        0x024957, 0x032BE4, 0x02FCC9, 0x02B04A, 0x024957, 0x01E1CC, 0x014BDE, 0x00A92F,
        0x0227E1, 0x02FD7A, 0x02D110, 0x0288F1, 0x0240F7, 0x01C551, 0x013840, 0x009F2F,
        0x01EAAB, 0x02A893, 0x028116, 0x0240F7, 0x020000, 0x019247, 0x011518, 0x009325,
        0x019247, 0x022DF9, 0x020D99, 0x01ECBC, 0x01B3CC, 0x015668, 0x00EBDA, 0x00783D,
        0x0120A3, 0x01B061, 0x01974B, 0x016E8E, 0x0137BB, 0x00F4ED, 0x00AEF4, 0x005931,
        0x00A4CE, 0x00E497, 0x00E6B5, 0x00CFA2, 0x00B093, 0x008F5C, 0x0062BF, 0x003597,
    },
    {
        0x01AAAB, 0x0274CB, 0x02504F, 0x02346C, 0x01E000, 0x017922, 0x011235, 0x008BCA,
        0x02BEC4, 0x03CEC4, 0x03C689, 0x0365DC, 0x02E3C1, 0x024539, 0x01905A, 0x00D64D,
        0x02DBAD, 0x03F6DD, 0x03BBFB, 0x035C5C, 0x02DBAD, 0x025A40, 0x019ED6, 0x00D37B,
        0x02B1D9, 0x03BCD8, 0x038554, 0x032B2D, 0x02D134, 0x0236A5, 0x018650, 0x00C6FB,
        0x026555, 0x0352B8, 0x03215C, 0x02D134, 0x028000, 0x01F6D8, 0x015A5E, 0x00B7EF,
        0x01F6D8, 0x02B977, 0x029100, 0x0267EB, 0x0220C0, 0x01AC02, 0x0126D1, 0x00964C,
        0x0168CC, 0x021C7A, 0x01FD1E, 0x01CA31, 0x0185A9, 0x013228, 0x00DAB2, 0x006F7D,
        0x00CE01, 0x011DBD, 0x012062, 0x01038A, 0x00DCB8, 0x00B333, 0x007B6F, 0x0042FC,
    },
    {
        0x020000, 0x02F28D, 0x02C6C5, 0x02A54E, 0x024000, 0x01C48F, 0x01490C, 0x00A7BF,
        0x034B52, 0x0491B8, 0x0487D8, 0x0413D5, 0x0377B5, 0x02B977, 0x01E06C, 0x01012A,
        0x036E03, 0x04C1D6, 0x047B2D, 0x04086E, 0x036E03, 0x02D2B3, 0x01F1CE, 0x00FDC7,
        0x033BD1, 0x047C37, 0x043998, 0x03CD69, 0x036172, 0x02A7FA, 0x01D460, 0x00EEC7,
        0x02E000, 0x03FCDD, 0x03C1A1, 0x036172, 0x030000, 0x025B6A, 0x019FA3, 0x00DCB8,
        0x025B6A, 0x0344F5, 0x031466, 0x02E31B, 0x028DB3, 0x02019B, 0x0161C7, 0x00B45B,
        0x01B0F5, 0x028892, 0x0262F1, 0x0225D5, 0x01D398, 0x016F63, 0x01066F, 0x0085C9,
        0x00F735, 0x0156E2, 0x015A10, 0x013772, 0x0108DD, 0x00D70A, 0x00941F, 0x005062,
    },
    {
        0x02AAAB, 0x03EE11, 0x03B3B2, 0x038713, 0x030000, 0x025B6A, 0x01B6BB, 0x00DFAA,
        0x04646D, 0x0617A0, 0x060A75, 0x056FC6, 0x049F9B, 0x03A1F4, 0x028090, 0x0156E2,
        0x0492AE, 0x0657C8, 0x05F991, 0x056093, 0x0492AE, 0x03C399, 0x0297BD, 0x01525F,
        0x044FC1, 0x05FAF4, 0x05A220, 0x0511E1, 0x0481ED, 0x038AA2, 0x027080, 0x013E5E,
        0x03D555, 0x055126, 0x05022D, 0x0481ED, 0x040000, 0x03248D, 0x022A2F, 0x01264B,
        0x03248D, 0x045BF2, 0x041B33, 0x03D979, 0x036799, 0x02ACCF, 0x01D7B5, 0x00F079,
        0x024147, 0x0360C3, 0x032E96, 0x02DD1C, 0x026F75, 0x01E9D9, 0x015DE9, 0x00B262,
        0x01499C, 0x01C92E, 0x01CD6A, 0x019F43, 0x016127, 0x011EB8, 0x00C57F, 0x006B2D,
    },
    {
        0x038000, 0x052876, 0x04DBD9, 0x04A148, 0x03F000, 0x0317FB, 0x023FD5, 0x01258F,
        0x05C3CF, 0x07FF02, 0x07EDBA, 0x0722B4, 0x06117C, 0x04C491, 0x0348BD, 0x01C209,
        0x060085, 0x085336, 0x07D78F, 0x070EC1, 0x060085, 0x04F0B9, 0x036728, 0x01BC1C,
        0x05A8AE, 0x07D960, 0x0764CA, 0x06A777, 0x05EA87, 0x04A5F5, 0x0333A8, 0x01A1DB,
        0x050800, 0x06FA82, 0x0692DA, 0x05EA87, 0x054000, 0x041FF9, 0x02D75E, 0x018242,
        0x041FF9, 0x05B8AE, 0x0563B2, 0x050D6E, 0x0477F9, 0x0382D0, 0x026B1D, 0x013B9F,
        0x02F5AD, 0x046F00, 0x042D25, 0x03C235, 0x03324A, 0x0282ED, 0x01CB42, 0x00EA21,
        0x01B09C, 0x02580C, 0x025D9B, 0x022108, 0x01CF83, 0x017851, 0x010336, 0x008CAC,
    },
    {
        0x040000, 0x05E519, 0x058D8A, 0x054A9C, 0x048000, 0x03891F, 0x029218, 0x014F7E,
        0x0696A4, 0x092370, 0x090FB0, 0x0827AA, 0x06EF69, 0x0572EE, 0x03C0D8, 0x020254,
        0x06DC05, 0x0983AC, 0x08F65A, 0x0810DD, 0x06DC05, 0x05A565, 0x03E39B, 0x01FB8E,
        0x0677A2, 0x08F86E, 0x087330, 0x079AD1, 0x06C2E4, 0x054FF3, 0x03A8C0, 0x01DD8D,
        0x05C000, 0x07F9B9, 0x078343, 0x06C2E4, 0x060000, 0x04B6D4, 0x033F47, 0x01B970,
        0x04B6D4, 0x0689EB, 0x0628CC, 0x05C635, 0x051B65, 0x040337, 0x02C38F, 0x0168B6,
        0x0361EA, 0x051124, 0x04C5E1, 0x044BAA, 0x03A730, 0x02DEC6, 0x020CDD, 0x010B93,
        0x01EE69, 0x02ADC5, 0x02B41F, 0x026EE5, 0x0211BA, 0x01AE14, 0x01283E, 0x00A0C4,
    },
    {
        0x050000, 0x075E60, 0x06F0ED, 0x069D43, 0x05A000, 0x046B67, 0x03369E, 0x01A35E,
        0x083C4D, 0x0B6C4C, 0x0B539C, 0x0A3194, 0x08AB44, 0x06CFAA, 0x04B10F, 0x0282E8,
        0x089307, 0x0BE497, 0x0B33F1, 0x0A1514, 0x089307, 0x070EBF, 0x04DC82, 0x027A72,
        0x08158B, 0x0B3689, 0x0A8FFC, 0x098186, 0x08739C, 0x06A3F0, 0x0492F0, 0x0254F0,
        0x073000, 0x09F827, 0x096413, 0x08739C, 0x078000, 0x05E489, 0x040F19, 0x0227CC,
        0x05E489, 0x082C66, 0x07B2FF, 0x0737C2, 0x06623F, 0x050405, 0x037473, 0x01C2E3,
        0x043A64, 0x06556D, 0x05F75A, 0x055E94, 0x0490FC, 0x039677, 0x029015, 0x014E78,
        0x026A04, 0x035936, 0x036127, 0x030A9E, 0x029629, 0x021999, 0x01724E, 0x00C8F5,
    },
    {
        0x060000, 0x08D7A6, 0x085450, 0x07EFEA, 0x06C000, 0x054DAE, 0x03DB24, 0x01F73E,
        0x09E1F6, 0x0DB528, 0x0D9788, 0x0C3B7E, 0x0A671E, 0x082C66, 0x05A145, 0x03037D,
        0x0A4A08, 0x0E4582, 0x0D7187, 0x0C194B, 0x0A4A08, 0x087818, 0x05D569, 0x02F955,
        0x09B373, 0x0D74A5, 0x0CACC8, 0x0B683A, 0x0A2455, 0x07F7ED, 0x057D20, 0x02CC54,
        0x08A000, 0x0BF696, 0x0B44E4, 0x0A2455, 0x090000, 0x07123E, 0x04DEEA, 0x029629,
        0x07123E, 0x09CEE0, 0x093D32, 0x08A950, 0x07A918, 0x0604D2, 0x042556, 0x021D11,
        0x0512DF, 0x0799B6, 0x0728D2, 0x06717F, 0x057AC8, 0x044E28, 0x03134C, 0x01915C,
        0x02E59E, 0x0404A7, 0x040E2F, 0x03A657, 0x031A97, 0x02851E, 0x01BC5D, 0x00F126,
    },
    {
        0x080000, 0x0BCA33, 0x0B1B15, 0x0A9538, 0x090000, 0x07123E, 0x052430, 0x029EFD,
        0x0D2D48, 0x1246E0, 0x121F5F, 0x104F53, 0x0DDED2, 0x0AE5DD, 0x0781B1, 0x0404A7,
        0x0DB80B, 0x130757, 0x11ECB4, 0x1021B9, 0x0DB80B, 0x0B4ACB, 0x07C736, 0x03F71D,
        0x0CEF44, 0x11F0DC, 0x10E661, 0x0F35A3, 0x0D85C7, 0x0A9FE7, 0x075180, 0x03BB1A,
        0x0B8000, 0x0FF372, 0x0F0686, 0x0D85C7, 0x0C0000, 0x096DA8, 0x067E8E, 0x0372E1,
        0x096DA8, 0x0D13D6, 0x0C5198, 0x0B8C6A, 0x0A36CB, 0x08066E, 0x05871E, 0x02D16B,
        0x06C3D4, 0x0A2248, 0x098BC3, 0x089754, 0x074E60, 0x05BD8B, 0x0419BB, 0x021726,
        0x03DCD3, 0x055B8A, 0x05683E, 0x04DDC9, 0x042374, 0x035C28, 0x02507C, 0x014188,
    },
    {
        0x0C0000, 0x11AF4C, 0x10A89F, 0x0FDFD4, 0x0D8000, 0x0A9B5D, 0x07B649, 0x03EE7B,
        0x13C3EC, 0x1B6A50, 0x1B2F0F, 0x1876FD, 0x14CE3C, 0x1058CB, 0x0B4289, 0x0606FB,
        0x149410, 0x1C8B03, 0x1AE30E, 0x183296, 0x149410, 0x10F030, 0x0BAAD2, 0x05F2AB,
        0x1366E6, 0x1AE949, 0x195991, 0x16D074, 0x1448AB, 0x0FEFDA, 0x0AFA40, 0x0598A7,
        0x114000, 0x17ED2B, 0x1689C8, 0x1448AB, 0x120000, 0x0E247C, 0x09BDD5, 0x052C51,
        0x0E247C, 0x139DC1, 0x127A63, 0x11529F, 0x0F5230, 0x0C09A5, 0x084AAC, 0x043A21,
        0x0A25BE, 0x0F336D, 0x0E51A4, 0x0CE2FE, 0x0AF590, 0x089C51, 0x062698, 0x0322B9,
        0x05CB3C, 0x08094E, 0x081C5D, 0x074CAE, 0x06352E, 0x050A3B, 0x0378BA, 0x01E24D,
    },
    {
        0x110000, 0x190DAC, 0x17998C, 0x167D16, 0x132000, 0x0F06C3, 0x0AECE7, 0x0591D9,
        0x1C0039, 0x26D69C, 0x2682AB, 0x22A891, 0x1D797F, 0x172876, 0x0FF398, 0x0889E3,
        0x1D2717, 0x286F9A, 0x2616FF, 0x2247AA, 0x1D2717, 0x17FEEF, 0x108754, 0x086D1D,
        0x1B7C71, 0x261FD3, 0x23E98D, 0x2051FA, 0x1CBC47, 0x1693CA, 0x0F8D30, 0x07ED98,
        0x187000, 0x21E552, 0x1FEDDC, 0x1CBC47, 0x198000, 0x140904, 0x0DCCEE, 0x07541E,
        0x140904, 0x1BCA27, 0x1A2D62, 0x188A62, 0x15B46F, 0x110DAA, 0x0BBF1F, 0x05FD04,
        0x0E6022, 0x1588DA, 0x1448FE, 0x124192, 0x0F868B, 0x0C32C8, 0x08B6AD, 0x047130,
        0x083540, 0x0B6284, 0x0B7D84, 0x0A574B, 0x08CB57, 0x0723D4, 0x04EB08, 0x02AB42,
    },
    {
        0x160000, 0x206C0C, 0x1E8A79, 0x1D1A59, 0x18C000, 0x13722A, 0x0E2385, 0x073537,
        0x243C86, 0x3242E8, 0x31D646, 0x2CDA25, 0x2624C3, 0x1DF820, 0x14A4A7, 0x0B0CCC,
        0x25BA1D, 0x345430, 0x314AEF, 0x2C5CBE, 0x25BA1D, 0x1F0DAE, 0x1563D6, 0x0AE78E,
        0x2391FB, 0x31565C, 0x2E798A, 0x29D380, 0x252FE4, 0x1D37BB, 0x142021, 0x0A4288,
        0x1FA000, 0x2BDD7A, 0x2951EF, 0x252FE4, 0x210000, 0x19ED8D, 0x11DC06, 0x097BEA,
        0x19ED8D, 0x23F68C, 0x21E061, 0x1FC224, 0x1C16AE, 0x1611AE, 0x0F3391, 0x07BFE7,
        0x129A87, 0x1BDE47, 0x1A4058, 0x17A026, 0x141787, 0x0FC93E, 0x0B46C1, 0x05BFA8,
        0x0A9F44, 0x0EBBBA, 0x0EDEAB, 0x0D61E9, 0x0B617F, 0x093D6D, 0x065D55, 0x037437,
    },
    {
        0x1C0000, 0x2943B2, 0x26DEC9, 0x250A43, 0x1F8000, 0x18BFD8, 0x11FEA9, 0x092C75,
        0x2E1E7C, 0x3FF810, 0x3F6DCE, 0x3915A3, 0x308BE1, 0x262485, 0x1A45EB, 0x0E1049,
        0x300425, 0x4299B2, 0x3EBC76, 0x387608, 0x300425, 0x2785C6, 0x1B393F, 0x0DE0E4,
        0x2D456E, 0x3ECB00, 0x3B2652, 0x353BBA, 0x2F5439, 0x252FA8, 0x199D41, 0x0D0EDC,
        0x284000, 0x37D40F, 0x3496D3, 0x2F5439, 0x2A0000, 0x20FFCB, 0x16BAF1, 0x0C1213,
        0x20FFCB, 0x2DC56D, 0x2B1D93, 0x286B74, 0x23BFC6, 0x1C1681, 0x1358E8, 0x09DCF8,
        0x17AD66, 0x2377FE, 0x21692A, 0x1E11A5, 0x19924F, 0x141767, 0x0E5A0D, 0x075104,
        0x0D84E2, 0x12C062, 0x12ECDA, 0x110840, 0x0E7C16, 0x0BC28A, 0x0819B2, 0x04655D,
    },
    {
        0x220000, 0x321B58, 0x2F3318, 0x2CFA2D, 0x264000, 0x1E0D86, 0x15D9CE, 0x0B23B2,
        0x380072, 0x4DAD38, 0x4D0556, 0x455122, 0x3AF2FE, 0x2E50EB, 0x1FE730, 0x1113C7,
        0x3A4E2D, 0x50DF33, 0x4C2DFD, 0x448F54, 0x3A4E2D, 0x2FFDDF, 0x210EA8, 0x10DA39,
        0x36F8E1, 0x4C3FA5, 0x47D31B, 0x40A3F5, 0x39788E, 0x2D2795, 0x1F1A61, 0x0FDB2F,
        0x30E000, 0x43CAA5, 0x3FDBB7, 0x39788E, 0x330000, 0x281209, 0x1B99DB, 0x0EA83B,
        0x281209, 0x37944D, 0x345AC4, 0x3114C3, 0x2B68DF, 0x221B53, 0x177E3E, 0x0BFA09,
        0x1CC044, 0x2B11B4, 0x2891FC, 0x248324, 0x1F0D17, 0x18658F, 0x116D5A, 0x08E260,
        0x106A80, 0x16C509, 0x16FB08, 0x14AE97, 0x1196AE, 0x0E47A8, 0x09D60F, 0x055684,
    },
    {
        0x2C0000, 0x40D818, 0x3D14F2, 0x3A34B2, 0x318000, 0x26E454, 0x1C470A, 0x0E6A6E,
        0x48790C, 0x6485D0, 0x63AC8D, 0x59B44A, 0x4C4986, 0x3BF03F, 0x29494D, 0x161998,
        0x4B743A, 0x68A861, 0x6295DE, 0x58B97B, 0x4B743A, 0x3E1B5C, 0x2AC7AC, 0x15CF1D,
        0x4723F6, 0x62ACB8, 0x5CF313, 0x53A701, 0x4A5FC7, 0x3A6F75, 0x284041, 0x148510,
        0x3F4000, 0x57BAF3, 0x52A3DE, 0x4A5FC7, 0x420000, 0x33DB1A, 0x23B80D, 0x12F7D4,
        0x33DB1A, 0x47ED19, 0x43C0C2, 0x3F8448, 0x382D5C, 0x2C235D, 0x1E6723, 0x0F7FCF,
        0x25350D, 0x37BC8E, 0x3480B0, 0x2F404C, 0x282F0E, 0x1F927D, 0x168D83, 0x0B7F50,
        0x153E87, 0x1D7775, 0x1DBD56, 0x1AC3D2, 0x16C2FF, 0x127AD9, 0x0CBAAA, 0x06E86E,
    },
};

// Block types read with Huffman: values 12 .. 15 repeat the last type this many times.
constexpr int kTypeRuns[4] = {4, 8, 12, 32};

// Bink audio: the lengths (x 8) of the runs of coefficients that share a width, and the critical bands' edges in Hz.
constexpr uint32_t kAudioRuns[16] = {2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 32, 64};
constexpr uint32_t kCriticalFrequencies[25] = {100,  200,  300,  400,  510,  630,  770,  920,  1080,
                                               1270, 1480, 1720, 2000, 2320, 2700, 3150, 3700, 4400,
                                               5300, 6400, 7700, 9500, 12000, 15500, 24500};

// The BINK structure's fields this library fills.
constexpr size_t kFieldBytes = 0x200;
constexpr uint32_t kWidth = 0x00;
constexpr uint32_t kHeight = 0x04;
constexpr uint32_t kFrames = 0x08;
constexpr uint32_t kFrameNum = 0x0C;
constexpr uint32_t kLastFrameNum = 0x10;
constexpr uint32_t kFrameRate = 0x14;
constexpr uint32_t kFrameRateDiv = 0x18;
constexpr uint32_t kNumTracks = 0xF0;

// BinkOpen's BINKSNDTRACK: the tracks named by BinkSetSoundTrack (else the first track).
constexpr uint32_t kSoundTrack = 0x00004000;

// The header's flags.
constexpr uint32_t kAlphaPlane = 0x00100000;
constexpr uint32_t kGrayscale = 0x00020000;
constexpr uint32_t kAudioDct = 0x1000;
constexpr uint32_t kAudioStereo = 0x2000;

// BinkCopyToBufferRect's surface types (the low 4 bits of its flags).
constexpr uint32_t kSurface24 = 1, kSurface24R = 2, kSurface32 = 3, kSurface32R = 4, kSurface32A = 5, kSurface32RA = 6,
                   kSurface4444 = 7, kSurface5551 = 8, kSurface555 = 9, kSurface565 = 10;

uint32_t read32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

// A bit reader from the lowest bit of each byte up, as Bink writes.
class Bits {
public:
    Bits(const uint8_t* data, size_t bytes) : m_data(data), m_bytes(bytes), m_size(bytes * 8) {}

    uint32_t get(int n) {
        if (n == 0) return 0;
        const uint64_t v = window();
        m_pos += static_cast<size_t>(n);
        return static_cast<uint32_t>(v & ((uint64_t{1} << n) - 1));
    }
    bool bit() { return get(1) != 0; }
    uint32_t peek(int n) const { return static_cast<uint32_t>(window() & ((uint64_t{1} << n) - 1)); }
    void skip(size_t n) { m_pos += n; }
    void align32() { m_pos = (m_pos + 31) & ~size_t{31}; }
    size_t position() const { return m_pos; }
    size_t size() const { return m_size; }
    bool over() const { return m_pos > m_size; }

private:
    uint64_t window() const {
        const size_t byte = m_pos >> 3;
        uint64_t v = 0;
        if (byte + 8 <= m_bytes) {
            std::memcpy(&v, m_data + byte, 8);
        } else {
            for (size_t i = 0; i < 8 && byte + i < m_bytes; ++i) v |= uint64_t{m_data[byte + i]} << (8 * i);
        }
        return v >> (m_pos & 7);
    }

    const uint8_t* m_data;
    size_t m_bytes;
    size_t m_size;
    size_t m_pos = 0;
};

// The 16 trees as tables of their 7-bit windows: the symbol and its length.
struct Vlc {
    uint8_t symbol[128];
    uint8_t length[128];
};

const std::array<Vlc, 16>& vlcs() {
    static const std::array<Vlc, 16> tables = [] {
        std::array<Vlc, 16> t{};
        for (int n = 0; n < 16; ++n) {
            for (int s = 0; s < 16; ++s) {
                const int length = kTreeLengths[n][s];
                for (int high = 0; high < (128 >> length); ++high) {
                    const int i = kTreeCodes[n][s] | (high << length);
                    t[n].symbol[i] = static_cast<uint8_t>(s);
                    t[n].length[i] = static_cast<uint8_t>(length);
                }
            }
        }
        return t;
    }();
    return tables;
}

// Bink's 8x8 inverse DCT: columns, then rows rounded down by 8 bits.
constexpr int kA1 = 2896, kA2 = 2217, kA3 = 3784, kA4 = -5352;

int mul(int a, int b) {
    return static_cast<int>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)) >> 11;
}

template <typename Out, typename Round>
void idct8(Out* d, ptrdiff_t ds, const int32_t* s, ptrdiff_t ss, Round round) {
    const int a0 = s[0] + s[4 * ss];
    const int a1 = s[0] - s[4 * ss];
    const int a2 = s[2 * ss] + s[6 * ss];
    const int a3 = mul(kA1, s[2 * ss] - s[6 * ss]);
    const int a4 = s[5 * ss] + s[3 * ss];
    const int a5 = s[5 * ss] - s[3 * ss];
    const int a6 = s[1 * ss] + s[7 * ss];
    const int a7 = s[1 * ss] - s[7 * ss];
    const int b0 = a4 + a6;
    const int b1 = mul(kA3, a5 + a7);
    const int b2 = mul(kA4, a5) - b0 + b1;
    const int b3 = mul(kA1, a6 - a4) - b2;
    const int b4 = mul(kA2, a7) + b3 - b1;
    d[0 * ds] = round(a0 + a2 + b0);
    d[1 * ds] = round(a1 + a3 - a2 + b2);
    d[2 * ds] = round(a1 - a3 + a2 + b3);
    d[3 * ds] = round(a0 - a2 - b4);
    d[4 * ds] = round(a0 - a2 + b4);
    d[5 * ds] = round(a1 - a3 + a2 - b3);
    d[6 * ds] = round(a1 + a3 - a2 - b2);
    d[7 * ds] = round(a0 + a2 - b0);
}

void idctColumns(int32_t* temp, const int32_t* block) {
    for (int i = 0; i < 8; ++i) {
        const int32_t* s = block + i;
        if ((s[8] | s[16] | s[24] | s[32] | s[40] | s[48] | s[56]) == 0) {
            for (int j = 0; j < 8; ++j) temp[i + 8 * j] = s[0];
        } else {
            idct8(temp + i, 8, s, 8, [](int v) { return v; });
        }
    }
}

uint8_t clip(int v) {
    return static_cast<uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v);
}

void idctPut(uint8_t* dst, ptrdiff_t stride, const int32_t* block) {
    int32_t temp[64];
    idctColumns(temp, block);
    for (int i = 0; i < 8; ++i) idct8(dst + i * stride, 1, temp + 8 * i, 1, [](int v) { return clip((v + 0x7F) >> 8); });
}

void idctAdd(uint8_t* dst, ptrdiff_t stride, const int32_t* block) {
    int32_t temp[64];
    int32_t rows[8];
    idctColumns(temp, block);
    for (int i = 0; i < 8; ++i) {
        idct8(rows, 1, temp + 8 * i, 1, [](int v) { return (v + 0x7F) >> 8; });
        uint8_t* d = dst + i * stride;
        // Added modulo 256 as binkw32 adds (not clamped).
        for (int j = 0; j < 8; ++j) d[j] = static_cast<uint8_t>(d[j] + rows[j]);
    }
}

void copy8(uint8_t* dst, const uint8_t* src, ptrdiff_t stride) {
    for (int j = 0; j < 8; ++j) std::memcpy(dst + j * stride, src + j * stride, 8);
}

void fill(uint8_t* dst, ptrdiff_t stride, int size, uint8_t v) {
    for (int j = 0; j < size; ++j) std::memset(dst + j * stride, v, static_cast<size_t>(size));
}

// An 8x8 block drawn at twice its size.
void scale(const uint8_t* unscaled, uint8_t* dst, ptrdiff_t stride) {
    for (int j = 0; j < 8; ++j) {
        uint8_t* d0 = dst + 2 * j * stride;
        uint8_t* d1 = d0 + stride;
        for (int i = 0; i < 8; ++i) {
            const uint8_t v = unscaled[j * 8 + i];
            d0[2 * i] = d0[2 * i + 1] = d1[2 * i] = d1[2 * i + 1] = v;
        }
    }
}

int64_t now() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

int64_t ticksPerSecond() {
    static const int64_t f = [] {
        LARGE_INTEGER t;
        QueryPerformanceFrequency(&t);
        return t.QuadPart;
    }();
    return f;
}

}  // namespace

// ---- The header.

bool MovieHeader::readStart(const uint8_t* b) {
    if (b[0] != 'B' || b[1] != 'I' || b[2] != 'K' || b[3] < 'b' || b[3] > 'k') return false;
    revision = b[3];
    frames = read32(b + 8);
    largestFrame = read32(b + 12);
    width = read32(b + 20);
    height = read32(b + 24);
    rate = read32(b + 28);
    rateDiv = read32(b + 32);
    videoFlags = read32(b + 36);
    const uint32_t trackCount = read32(b + 40);
    if (frames == 0 || frames > 1000000 || width == 0 || height == 0 || width > 4096 || height > 4096 ||
        trackCount > 256 || rate == 0 || rateDiv == 0) {
        return false;
    }
    tracks.assign(trackCount, {});
    return true;
}

size_t MovieHeader::tableBytes() const {
    return tracks.size() * 12 + (static_cast<size_t>(frames) + 1) * 4;
}

// After the fixed part: each track's largest packet (not needed), its rate and flags, its id; then the index.
bool MovieHeader::readTables(const uint8_t* b, uint64_t fileSize) {
    const size_t n = tracks.size();
    for (size_t i = 0; i < n; ++i) {
        const uint32_t v = read32(b + 4 * n + 4 * i);
        tracks[i].sampleRate = v & 0xFFFF;
        tracks[i].flags = v >> 16;
        tracks[i].id = read32(b + 8 * n + 4 * i);
    }
    index.resize(static_cast<size_t>(frames) + 1);
    std::memcpy(index.data(), b + 12 * n, index.size() * 4);
    for (size_t i = 1; i < index.size(); ++i) {
        if ((index[i] & ~1u) < (index[i - 1] & ~1u) || (index[i] & ~1u) > fileSize) return false;
    }
    return true;
}

// ---- The pictures.

namespace {

enum Source { kBlockTypes, kSubBlockTypes, kColors, kPattern, kXOff, kYOff, kIntraDc, kInterDc, kRun, kSources };
enum BlockType { kSkip, kScaled, kMotion, kRunBlock, kResidue, kIntra, kFill, kInter, kPatternBlock, kRaw };

struct Tree {
    int table = 0;
    std::array<uint8_t, 16> symbols{};
};

// A bundle: the values of one kind for the blocks of a plane, read a row of blocks at a time as they run out.
struct Bundle {
    std::vector<int16_t> values;
    size_t written = 0;
    size_t read = 0;
    bool ended = false;
    int bits = 0;  // of the count of values a read gives
    Tree tree;
};

struct Plane {
    std::vector<uint8_t> pixels;
    ptrdiff_t stride = 0;
};

}  // namespace

struct VideoDecoder::State {
    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t revision = 'i';
    bool alpha = false;
    bool gray = false;
    std::array<std::array<Plane, 4>, 2> pictures;
    int current = 0;
    std::array<Bundle, kSources> bundles;
    std::array<Tree, 16> colorHigh;
    int lastColor = 0;

    uint8_t huffman(Bits& b, const Tree& t) const {
        const Vlc& v = vlcs()[static_cast<size_t>(t.table)];
        const uint32_t w = b.peek(7);
        b.skip(v.length[w]);
        return t.symbols[v.symbol[w]];
    }

    static void merge(Bits& b, uint8_t* dst, const uint8_t* src, int size) {
        const uint8_t* src2 = src + size;
        int size2 = size;
        do {
            if (!b.bit()) {
                *dst++ = *src++;
                --size;
            } else {
                *dst++ = *src2++;
                --size2;
            }
        } while (size != 0 && size2 != 0);
        while (size-- > 0) *dst++ = *src++;
        while (size2-- > 0) *dst++ = *src2++;
    }

    // A tree: its table and the order of its 16 symbols, listed or merged from sorted halves.
    void readTree(Bits& b, Tree& t) {
        t.table = static_cast<int>(b.get(4));
        if (t.table == 0) {
            for (int i = 0; i < 16; ++i) t.symbols[static_cast<size_t>(i)] = static_cast<uint8_t>(i);
            return;
        }
        if (b.bit()) {
            std::array<bool, 16> used{};
            int length = static_cast<int>(b.get(3));
            for (int i = 0; i <= length; ++i) {
                t.symbols[static_cast<size_t>(i)] = static_cast<uint8_t>(b.get(4));
                used[t.symbols[static_cast<size_t>(i)]] = true;
            }
            for (int i = 0; i < 16 && length < 15; ++i) {
                if (!used[static_cast<size_t>(i)]) t.symbols[static_cast<size_t>(++length)] = static_cast<uint8_t>(i);
            }
        } else {
            const int depth = static_cast<int>(b.get(2));
            std::array<uint8_t, 16> in{}, out{};
            for (int i = 0; i < 16; ++i) in[static_cast<size_t>(i)] = static_cast<uint8_t>(i);
            for (int i = 0; i <= depth; ++i) {
                const int size = 1 << i;
                for (int at = 0; at < 16; at += size * 2) merge(b, out.data() + at, in.data() + at, size);
                std::swap(in, out);
            }
            t.symbols = in;
        }
    }

    void readBundle(Bits& b, int source) {
        Bundle& u = bundles[static_cast<size_t>(source)];
        if (source == kColors) {
            for (Tree& t : colorHigh) readTree(b, t);
            lastColor = 0;
        }
        if (source != kIntraDc && source != kInterDc) readTree(b, u.tree);
        u.written = 0;
        u.read = 0;
        u.ended = false;
    }

    static int lengthFor(uint32_t v) { return static_cast<int>(std::bit_width(v + 511)); }

    // The bits of each bundle's counts, by the plane's width and its blocks across.
    void setLengths(uint32_t w, uint32_t blocksWide) {
        w = (w + 7) & ~7u;
        bundles[kBlockTypes].bits = lengthFor(w >> 3);
        bundles[kSubBlockTypes].bits = lengthFor(w >> 4);
        bundles[kColors].bits = lengthFor(blocksWide * 64);
        bundles[kIntraDc].bits = bundles[kInterDc].bits = bundles[kXOff].bits = bundles[kYOff].bits =
            lengthFor(w >> 3);
        bundles[kPattern].bits = lengthFor(blocksWide << 3);
        bundles[kRun].bits = lengthFor(blocksWide * 48);
    }

    // How many values this row of blocks adds: none while the last ones are not used up, none for good after a 0;
    // -1 when they would not fit.
    static int newValues(Bits& b, Bundle& u) {
        if (u.ended || u.written > u.read) return 0;
        const uint32_t t = b.get(u.bits);
        if (t == 0) {
            u.ended = true;
            return 0;
        }
        if (u.written + t > u.values.size()) return -1;
        return static_cast<int>(t);
    }

    bool readBlockTypes(Bits& b, Bundle& u) {
        const int t = newValues(b, u);
        if (t <= 0) return t == 0;
        const size_t end = u.written + static_cast<size_t>(t);
        if (b.bit()) {
            const auto v = static_cast<int16_t>(b.get(4));
            std::fill(u.values.begin() + static_cast<ptrdiff_t>(u.written), u.values.begin() + static_cast<ptrdiff_t>(end), v);
            u.written = end;
            return true;
        }
        int16_t last = 0;
        do {
            const int v = huffman(b, u.tree);
            if (v < 12) {
                last = static_cast<int16_t>(v);
                u.values[u.written++] = last;
            } else {
                const auto run = static_cast<size_t>(kTypeRuns[v - 12]);
                if (end - u.written < run) return false;
                std::fill_n(u.values.begin() + static_cast<ptrdiff_t>(u.written), run, last);
                u.written += run;
            }
        } while (u.written < end);
        return true;
    }

    int color(Bits& b, const Bundle& u) {
        lastColor = huffman(b, colorHigh[static_cast<size_t>(lastColor)]);
        int v = (lastColor << 4) | huffman(b, u.tree);
        if (revision < 'i') {
            const int sign = static_cast<int8_t>(v) >> 7;
            v = (((v & 0x7F) ^ sign) - sign) + 0x80;
        }
        return v;
    }

    bool readColors(Bits& b, Bundle& u) {
        const int t = newValues(b, u);
        if (t <= 0) return t == 0;
        const size_t end = u.written + static_cast<size_t>(t);
        if (b.bit()) {
            const auto v = static_cast<int16_t>(color(b, u) & 0xFF);
            std::fill(u.values.begin() + static_cast<ptrdiff_t>(u.written), u.values.begin() + static_cast<ptrdiff_t>(end), v);
            u.written = end;
        } else {
            while (u.written < end) u.values[u.written++] = static_cast<int16_t>(color(b, u) & 0xFF);
        }
        return true;
    }

    bool readPatterns(Bits& b, Bundle& u) {
        const int t = newValues(b, u);
        if (t <= 0) return t == 0;
        const size_t end = u.written + static_cast<size_t>(t);
        while (u.written < end) {
            int v = huffman(b, u.tree);
            v |= huffman(b, u.tree) << 4;
            u.values[u.written++] = static_cast<int16_t>(v);
        }
        return true;
    }

    bool readMotion(Bits& b, Bundle& u) {
        const int t = newValues(b, u);
        if (t <= 0) return t == 0;
        const size_t end = u.written + static_cast<size_t>(t);
        if (b.bit()) {
            int v = static_cast<int>(b.get(4));
            if (v != 0 && b.bit()) v = -v;
            std::fill(u.values.begin() + static_cast<ptrdiff_t>(u.written), u.values.begin() + static_cast<ptrdiff_t>(end),
                      static_cast<int16_t>(v));
            u.written = end;
        } else {
            while (u.written < end) {
                int v = huffman(b, u.tree);
                if (v != 0 && b.bit()) v = -v;
                u.values[u.written++] = static_cast<int16_t>(v);
            }
        }
        return true;
    }

    bool readRuns(Bits& b, Bundle& u) {
        const int t = newValues(b, u);
        if (t <= 0) return t == 0;
        const size_t end = u.written + static_cast<size_t>(t);
        if (b.bit()) {
            const auto v = static_cast<int16_t>(b.get(4));
            std::fill(u.values.begin() + static_cast<ptrdiff_t>(u.written), u.values.begin() + static_cast<ptrdiff_t>(end), v);
            u.written = end;
        } else {
            while (u.written < end) u.values[u.written++] = huffman(b, u.tree);
        }
        return true;
    }

    // DC values: the first whole, the rest as differences in groups of 8 of a width each.
    static bool readDcs(Bits& b, Bundle& u, int startBits, bool hasSign) {
        int t = newValues(b, u);
        if (t <= 0) return t == 0;
        int v;
        if (hasSign) {
            v = static_cast<int>(b.get(startBits - 1));
            if (v != 0 && b.bit()) v = -v;
        } else {
            v = static_cast<int>(b.get(startBits));
        }
        u.values[u.written++] = static_cast<int16_t>(v);
        --t;
        for (int i = 0; i < t; i += 8) {
            const int n = std::min(t - i, 8);
            const int size = static_cast<int>(b.get(4));
            for (int j = 0; j < n; ++j) {
                if (size != 0) {
                    int d = static_cast<int>(b.get(size));
                    if (d != 0 && b.bit()) d = -d;
                    v += d;
                    if (v < -32768 || v > 32767) return false;
                }
                u.values[u.written++] = static_cast<int16_t>(v);
            }
        }
        return true;
    }

    int value(int source) {
        Bundle& u = bundles[static_cast<size_t>(source)];
        return u.read < u.values.size() ? u.values[u.read++] : 0;
    }

    static int coefficient(Bits& b, int bits) {
        if (bits == 0) return b.bit() ? -1 : 1;
        int t = static_cast<int>(b.get(bits)) | (1 << bits);
        if (b.bit()) t = -t;
        return t;
    }

    // A block's DCT coefficients by bit planes of their size, then its quantizer (-1: broken). count and order list
    // the coefficients read, by their place in kScan.
    static int readDct(Bits& b, int32_t block[64], int order[64], int& count) {
        int coefs[128];
        int modes[128];
        int listStart = 64, listEnd = 64;
        count = 0;
        coefs[listEnd] = 4;
        modes[listEnd++] = 0;
        coefs[listEnd] = 24;
        modes[listEnd++] = 0;
        coefs[listEnd] = 44;
        modes[listEnd++] = 0;
        coefs[listEnd] = 1;
        modes[listEnd++] = 3;
        coefs[listEnd] = 2;
        modes[listEnd++] = 3;
        coefs[listEnd] = 3;
        modes[listEnd++] = 3;
        for (int bits = static_cast<int>(b.get(4)) - 1; bits >= 0; --bits) {
            int pos = listStart;
            while (pos < listEnd) {
                if ((modes[pos] | coefs[pos]) == 0 || !b.bit()) {
                    ++pos;
                    continue;
                }
                int c = coefs[pos];
                const int mode = modes[pos];
                switch (mode) {
                case 0:
                case 2:
                    if (mode == 0) {
                        coefs[pos] = c + 4;
                        modes[pos] = 1;
                    } else {
                        coefs[pos] = 0;
                        modes[pos++] = 0;
                    }
                    for (int i = 0; i < 4; ++i, ++c) {
                        if (b.bit()) {
                            if (listStart == 0) return -1;
                            coefs[--listStart] = c;
                            modes[listStart] = 3;
                        } else {
                            if (c > 63 || count > 63) return -1;
                            block[kScan[c]] = coefficient(b, bits);
                            order[count++] = c;
                        }
                    }
                    break;
                case 1:
                    modes[pos] = 2;
                    for (int i = 0; i < 3; ++i) {
                        c += 4;
                        if (listEnd >= 128) return -1;
                        coefs[listEnd] = c;
                        modes[listEnd++] = 2;
                    }
                    break;
                case 3:
                    if (c > 63 || count > 63) return -1;
                    block[kScan[c]] = coefficient(b, bits);
                    order[count++] = c;
                    coefs[pos] = 0;
                    modes[pos++] = 0;
                    break;
                default:
                    break;
                }
            }
        }
        return static_cast<int>(b.get(4));
    }

    static void unquantize(int32_t block[64], const int32_t* quant, const int order[64], int count) {
        block[0] = static_cast<int32_t>(static_cast<uint32_t>(block[0]) * static_cast<uint32_t>(quant[0])) >> 11;
        for (int i = 0; i < count; ++i) {
            const int p = kScan[order[i]];
            block[p] = static_cast<int32_t>(static_cast<uint32_t>(block[p]) * static_cast<uint32_t>(quant[p])) >> 11;
        }
    }

    // A residue's coefficients (pixels, in kScan's order) by bit planes, at most masks of them set.
    static void readResidue(Bits& b, int16_t block[64], int masks) {
        int coefs[128];
        int modes[128];
        int nonZero[64];
        int nonZeroCount = 0;
        int listStart = 64, listEnd = 64;
        coefs[listEnd] = 4;
        modes[listEnd++] = 0;
        coefs[listEnd] = 24;
        modes[listEnd++] = 0;
        coefs[listEnd] = 44;
        modes[listEnd++] = 0;
        coefs[listEnd] = 0;
        modes[listEnd++] = 2;
        for (int mask = 1 << b.get(3); mask != 0; mask >>= 1) {
            for (int i = 0; i < nonZeroCount; ++i) {
                if (!b.bit()) continue;
                int16_t& v = block[nonZero[i]];
                v = static_cast<int16_t>(v < 0 ? v - mask : v + mask);
                if (--masks < 0) return;
            }
            int pos = listStart;
            while (pos < listEnd) {
                if ((coefs[pos] | modes[pos]) == 0 || !b.bit()) {
                    ++pos;
                    continue;
                }
                int c = coefs[pos];
                const int mode = modes[pos];
                switch (mode) {
                case 0:
                case 2:
                    if (mode == 0) {
                        coefs[pos] = c + 4;
                        modes[pos] = 1;
                    } else {
                        coefs[pos] = 0;
                        modes[pos++] = 0;
                    }
                    for (int i = 0; i < 4; ++i, ++c) {
                        if (b.bit()) {
                            if (listStart == 0) return;
                            coefs[--listStart] = c;
                            modes[listStart] = 3;
                        } else {
                            if (c > 63 || nonZeroCount >= 64) return;
                            nonZero[nonZeroCount++] = kScan[c];
                            block[kScan[c]] = static_cast<int16_t>(b.bit() ? -mask : mask);
                            if (--masks < 0) return;
                        }
                    }
                    break;
                case 1:
                    modes[pos] = 2;
                    for (int i = 0; i < 3; ++i) {
                        c += 4;
                        if (listEnd >= 128) return;
                        coefs[listEnd] = c;
                        modes[listEnd++] = 2;
                    }
                    break;
                case 3:
                    if (c > 63 || nonZeroCount >= 64) return;
                    nonZero[nonZeroCount++] = kScan[c];
                    block[kScan[c]] = static_cast<int16_t>(b.bit() ? -mask : mask);
                    coefs[pos] = 0;
                    modes[pos++] = 0;
                    if (--masks < 0) return;
                    break;
                default:
                    break;
                }
            }
        }
    }

    // The 8x8 block of a run block: runs of one color or of colors each, along one of the 16 scan orders.
    template <typename Put>
    bool runBlock(Bits& b, Put put) {
        const uint8_t* scan = kPatterns[b.get(4)];
        int i = 0;
        do {
            const int run = value(kRun) + 1;
            i += run;
            if (i > 64) return false;
            if (b.bit()) {
                const auto v = static_cast<uint8_t>(value(kColors));
                for (int j = 0; j < run; ++j) put(*scan++, v);
            } else {
                for (int j = 0; j < run; ++j) put(*scan++, static_cast<uint8_t>(value(kColors)));
            }
        } while (i < 63);
        if (i == 63) put(*scan, static_cast<uint8_t>(value(kColors)));
        return true;
    }

    bool decodePlane(Bits& b, int index, bool chroma) {
        const uint32_t bw = chroma ? (width + 15) >> 4 : (width + 7) >> 3;
        const uint32_t bh = chroma ? (height + 15) >> 4 : (height + 7) >> 3;
        setLengths(std::max<uint32_t>(width >> (chroma ? 1 : 0), 8), bw);
        for (int i = 0; i < kSources; ++i) readBundle(b, i);
        Plane& out = pictures[static_cast<size_t>(current)][static_cast<size_t>(index)];
        const Plane& last = pictures[static_cast<size_t>(current ^ 1)][static_cast<size_t>(index)];
        const ptrdiff_t stride = out.stride;
        const ptrdiff_t lastRef = static_cast<ptrdiff_t>(bw - 1) * 8 + stride * static_cast<ptrdiff_t>(bh - 1) * 8;
        int32_t block[64];
        int16_t residue[64];
        uint8_t unscaled[64];
        int order[64];
        int count = 0;
        // The block of the last picture a motion vector points to; false when it points outside.
        auto reference = [&](ptrdiff_t at, const uint8_t*& ref) {
            const ptrdiff_t r = at + value(kYOff) * stride + value(kXOff);
            if (r < 0 || r > lastRef) return false;
            ref = last.pixels.data() + r;
            return true;
        };
        for (uint32_t by = 0; by < bh; ++by) {
            if (!readBlockTypes(b, bundles[kBlockTypes]) || !readBlockTypes(b, bundles[kSubBlockTypes]) ||
                !readColors(b, bundles[kColors]) || !readPatterns(b, bundles[kPattern]) ||
                !readMotion(b, bundles[kXOff]) || !readMotion(b, bundles[kYOff]) ||
                !readDcs(b, bundles[kIntraDc], 11, false) || !readDcs(b, bundles[kInterDc], 11, true) ||
                !readRuns(b, bundles[kRun])) {
                return false;
            }
            for (uint32_t bx = 0; bx < bw; ++bx) {
                const ptrdiff_t at = static_cast<ptrdiff_t>(by) * 8 * stride + static_cast<ptrdiff_t>(bx) * 8;
                uint8_t* dst = out.pixels.data() + at;
                const int type = value(kBlockTypes);
                // A scaled block covers the row below too: its place there is skipped.
                if ((by & 1) != 0 && type == kScaled) {
                    ++bx;
                    continue;
                }
                const uint8_t* ref = nullptr;
                switch (type) {
                case kSkip:
                    copy8(dst, last.pixels.data() + at, stride);
                    break;
                case kScaled: {
                    const int sub = value(kSubBlockTypes);
                    switch (sub) {
                    case kRunBlock:
                        if (!runBlock(b, [&](int p, uint8_t v) { unscaled[p] = v; })) return false;
                        break;
                    case kIntra: {
                        std::memset(block, 0, sizeof(block));
                        block[0] = value(kIntraDc);
                        const int q = readDct(b, block, order, count);
                        if (q < 0) return false;
                        unquantize(block, kIntraQuant[q], order, count);
                        idctPut(unscaled, 8, block);
                        break;
                    }
                    case kFill:
                        fill(dst, stride, 16, static_cast<uint8_t>(value(kColors)));
                        break;
                    case kPatternBlock: {
                        uint8_t colors[2];
                        colors[0] = static_cast<uint8_t>(value(kColors));
                        colors[1] = static_cast<uint8_t>(value(kColors));
                        for (int j = 0; j < 8; ++j) {
                            int v = value(kPattern);
                            for (int i = 0; i < 8; ++i, v >>= 1) unscaled[j * 8 + i] = colors[v & 1];
                        }
                        break;
                    }
                    case kRaw:
                        for (int i = 0; i < 64; ++i) unscaled[i] = static_cast<uint8_t>(value(kColors));
                        break;
                    default:
                        return false;
                    }
                    if (sub != kFill) scale(unscaled, dst, stride);
                    ++bx;
                    break;
                }
                case kMotion:
                    if (!reference(at, ref)) return false;
                    copy8(dst, ref, stride);
                    break;
                case kRunBlock:
                    if (!runBlock(b, [&](int p, uint8_t v) { dst[(p >> 3) * stride + (p & 7)] = v; })) return false;
                    break;
                case kResidue: {
                    if (!reference(at, ref)) return false;
                    copy8(dst, ref, stride);
                    std::memset(residue, 0, sizeof(residue));
                    readResidue(b, residue, static_cast<int>(b.get(7)));
                    for (int j = 0; j < 8; ++j) {
                        for (int i = 0; i < 8; ++i) {
                            uint8_t& d = dst[j * stride + i];
                            d = static_cast<uint8_t>(d + residue[j * 8 + i]);  // modulo 256
                        }
                    }
                    break;
                }
                case kIntra: {
                    std::memset(block, 0, sizeof(block));
                    block[0] = value(kIntraDc);
                    const int q = readDct(b, block, order, count);
                    if (q < 0) return false;
                    unquantize(block, kIntraQuant[q], order, count);
                    idctPut(dst, stride, block);
                    break;
                }
                case kFill:
                    fill(dst, stride, 8, static_cast<uint8_t>(value(kColors)));
                    break;
                case kInter: {
                    if (!reference(at, ref)) return false;
                    copy8(dst, ref, stride);
                    std::memset(block, 0, sizeof(block));
                    block[0] = value(kInterDc);
                    const int q = readDct(b, block, order, count);
                    if (q < 0) return false;
                    unquantize(block, kInterQuant[q], order, count);
                    idctAdd(dst, stride, block);
                    break;
                }
                case kPatternBlock: {
                    uint8_t colors[2];
                    colors[0] = static_cast<uint8_t>(value(kColors));
                    colors[1] = static_cast<uint8_t>(value(kColors));
                    for (int j = 0; j < 8; ++j) {
                        int v = value(kPattern);
                        for (int i = 0; i < 8; ++i, v >>= 1) dst[j * stride + i] = colors[v & 1];
                    }
                    break;
                }
                case kRaw:
                    for (int j = 0; j < 8; ++j) {
                        for (int i = 0; i < 8; ++i) dst[j * stride + i] = static_cast<uint8_t>(value(kColors));
                    }
                    break;
                default:
                    return false;
                }
            }
            if (b.over()) return false;
        }
        b.align32();
        return !b.over();
    }
};

VideoDecoder::VideoDecoder() : m(std::make_unique<State>()) {}
VideoDecoder::~VideoDecoder() = default;

void VideoDecoder::start(const MovieHeader& header) {
    State& s = *m;
    s.width = header.width;
    s.height = header.height;
    s.revision = header.revision;
    s.alpha = (header.videoFlags & kAlphaPlane) != 0;
    s.gray = (header.videoFlags & kGrayscale) != 0;
    s.current = 0;
    // Room for a scaled block past the last column and row.
    const uint32_t bw = (s.width + 7) >> 3, bh = (s.height + 7) >> 3;
    const uint32_t cw = (s.width + 15) >> 4, ch = (s.height + 15) >> 4;
    for (auto& picture : s.pictures) {
        for (int i = 0; i < 4; ++i) {
            Plane& p = picture[static_cast<size_t>(i)];
            const bool chroma = i == 1 || i == 2;
            const uint32_t w = chroma ? cw : bw, h = chroma ? ch : bh;
            if (i == 3 && !s.alpha) {
                p = {};
                continue;
            }
            p.stride = static_cast<ptrdiff_t>(w + 2) * 8;
            p.pixels.assign(static_cast<size_t>(p.stride) * (h + 2) * 8, static_cast<uint8_t>(chroma ? 128 : i == 3 ? 255 : 0));
        }
    }
    const size_t blocks = static_cast<size_t>(bw) * bh;
    for (Bundle& u : s.bundles) u.values.assign(blocks * 64, 0);
}

bool VideoDecoder::decode(const uint8_t* data, size_t bytes) {
    State& s = *m;
    if (s.width == 0) return false;
    s.current ^= 1;
    Bits b(data, bytes);
    if (s.alpha) {
        if (s.revision >= 'i') b.skip(32);
        if (!s.decodePlane(b, 3, false)) return false;
    }
    if (s.revision >= 'i') b.skip(32);
    for (int plane = 0; plane < 3; ++plane) {
        if (plane != 0 && s.gray) break;
        // From revision 'h' the second plane is V, the third U.
        const int index = plane == 0 ? 0 : s.revision >= 'h' ? 3 - plane : plane;
        if (!s.decodePlane(b, index, plane != 0)) return false;
        if (b.position() >= b.size()) break;
    }
    return true;
}

const uint8_t* VideoDecoder::plane(int index) const {
    const Plane& p = m->pictures[static_cast<size_t>(m->current)][static_cast<size_t>(index)];
    return p.pixels.empty() ? nullptr : p.pixels.data();
}

int32_t VideoDecoder::stride(int index) const {
    return static_cast<int32_t>(m->pictures[static_cast<size_t>(m->current)][static_cast<size_t>(index)].stride);
}

// ---- The sound.

namespace {

// A float of Bink audio: 5 bits of exponent, 23 of mantissa, the sign.
float readFloat(Bits& b) {
    const int power = static_cast<int>(b.get(5));
    float f = std::ldexp(static_cast<float>(b.get(23)), power - 23);
    if (b.bit()) f = -f;
    return f;
}

// An inverse complex FFT (e^{+2 pi i k n / N}) of a power of two.
class Fft {
public:
    void start(uint32_t n) {
        m_n = n;
        m_turns.resize(n / 2);
        for (uint32_t k = 0; k < n / 2; ++k) {
            const double a = 2.0 * std::numbers::pi * k / n;
            m_turns[k] = {static_cast<float>(std::cos(a)), static_cast<float>(std::sin(a))};
        }
        m_reversed.resize(n);
        const int bits = std::countr_zero(n);
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t r = 0;
            for (int j = 0; j < bits; ++j) r |= ((i >> j) & 1u) << (bits - 1 - j);
            m_reversed[i] = r;
        }
    }

    void run(std::complex<float>* x) const {
        for (uint32_t i = 0; i < m_n; ++i) {
            if (i < m_reversed[i]) std::swap(x[i], x[m_reversed[i]]);
        }
        for (uint32_t size = 2; size <= m_n; size *= 2) {
            const uint32_t half = size / 2, step = m_n / size;
            for (uint32_t at = 0; at < m_n; at += size) {
                for (uint32_t k = 0; k < half; ++k) {
                    const std::complex<float> t = m_turns[k * step] * x[at + k + half];
                    x[at + k + half] = x[at + k] - t;
                    x[at + k] += t;
                }
            }
        }
    }

private:
    uint32_t m_n = 0;
    std::vector<std::complex<float>> m_turns;
    std::vector<uint32_t> m_reversed;
};

}  // namespace

struct AudioDecoder::State {
    bool dct = true;
    uint32_t rate = 0;
    uint32_t channels = 1;       // of the samples made
    uint32_t blockChannels = 1;  // transformed apart (RDFT: 1, its channels interleaved in the block)
    uint32_t length = 0;         // a block's samples
    uint32_t overlap = 0;
    uint32_t bandCount = 0;
    std::array<uint32_t, 26> bands{};
    float root = 0.0f;
    std::array<float, 96> quant{};
    bool first = true;
    std::array<std::vector<float>, 2> coefficients;
    std::array<std::vector<float>, 2> previous;
    std::vector<std::complex<float>> work;
    std::vector<std::complex<float>> shifts;  // DCT: e^{i pi k / 2N}
    Fft fft;

    // DCT-III: out_n = 2/N (X0 + sum X_k cos(pi k (2n + 1) / 2N)), through an inverse FFT of the X_k turned by
    // pi k / 2N, its even samples in order and its odd ones from the end.
    void inverseDct(float* x) {
        const uint32_t n = length;
        for (uint32_t k = 0; k < n; ++k) work[k] = x[k] * shifts[k];
        fft.run(work.data());
        const float s = 2.0f / static_cast<float>(n);
        for (uint32_t i = 0; i < n / 2; ++i) {
            x[2 * i] = work[i].real() * s;
            x[2 * i + 1] = work[n - 1 - i].real() * s;
        }
    }

    // The inverse real DFT: x[0] the DC, x[1] the Nyquist term, then the real and imaginary parts; half the sum.
    void inverseRdft(float* x) {
        const uint32_t n = length;
        work[0] = x[0];
        work[n / 2] = x[1];
        for (uint32_t k = 1; k < n / 2; ++k) {
            work[k] = {x[2 * k], x[2 * k + 1]};
            work[n - k] = std::conj(work[k]);
        }
        fft.run(work.data());
        for (uint32_t i = 0; i < n; ++i) x[i] = work[i].real() * 0.5f;
    }

    bool block(Bits& b, std::vector<int16_t>& out) {
        if (dct) b.skip(2);
        for (uint32_t ch = 0; ch < blockChannels; ++ch) {
            float* c = coefficients[ch].data();
            c[0] = readFloat(b) * root;
            c[1] = readFloat(b) * root;
            float q[25];
            for (uint32_t i = 0; i < bandCount; ++i) q[i] = quant[std::min<uint32_t>(b.get(8), 95)];
            uint32_t k = 0;
            float qv = q[0];
            uint32_t i = 2;
            while (i < length) {
                uint32_t j = b.bit() ? i + kAudioRuns[b.get(4)] * 8 : i + 8;
                j = std::min(j, length);
                const int width = static_cast<int>(b.get(4));
                if (width == 0) {
                    std::fill(c + i, c + j, 0.0f);
                    i = j;
                    while (bands[k] < i) qv = q[k++];
                } else {
                    while (i < j) {
                        if (bands[k] == i) qv = q[k++];
                        const uint32_t v = b.get(width);
                        c[i] = v == 0 ? 0.0f : b.bit() ? -qv * static_cast<float>(v) : qv * static_cast<float>(v);
                        ++i;
                    }
                }
            }
            if (b.over()) return false;
            if (dct) {
                inverseDct(c);
            } else {
                inverseRdft(c);
            }
        }
        auto sample = [](float v) {
            const long s = std::lrint(v * 32768.0f);
            return static_cast<int32_t>(s < -32768 ? -32768 : s > 32767 ? 32767 : s);
        };
        // The overlap faded from the last block's tail into this block on the samples, the sum shifted down
        // (towards minus infinity) by the count, as binkw32 does (the fade on floats rounds differently in about
        // half of the overlap's samples).
        const uint32_t count = overlap * blockChannels;
        const int shift = std::countr_zero(count);
        const uint32_t made = length - overlap;
        for (uint32_t i = 0; i < made; ++i) {
            for (uint32_t ch = 0; ch < blockChannels; ++ch) {
                int32_t v = sample(coefficients[ch][i]);
                if (!first && i < overlap) {
                    const auto j = static_cast<int32_t>(i * blockChannels + ch);
                    v = (sample(previous[ch][i]) * (static_cast<int32_t>(count) - j) + v * j) >> shift;
                }
                out.push_back(static_cast<int16_t>(v));
            }
        }
        for (uint32_t ch = 0; ch < blockChannels; ++ch) {
            std::memcpy(previous[ch].data(), coefficients[ch].data() + length - overlap, overlap * sizeof(float));
        }
        first = false;
        return true;
    }
};

AudioDecoder::AudioDecoder() : m(std::make_unique<State>()) {}
AudioDecoder::~AudioDecoder() = default;

void AudioDecoder::start(uint32_t sampleRate, uint32_t flags) {
    State& s = *m;
    s.dct = (flags & kAudioDct) != 0;
    s.rate = sampleRate;
    s.channels = (flags & kAudioStereo) != 0 ? 2 : 1;
    uint32_t rate = sampleRate;
    uint32_t bits = rate < 22050 ? 9 : rate < 44100 ? 10 : 11;
    if (s.dct) {
        s.blockChannels = s.channels;
    } else {
        // The RDFT's blocks hold the channels interleaved: one transform of their samples together.
        rate *= s.channels;
        s.blockChannels = 1;
        bits += static_cast<uint32_t>(std::bit_width(s.channels) - 1);
    }
    s.length = 1u << bits;
    s.overlap = s.length / 16;
    const uint32_t half = (rate + 1) / 2;
    const double length = s.length;
    s.root = static_cast<float>((s.dct ? length : 2.0) / (std::sqrt(length) * 32768.0));
    for (int i = 0; i < 96; ++i) s.quant[static_cast<size_t>(i)] = std::exp(static_cast<float>(i) * 0.15289164787221953823f) * s.root;
    for (s.bandCount = 1; s.bandCount < 25; ++s.bandCount) {
        if (half <= kCriticalFrequencies[s.bandCount - 1]) break;
    }
    s.bands[0] = 2;
    for (uint32_t i = 1; i < s.bandCount; ++i) s.bands[i] = (kCriticalFrequencies[i - 1] * s.length / half) & ~1u;
    s.bands[s.bandCount] = s.length;
    s.first = true;
    for (uint32_t ch = 0; ch < 2; ++ch) {
        s.coefficients[ch].assign(s.length, 0.0f);
        s.previous[ch].assign(s.overlap, 0.0f);
    }
    s.work.assign(s.length, {});
    s.fft.start(s.length);
    s.shifts.resize(s.length);
    for (uint32_t k = 0; k < s.length; ++k) {
        const double a = std::numbers::pi * k / (2.0 * s.length);
        s.shifts[k] = {static_cast<float>(std::cos(a)), static_cast<float>(std::sin(a))};
    }
}

uint32_t AudioDecoder::channels() const {
    return m->channels;
}

uint32_t AudioDecoder::sampleRate() const {
    return m->rate;
}

void AudioDecoder::decode(const uint8_t* data, size_t bytes, std::vector<int16_t>& out) {
    State& s = *m;
    if (s.length == 0 || bytes < 4) return;
    Bits b(data, bytes);
    b.skip(32);
    while (b.position() / 8 < bytes) {
        if (!s.block(b, out)) break;
        b.align32();
    }
}

// ---- binkw32's calls.

// A movie's sound: a looping DirectSound buffer of a second, fed by the sound thread from the samples decoded.
struct Native::Sound {
    IDirectSoundBuffer* buffer = nullptr;
    uint32_t bytes = 0;      // of the buffer
    uint32_t frameBytes = 0; // a sample of all channels
    std::vector<int16_t> pending;
    size_t pendingAt = 0;
    uint32_t writeAt = 0;   // where the next samples go in the buffer
    uint64_t written = 0;   // bytes put in the buffer
    uint64_t played = 0;    // bytes the buffer has played
    uint64_t quietTo = 0;   // the buffer silent up to here (in the count of written)
    uint32_t lastPlay = 0;
    bool playing = false;
    bool paused = false;
    int32_t volume = 32768;

    // Bytes into the buffer at the place, across its end.
    void put(uint32_t at, const uint8_t* data, uint32_t n) {
        void* p1 = nullptr;
        void* p2 = nullptr;
        DWORD n1 = 0, n2 = 0;
        if (FAILED(buffer->Lock(at, n, &p1, &n1, &p2, &n2, 0))) return;
        if (data != nullptr) {
            std::memcpy(p1, data, n1);
            if (p2 != nullptr) std::memcpy(p2, data + n1, n2);
        } else {
            std::memset(p1, 0, n1);
            if (p2 != nullptr) std::memset(p2, 0, n2);
        }
        buffer->Unlock(p1, n1, p2, n2);
    }

    void service() {
        if (buffer == nullptr || paused) return;
        if (playing) {
            DWORD play = 0, write = 0;
            if (FAILED(buffer->GetCurrentPosition(&play, &write))) return;
            played += (play + bytes - lastPlay) % bytes;
            lastPlay = play;
            if (written < played) {
                // The samples ran out: what comes next goes past the write cursor.
                written = played + (write + bytes - play) % bytes;
                writeAt = write;
                quietTo = written;
            }
        }
        const auto ahead = static_cast<uint32_t>(written - played);
        uint32_t room = bytes > ahead + frameBytes ? bytes - ahead - frameBytes : 0;
        room -= room % frameBytes;
        const auto waiting = static_cast<uint32_t>((pending.size() - pendingAt) * sizeof(int16_t));
        const uint32_t n = std::min(room, waiting);
        if (n != 0) {
            put(writeAt, reinterpret_cast<const uint8_t*>(pending.data() + pendingAt), n);
            pendingAt += n / sizeof(int16_t);
            writeAt = (writeAt + n) % bytes;
            written += n;
            if (pendingAt == pending.size()) {
                pending.clear();
                pendingAt = 0;
            }
        }
        // Silence after the samples, so a late frame is not old sound played again.
        const uint64_t quietEnd = written + std::min<uint64_t>(room - n, bytes / 4);
        if (quietTo < written) quietTo = written;
        if (quietEnd > quietTo) {
            put(static_cast<uint32_t>((writeAt + (quietTo - written)) % bytes), nullptr,
                static_cast<uint32_t>(quietEnd - quietTo));
            quietTo = quietEnd;
        }
        if (!playing && written != 0) {
            buffer->Play(0, 0, DSBPLAY_LOOPING);
            playing = true;
        }
    }
};

struct Native::Movie {
    HANDLE file = INVALID_HANDLE_VALUE;
    uint64_t fileSize = 0;
    MovieHeader header;
    std::array<uint8_t, kFieldBytes> fields{};
    VideoDecoder video;
    int track = -1;  // the header's track played
    AudioDecoder audio;
    std::unique_ptr<Sound> sound;
    std::vector<uint8_t> packet;
    std::vector<int16_t> samples;
    uint32_t decoded = 0;    // the frame in the pictures (0: none)
    uint32_t audioNext = 1;  // the frame whose sound comes next
    bool timing = false;     // the clock started (the first BinkWait)
    bool paused = false;
    int64_t base = 0;        // when frame `advanced` 0 was due
    uint64_t advanced = 0;   // frames gone on since
    int64_t pausedAt = 0;

    uint32_t field(uint32_t offset) const { return read32(fields.data() + offset); }
    void setField(uint32_t offset, uint32_t value) { std::memcpy(fields.data() + offset, &value, 4); }

    int64_t due() const {
        const double seconds = static_cast<double>(advanced) * header.rateDiv / header.rate;
        return base + static_cast<int64_t>(seconds * static_cast<double>(ticksPerSecond()));
    }

    bool readAt(uint64_t offset, void* to, uint32_t n) {
        LARGE_INTEGER at;
        at.QuadPart = static_cast<LONGLONG>(offset);
        DWORD got = 0;
        return SetFilePointerEx(file, at, nullptr, FILE_BEGIN) && ReadFile(file, to, n, &got, nullptr) && got == n;
    }
};

Native::Native() = default;

Native::~Native() {
    for (uint32_t i = 0; i < m_movies.size(); ++i) close(i + 1);
    if (m_soundThread.joinable()) {
        {
            std::lock_guard lock(m_soundLock);
            m_quit = true;
        }
        m_soundThread.join();
    }
}

bool Native::start(const std::filesystem::path&, std::string*) {
    return true;
}

// BinkSetSoundSystem(BinkOpenDirectSound, the game's DirectSound).
int32_t Native::setSoundSystem(uintptr_t directSound) {
    m_directSound = reinterpret_cast<void*>(directSound);
    return 1;
}

void Native::setSoundTrack(uint32_t count, const uint32_t* tracks) {
    m_tracks.assign(tracks, tracks + (tracks != nullptr ? count : 0));
}

Native::Movie* Native::find(uint32_t movie) const {
    return movie != 0 && movie <= m_movies.size() ? m_movies[movie - 1].get() : nullptr;
}

// BinkOpen: the header and the index read; the sound's track the first of BinkSetSoundTrack's found in the file (with
// BINKSNDTRACK), else the first. FrameNum 1, LastFrameNum -1 as binkw32 leaves them.
uint32_t Native::open(const std::filesystem::path& file, uint32_t flags) {
    auto m = std::make_unique<Movie>();
    m->file = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                          FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (m->file == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER size{};
    GetFileSizeEx(m->file, &size);
    m->fileSize = static_cast<uint64_t>(size.QuadPart);
    uint8_t start[44];
    std::vector<uint8_t> tables;
    bool good = m->readAt(0, start, sizeof(start)) && m->header.readStart(start);
    if (good) {
        tables.resize(m->header.tableBytes());
        good = m->readAt(sizeof(start), tables.data(), static_cast<uint32_t>(tables.size())) &&
               m->header.readTables(tables.data(), m->fileSize);
    }
    if (!good) {
        CloseHandle(m->file);
        log::warning("bink: {} is not a Bink 1 file this decoder reads", file.filename().string());
        return 0;
    }
    const MovieHeader& h = m->header;
    m->video.start(h);
    m->setField(kWidth, h.width);
    m->setField(kHeight, h.height);
    m->setField(kFrames, h.frames);
    m->setField(kFrameNum, 1);
    m->setField(kLastFrameNum, 0xFFFFFFFFu);
    m->setField(kFrameRate, h.rate);
    m->setField(kFrameRateDiv, h.rateDiv);
    m->setField(kNumTracks, static_cast<uint32_t>(h.tracks.size()));
    if ((flags & kSoundTrack) != 0) {
        for (uint32_t id : m_tracks) {
            for (size_t i = 0; i < h.tracks.size() && m->track < 0; ++i) {
                if (h.tracks[i].id == id) m->track = static_cast<int>(i);
            }
        }
    } else if (!h.tracks.empty()) {
        m->track = 0;
    }
    if (m->track >= 0 && m_directSound != nullptr) {
        const MovieHeader::Track& t = h.tracks[static_cast<size_t>(m->track)];
        m->audio.start(t.sampleRate, t.flags);
        startSound(*m);
    }
    for (uint32_t i = 0; i < m_movies.size(); ++i) {
        if (!m_movies[i]) {
            m_movies[i] = std::move(m);
            return i + 1;
        }
    }
    m_movies.push_back(std::move(m));
    return static_cast<uint32_t>(m_movies.size());
}

void Native::startSound(Movie& m) {
    auto* ds = static_cast<IDirectSound8*>(m_directSound);
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = static_cast<WORD>(m.audio.channels());
    format.nSamplesPerSec = m.audio.sampleRate();
    format.wBitsPerSample = 16;
    format.nBlockAlign = static_cast<WORD>(format.nChannels * 2);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    DSBUFFERDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
    desc.dwBufferBytes = format.nAvgBytesPerSec;
    desc.lpwfxFormat = &format;
    IDirectSoundBuffer* buffer = nullptr;
    if (FAILED(ds->CreateSoundBuffer(&desc, &buffer, nullptr)) || buffer == nullptr) {
        log::warning("bink: no sound buffer for a movie's track");
        return;
    }
    auto sound = std::make_unique<Sound>();
    sound->buffer = buffer;
    sound->bytes = desc.dwBufferBytes;
    sound->frameBytes = format.nBlockAlign;
    sound->put(0, nullptr, sound->bytes);
    std::lock_guard lock(m_soundLock);
    m_sounds.push_back(sound.get());
    m.sound = std::move(sound);
    if (!m_soundThread.joinable()) m_soundThread = std::thread([this] { serviceSounds(); });
}

// binkw32's sound timer: every 10 ms each movie's buffer gets the samples it has room for.
void Native::serviceSounds() {
    for (;;) {
        {
            std::lock_guard lock(m_soundLock);
            if (m_quit) return;
            for (Sound* s : m_sounds) s->service();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Native::close(uint32_t movie) {
    Movie* m = find(movie);
    if (m == nullptr) return;
    if (m->sound) {
        std::lock_guard lock(m_soundLock);
        std::erase(m_sounds, m->sound.get());
        m->sound->buffer->Stop();
        m->sound->buffer->Release();
        m->sound.reset();
    }
    CloseHandle(m->file);
    m_movies[movie - 1].reset();
}

// One frame read: each track's packet (its size, then its data), the sound's track decoded when its frame is the
// next to be heard, then the picture.
void Native::decodeFrame(Movie& m, uint32_t frame) {
    const MovieHeader& h = m.header;
    const uint32_t start = h.index[frame - 1] & ~1u;
    const uint32_t end = h.index[frame] & ~1u;
    if (end <= start) return;
    const uint32_t size = end - start;
    m.packet.resize(size);
    if (!m.readAt(start, m.packet.data(), size)) return;
    uint32_t pos = 0;
    for (size_t t = 0; t < h.tracks.size(); ++t) {
        if (pos + 4 > size) return;
        const uint32_t bytes = read32(m.packet.data() + pos);
        pos += 4;
        if (bytes > size - pos) return;
        if (static_cast<int>(t) == m.track && m.sound && frame == m.audioNext && bytes >= 4) {
            m.samples.clear();
            m.audio.decode(m.packet.data() + pos, bytes, m.samples);
            std::lock_guard lock(m_soundLock);
            m.sound->pending.insert(m.sound->pending.end(), m.samples.begin(), m.samples.end());
        }
        pos += bytes;
    }
    if (frame == m.audioNext) m.audioNext = frame >= h.frames ? 1 : frame + 1;
    m.video.decode(m.packet.data() + pos, size - pos);
}

// BinkDoFrame: the frame FrameNum names decoded, with the frames between it and the one decoded last (or from the
// key frame before it).
int32_t Native::doFrame(uint32_t movie) {
    Movie* m = find(movie);
    if (m == nullptr) return 0;
    const uint32_t target = m->field(kFrameNum);
    if (target == 0 || target > m->header.frames || target == m->decoded) return 0;
    uint32_t from = target;
    if (m->decoded != 0 && m->decoded < target) {
        from = m->decoded + 1;
    } else {
        while (from > 1 && (m->header.index[from - 1] & 1u) == 0) --from;
    }
    for (uint32_t f = from; f <= target; ++f) decodeFrame(*m, f);
    m->decoded = target;
    m->setField(kLastFrameNum, target);
    return 0;
}

// BinkNextFrame: on to the next frame, after the last back to the first.
void Native::nextFrame(uint32_t movie) {
    Movie* m = find(movie);
    if (m == nullptr) return;
    const uint32_t frame = m->field(kFrameNum);
    m->setField(kFrameNum, frame >= m->header.frames ? 1 : frame + 1);
    if (m->timing) ++m->advanced;
}

// BinkWait: 1 while the frame's time has not come (and while paused), else 0. The clock starts at the first call. A
// movie more than three frames behind is not caught up: its clock goes on from now, the frame at once and the next at
// the movie's pace. binkw32 measured so (a 25 fps movie stalled 100 ms or more goes on at 25 fps; stalled less, its
// one to three frames are caught up); every late frame let through at once played a menu's movie fast for seconds
// after a window's load.
int32_t Native::wait(uint32_t movie) {
    Movie* m = find(movie);
    if (m == nullptr) return 0;
    if (m->paused) return 1;
    const int64_t t = now();
    if (!m->timing) {
        m->timing = true;
        m->base = t;
        m->advanced = 0;
    }
    const int64_t due = m->due();
    if (t < due) return 1;
    const double frame = static_cast<double>(m->header.rateDiv) / m->header.rate;
    const auto late = static_cast<int64_t>(3.0 * frame * static_cast<double>(ticksPerSecond()));
    if (t - due > late) m->base += t - due;
    return 0;
}

int32_t Native::pause(uint32_t movie, int32_t on) {
    Movie* m = find(movie);
    if (m == nullptr) return 0;
    const bool paused = on != 0;
    if (paused != m->paused) {
        const int64_t t = now();
        if (paused) {
            m->pausedAt = t;
        } else {
            m->base += t - m->pausedAt;
        }
        m->paused = paused;
        if (m->sound) {
            std::lock_guard lock(m_soundLock);
            Sound& s = *m->sound;
            s.paused = paused;
            if (s.playing) {
                if (paused) {
                    s.buffer->Stop();
                } else {
                    s.buffer->Play(0, 0, DSBPLAY_LOOPING);
                }
            }
        }
    }
    return paused ? 1 : 0;
}

// BinkSetVolume: 0 .. 32768 (full) for the track of that id; a track the movie does not play is not heard from.
void Native::setVolume(uint32_t movie, uint32_t track, int32_t volume) {
    Movie* m = find(movie);
    if (m == nullptr || !m->sound || m->track < 0 || m->header.tracks[static_cast<size_t>(m->track)].id != track) return;
    Sound& s = *m->sound;
    if (s.volume == volume) return;
    s.volume = volume;
    LONG decibels = DSBVOLUME_MIN;
    if (volume > 0) {
        const double d = 2000.0 * std::log10(static_cast<double>(std::min(volume, 32768)) / 32768.0);
        decibels = static_cast<LONG>(std::max(d, static_cast<double>(DSBVOLUME_MIN)));
    }
    s.buffer->SetVolume(decibels);
}

// binkw32's conversion (measured against its pictures, exact): tables of each term cut toward 0 and added, Y below 16
// as 16: R = Y' + Cr x 1.596, G = Y' - Cb x 0.3918 - Cr x 0.813, B = Y' + Cb x 2.0173, Y' = (Y - 16) x 1.16438, then
// within 0 .. 255.
struct ColourTables {
    std::array<int, 256> y{}, r{}, gu{}, gv{}, b{};
    ColourTables() {
        for (int i = 0; i < 256; ++i) {
            const double c = i - 128;
            y[static_cast<size_t>(i)] = static_cast<int>((std::max(i, 16) - 16) * 1.16438);
            r[static_cast<size_t>(i)] = static_cast<int>(c * 1.596);
            gu[static_cast<size_t>(i)] = static_cast<int>(-c * 0.3918);
            gv[static_cast<size_t>(i)] = static_cast<int>(-c * 0.813);
            b[static_cast<size_t>(i)] = static_cast<int>(c * 2.0173);
        }
    }
};

// BinkCopyToBufferRect: the rectangle of the picture to the buffer at (destX, destY) in the surface type of the flags'
// low 4 bits; rows past destHeight are not written.
int32_t Native::copyToBufferRect(uint32_t movie, void* dest, int32_t pitch, uint32_t destHeight, uint32_t destX,
                                 uint32_t destY, uint32_t srcX, uint32_t srcY, uint32_t srcW, uint32_t srcH,
                                 uint32_t flags) {
    Movie* m = find(movie);
    if (m == nullptr || dest == nullptr) return 0;
    const uint32_t width = m->header.width, height = m->header.height;
    if (srcX >= width || srcY >= height || destY >= destHeight) return 0;
    const uint32_t w = std::min(srcW, width - srcX);
    const uint32_t h = std::min({srcH, height - srcY, destHeight - destY});
    const uint32_t type = flags & 0xF;
    const uint32_t bytes = type == kSurface24 || type == kSurface24R                         ? 3
                           : type == kSurface32 || type == kSurface32R || type == kSurface32A ||
                                     type == kSurface32RA                                     ? 4
                                                                                              : 2;
    const VideoDecoder& v = m->video;
    const uint8_t* ys = v.plane(0);
    const uint8_t* us = v.plane(1);
    const uint8_t* vs = v.plane(2);
    const uint8_t* as = v.plane(3);
    const ptrdiff_t yStride = v.stride(0), cStride = v.stride(1), aStride = v.stride(3);
    static const ColourTables t;
    for (uint32_t row = 0; row < h; ++row) {
        const uint32_t sy = srcY + row;
        auto* out = static_cast<uint8_t*>(dest) + static_cast<ptrdiff_t>(destY + row) * pitch +
                    static_cast<ptrdiff_t>(destX) * bytes;
        const uint8_t* yRow = ys + static_cast<ptrdiff_t>(sy) * yStride;
        const uint8_t* uRow = us + static_cast<ptrdiff_t>(sy >> 1) * cStride;
        const uint8_t* vRow = vs + static_cast<ptrdiff_t>(sy >> 1) * cStride;
        const uint8_t* aRow = as != nullptr ? as + static_cast<ptrdiff_t>(sy) * aStride : nullptr;
        for (uint32_t col = 0; col < w; ++col, out += bytes) {
            const uint32_t sx = srcX + col;
            const int y = t.y[yRow[sx]];
            const uint8_t cb = uRow[sx >> 1], cr = vRow[sx >> 1];
            const uint8_t r = clip(y + t.r[cr]);
            const uint8_t g = clip(y + t.gu[cb] + t.gv[cr]);
            const uint8_t b = clip(y + t.b[cb]);
            const uint8_t a = aRow != nullptr ? aRow[sx] : 0;  // binkw32: 0 without an alpha plane
            switch (type) {
            case kSurface24:
                out[0] = b, out[1] = g, out[2] = r;
                break;
            case kSurface24R:
                out[0] = r, out[1] = g, out[2] = b;
                break;
            case kSurface32:
            case kSurface32A:
                out[0] = b, out[1] = g, out[2] = r, out[3] = type == kSurface32A ? a : 0;  // binkw32 leaves the X byte 0
                break;
            case kSurface32R:
            case kSurface32RA:
                out[0] = r, out[1] = g, out[2] = b, out[3] = type == kSurface32RA ? a : 0;
                break;
            default: {
                uint16_t p;
                if (type == kSurface565) {
                    p = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
                } else if (type == kSurface4444) {
                    p = static_cast<uint16_t>(((a >> 4) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4));
                } else {
                    p = static_cast<uint16_t>(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
                    if (type == kSurface5551 && a >= 128) p |= 0x8000;
                }
                std::memcpy(out, &p, 2);
                break;
            }
            }
        }
    }
    return 0;
}

const uint8_t* Native::fields(uint32_t movie) {
    Movie* m = find(movie);
    return m != nullptr ? m->fields.data() : nullptr;
}

void Native::setField(uint32_t movie, uint32_t offset, uint32_t value) {
    if (Movie* m = find(movie); m != nullptr && offset + 4 <= kFieldBytes) m->setField(offset, value);
}

}  // namespace oj::bink
