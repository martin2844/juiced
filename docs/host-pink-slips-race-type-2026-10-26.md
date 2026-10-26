# Pink-slip race-type navigation fix — 2026-10-26

Android .8 / versionCode 9 fixes a reachable-UI bug in .7. The retail
CMSHostMeetingSetupRace screen contains a TYPE button, but Route's upward
navigation does not reach it. The existing handler supported changing physical
race types in pink mode, but ordinary controller navigation could not select it.

For pink-slip setup, enterRace now connects Route/up to Type, Type/down to Route,
and initially selects Type. Normal hosting retains Route as its initial selection
and no upward link. Left/right selects Point-to-Point, Circuit or Sprint using the
existing handler. Respect requirements and car-wager settlement are unchanged.

The regression test first reproduced the bug (eight Up presses remained at ROUTE).
It now uses moveHighlight followed by updateRace, matching frontend input order,
to test down/up navigation and selection of all three race types. It verifies that
the chosen Point-to-Point type is retained in both the created event and its round,
while pink-slip flags, two-racer setup and native settlement checks still pass.

Validation:
- android_host_pink_check passed with retail assets and an in-memory test career.
- GLES screenshot inspected: out/validation/android/pink-type-ui/pink-race.png.
- Signed arm64 release build passed and installed with adb install -r on RP5.
- APK: out/releases/android8/OpenJuice-1.02.5-android.8-arm64.apk.
- Build/test logs: out/validation/android/pink-type-*.log.
- No save files, mod enable/disable markers or package collection changed.
- No manually driven full race performed for this navigation fix.
