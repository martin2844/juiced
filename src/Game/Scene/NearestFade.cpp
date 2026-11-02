#include "Game/Scene/NearestFade.h"

#include <algorithm>

namespace oj::scene {

std::vector<float> rankFade(const std::vector<float>& distances, size_t keep, float band) {
    std::vector<float> out(std::min(distances.size(), keep), 1.0f);
    if (distances.size() <= keep) return out;
    const float left = distances[keep];
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = band > 0.0f ? std::clamp((left - distances[i]) / band, 0.0f, 1.0f) : 1.0f;
    }
    return out;
}

float distanceFade(float distance, float full, float reach) {
    if (distance <= full) return 1.0f;
    if (distance >= reach) return 0.0f;
    return (reach - distance) / (reach - full);
}

}  // namespace oj::scene
