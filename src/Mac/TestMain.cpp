#include "Core/Base/Text.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>
int wmain(int, wchar_t **);
int main(int argc, char **argv) {
    std::vector<std::wstring> strings;
    for (int i = 0; i < argc; i++)
        strings.push_back(oj::toWide(argv[i]));
    std::vector<wchar_t *> args;
    for (auto &s : strings)
        args.push_back(s.data());
    // Upstream audio regressions now also run against the native SDL mixer.
    // A dummy sink makes these deterministic and inaudible.
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_AUDIO)) return 1;
    const int result = wmain(argc, args.data());
    SDL_Quit();
    return result;
}
