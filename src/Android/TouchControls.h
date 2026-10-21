#pragma once
#include <array>
#include <cstdint>
#include <map>
namespace oj::android {
// Normalized landscape controls. Every finger retains an independent binding;
// releasing one finger must not release a key still held by another.
class TouchControls {
  public:
    enum Key { Left, Right, Throttle, Brake, Select, Back, Count };
    void update(int64_t finger, float x, float y);
    void release(int64_t finger) {
        fingers.erase(finger);
    }
    void clear() {
        fingers.clear();
    }
    std::array<bool, Count> held() const;

  private:
    std::map<int64_t, int> fingers;
};
} // namespace oj::android
