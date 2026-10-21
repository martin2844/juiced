#pragma once
// The starting grid's order of the race's cars (0x004975A0, docs/spec/18-racers.md).

#include "Core/Race/RaceKinds.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace oj {

// 0x004976A5: the grid point of car i of the race's `count` cars off the network: in a career race (mode 2) the i-th;
// else (an arcade, custom, split screen or attract race) the count less i less 1, so the first cars made (the players')
// start at the back; a sprint's (type 6) moved on by its heat (-1 as 0) within the count.
inline size_t startingGridIndex(RaceMode mode, RaceType type, int32_t heat, size_t count, size_t i) {
    if (count == 0) return 0;
    size_t index = mode == RaceMode::Career ? i : count - i - 1;
    if (type == RaceType::Sprint) index = (index + static_cast<size_t>(std::max(heat, 0))) % count;
    return index;
}

}  // namespace oj
