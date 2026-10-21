#pragma once
// The accessibility shortcuts (StickyKeys, ToggleKeys, FilterKeys) are switched off while the game runs, so that
// holding Shift or Num Lock does not open their dialogs; the saved state comes back at the end.

namespace oj {

class SystemKeys {
public:
    SystemKeys(const SystemKeys&) = delete;
    SystemKeys& operator=(const SystemKeys&) = delete;

    // enabled = false: nothing is changed (test runs).
    explicit SystemKeys(bool enabled);
    ~SystemKeys();

private:
    struct Saved;
    Saved* m_saved = nullptr;
};

}  // namespace oj
