// Integration tests: Movies.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Base/GameFiles.h"
#include "Core/Network/GameSpy/GameSpyHttp.h"
#include "Game/Media/BinkLibrary.h"

#include <cstdio>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// The game's movies through its binkw32.dll (OpenJuice's own decoder in the 64-bit build): opened by a file
// handle with the language's sound track as the stock code opens them, a frame decoded and copied.
OJ_INTEGRATION_TEST(Movies, Bink) {
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    oj::bink::Library* bink = oj::bink::Library::get(game.root());
    bool hub = false, leader = false;
    if (bink != nullptr) {
        const uint32_t track = 7;
        bink->setSoundTrack(1, &track);
        const uint32_t m = bink->open(game.root() / "movies" / "hub_01.bik", 0x804000);
        std::vector<uint8_t> pixels(640 * 480 * 4, 0);
        if (m != 0) {
            for (int i = 0; i < 30; ++i) {
                bink->doFrame(m);
                if (i < 29) bink->nextFrame(m);
            }
            bink->copyToBufferRect(m, pixels.data(), 640 * 4, 480, 0, 0, 0, 0, 640, 480, 3u | 0x80000000u);
            uint64_t sum = 0;
            for (uint8_t v : pixels) sum += v;
            std::printf("     hub_01: %ux%u, %u frames, frame %u, %u tracks, pixel sum %llu\n",
                        bink->field(m, 0), bink->field(m, 4), bink->field(m, 8), bink->field(m, 0xC),
                        bink->field(m, 0xF0), static_cast<unsigned long long>(sum));
            hub = bink->field(m, 0) == 640 && bink->field(m, 4) == 480 && bink->field(m, 0xC) == 30 &&
                  bink->field(m, 0xF0) == 0 && sum != 0;
            bink->close(m);
        }
        const uint32_t b = bink->open(game.root() / "movies" / "Biggi" / "BIGGI_ACCEPT_BET1.bik", 0x804000);
        if (b != 0) {
            leader = bink->field(b, 0xF0) == 8;
            bink->close(b);
        }
    }
    check(hub && leader, "movies: the game's binkw32.dll opens hub_01 and a leader's clip, decodes frame 30");
}
