#pragma once
// The fly-by before a race (docs/spec/15-tracks.md, "The fly-by"): the shots of scripts\<route>_intro.cam played on
// the race's camera (0x0041C530 each frame, the race's camera step 0x00405BE0 around it).

#include "Core/Tracks/Track.h"
#include "Render/Camera.h"

#include <string>
#include <vector>

namespace oj::scene {

class RaceIntro {
public:
    // scripts\<route>_intro.cam (the route in lower case); false when the route has none (the race then starts its
    // count of 6 s at once).
    bool load(const std::string& route);
    // A frame of the fly-by and the camera step: the camera's place, field of view and far plane. false when the
    // fly-by has ended this frame or before (the count of 6 s starts).
    bool update(float dt, render::Camera& camera, bool wideScreen);
    // The black over the view while a shot comes in: 1 at its start, 0 after a second.
    float fade() const { return m_fadeLeft > 0.0f ? m_fadeLeft : 0.0f; }
    // The race camera's object at +0x1D0 (made by 0x0041C350 with everything 0) cleared as the fly-by ends
    // (0x0041C72C) and as the views behind and on the car, the replay's next car, the TV, the ground and the sweep
    // before the start reset (0x0040206E, 0x00403421, 0x00403E7C, 0x00403F81, 0x00404E89, 0x00405317) and after a
    // fly-by is read (0x00405C33): its +0x00, shot (+0x04), +0x18, key (+0x1C), fade (+0x24) and wait (+0x28) 0, and
    // the first player's object it took (+0x20, its +0x0C) stopped; OpenJuice keeps no such object.
    void clear();

private:
    // 0x0041C450: the camera at a position with a rotation (w, x, y, z), the field of view kept for the step.
    void place(const std::array<float, 4>& position, const std::array<float, 4>& rotation, float fov,
               render::Camera& camera);

    std::vector<IntroShot> m_shots;  // +0x08
    uint32_t m_shot = 0;             // +0x04
    float m_index = 0.0f;            // +0x1C: the key and the way to the next
    float m_fadeLeft = 0.0f;         // +0x24
    float m_waited = 0.0f;           // +0x28
    float m_fov = 0.0f;              // the race camera's +0x1E4
    bool m_done = false;
};

}  // namespace oj::scene
