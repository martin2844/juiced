#pragma once
// The fixed build's nearest lights kept as they come and go (the user, 2026-10-28: by night the street lamps' light in
// the haze, its glare on the road and the cars' shadows from them came and went at once about the camera): of the
// lights found nearest a place, those taken are weighed by their lead over the nearest left out, so one passed by the
// next goes out as it is passed, and by their distance's fade toward the reach they are looked for within.

#include <cstddef>
#include <vector>

namespace oj::scene {

// The weights of the first `keep` of `distances` (all the lights found, nearest first): each 1 by `band` nearer than
// the nearest left out (the `keep`-th, from 0), 0 as near as it, linear between; all 1 with none left out.
std::vector<float> rankFade(const std::vector<float>& distances, size_t keep, float band);
// 1 within `full`, 0 from `reach` on, linear between.
float distanceFade(float distance, float full, float reach);

}  // namespace oj::scene
