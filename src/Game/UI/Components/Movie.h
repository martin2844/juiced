#pragma once
// A Movie element (0x005AA290): the movie it plays.

#include <cstdint>
#include <string>

namespace oj::ui {

struct MovieState {
    // Movies (0x005AA290)
    std::string name;        // +0xC0: Movie <name>
    bool looped = false;      // +0x100: Looped
    bool playFast = false;    // +0x104: PlayFast, or a movie named "frontend"
    bool muting = false;      // +0x108: muted under a message window
    uint32_t muteFrames = 0;  // +0x10C: the draws before the muted movie starts again
    float volume = 1.0f; // +0x114
};

}  // namespace oj::ui
