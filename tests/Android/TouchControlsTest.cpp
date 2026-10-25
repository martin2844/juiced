#include "Android/TouchControls.h"
#undef NDEBUG
#include <cassert>
#include <iostream>
int main() {
    using C = oj::android::TouchControls;
    C c;
    c.update(1, .1f, .8f);
    c.update(2, .9f, .8f);
    assert(c.held()[C::Left] && c.held()[C::Throttle]);
    c.update(3, .05f, .8f);
    c.release(1);
    assert(c.held()[C::Left]); // another finger still holds the same key
    c.update(3, .25f, .8f);
    assert(!c.held()[C::Left] && c.held()[C::Right]);
    c.update(2, .5f, .5f);
    assert(!c.held()[C::Throttle]);
    c.update(5, .9f, .1f);
    assert(c.held()[C::Select]);
    c.update(9, .9f, .3f);
    c.update(10, .7f, .3f);
    assert(c.held()[C::Nitro] && c.held()[C::Handbrake]);
    c.update(11, .1f, .3f);
    c.update(12, .25f, .3f);
    assert(c.held()[C::GearDown] && c.held()[C::GearUp]);
    c.clear();
    for (bool held : c.held())
        assert(!held);
    c.update(7, -.1f, .9f);
    c.update(8, 1.1f, .9f);
    for (bool held : c.held())
        assert(!held); // letterbox/outside controls
    std::cout << "Touch controls: multitouch, drag, release, focus reset and bounds passed\n";
}
