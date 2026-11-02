// Integration tests: the fixed build's picture of a replay in loadreplay (Game/UI/WindowFixes, the district's picture
// of Frontend/Saves/LoadReplay): the game's window and pictures, the window over replays of a folder of the tests
// through a front end of the tests (Framework/Fakes/FakeSaveHost.h), drawn on a Direct3D 9 device.

#include "Framework/Test.h"
#include "Framework/Fakes/FakeSaveHost.h"
#include "Framework/Fakes/NullRenderDevice.h"
#include "Framework/Fixtures.h"
#include "Framework/RealDevice.h"

#include "Core/Base/GameFiles.h"
#include "Core/Data/ExeTables.h"
#include "Core/Data/GameData.h"
#include "Core/Data/Language.h"
#include "Core/Formats/Ddt.h"
#include "Game/Frontend/Saves/LoadReplay.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/WindowFixes.h"
#include "Render/Display.h"

#include <windows.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace {

using oj::test::check;

int indexOf(const oj::ui::Window& w, const char* name) {
    for (size_t i = 0; i < w.elements.size(); ++i) {
        if (w.elements[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

// A back buffer of the size asked.
class SizedDevice : public oj::test::NullRenderDevice {
public:
    SizedDevice(uint32_t w, uint32_t h) : m_w(w), m_h(h) {}
    uint32_t width() const override { return m_w; }
    uint32_t height() const override { return m_h; }

private:
    uint32_t m_w, m_h;
};

// The game's mode for the test, the stock's after it.
struct ModeFor {
    explicit ModeFor(oj::render::AspectMode m) { oj::render::setAspectMode(m); }
    ~ModeFor() { oj::render::setAspectMode(oj::render::AspectMode::Stock); }
};

// The textures of the quads drawn.
class TexturesDevice : public oj::test::NullRenderDevice {
public:
    std::vector<oj::render::TextureId> drawn;
    void drawQuad(const oj::render::QuadVertex (&)[4], oj::render::TextureId texture, oj::render::Blend,
                  bool) override {
        drawn.push_back(texture);
    }
};

using oj::test::FakeSaveHost;

// A replay's folder of the save device: its USERDATA (the cars, the race's type, the track, the route, the date, the
// driver) written at the time given.
void writeReplay(const std::filesystem::path& save, const wchar_t* folder, int32_t cars, const char* track,
                 std::filesystem::file_time_type written) {
    std::error_code ec;
    std::filesystem::create_directories(save / folder, ec);
    std::vector<uint8_t> data(0x79, 0);
    const int32_t type = 1;
    std::memcpy(data.data(), &cars, 4);
    std::memcpy(data.data() + 4, &type, 4);
    std::memcpy(data.data() + 8, track, std::strlen(track));
    std::memcpy(data.data() + 0x28, "R1", 2);
    std::memcpy(data.data() + 0x48, "10/26/2026 12:00", 16);
    const char16_t driver[] = u"Test";
    std::memcpy(data.data() + 0x59, driver, 8);
    const std::filesystem::path file = save / folder / L"USERDATA";
    std::ofstream(file, std::ios::binary)
        .write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    std::filesystem::last_write_time(file, written, ec);
}

// A capture's pixel within a few levels of a colour (0xRRGGBB) in each channel.
bool within(uint32_t pixel, uint32_t colour, int levels) {
    for (int shift = 0; shift < 24; shift += 8) {
        const int a = static_cast<int>((pixel >> shift) & 0xFF), b = static_cast<int>((colour >> shift) & 0xFF);
        if (a - b > levels || b - a > levels) return false;
    }
    return true;
}

}  // namespace

// loadreplay's picture right of the details (their texts end at 0.521), level with them, its frame (ui_replayborder)
// square on a 4:3 screen and after it, drawn over it, the picture inside it up to its line (4 of 128 texels in). The
// window's file has neither.
OJ_INTEGRATION_TEST(ReplayPicture, WindowElements) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::ui::Window w;
    const bool read = w.load(std::filesystem::path("ui") / "windows" / "loadreplay.ui");
    check(read, "loadreplay.ui read");
    if (!read) return;
    check(indexOf(w, oj::ui::kReplayPicture) < 0 && indexOf(w, oj::ui::kReplayPictureFrame) < 0,
          "the file: no picture of the replay");
    oj::ui::fixWindow("loadreplay", w);
    const size_t count = w.elements.size();
    oj::ui::fixWindow("loadreplay", w);
    const int picture = indexOf(w, oj::ui::kReplayPicture);
    const int frame = indexOf(w, oj::ui::kReplayPictureFrame);
    const int track = indexOf(w, "INFO_RACETRACK");
    const bool made = picture >= 0 && frame >= 0 && track >= 0 && w.elements.size() == count;
    check(made, "fixed: the picture and its frame, once");
    if (!made) return;
    const oj::ui::Element& p = w.elements[static_cast<size_t>(picture)];
    const oj::ui::Element& f = w.elements[static_cast<size_t>(frame)];
    const oj::ui::Element& t = w.elements[static_cast<size_t>(track)];
    check(f.kind == oj::ui::ElementKind::Box && f.x > t.x + t.w && f.x + f.w < 1.0f && f.y > 0.209f &&
              f.y + f.h < 0.709f && std::fabs(f.w * 640.0f - f.h * 480.0f) < 0.01f && frame > picture,
          "the frame right of the details, level with them, square, after the picture");
    const float in = 4.0f / 128.0f;
    check(std::fabs(p.x - (f.x + f.w * in)) < 1e-5f && std::fabs(p.y - (f.y + f.h * in)) < 1e-5f &&
              std::fabs(p.w - f.w * (1.0f - 2.0f * in)) < 1e-5f && std::fabs(p.h - f.h * (1.0f - 2.0f * in)) < 1e-5f,
          "the picture inside the frame up to its line");
}

// The picture and its frame on a 16:9 screen of the fixed build's wide screens (1920 x 1080, the screen's own shape):
// square, the frame 0.3 of the 4:3 frame's width (1440) across, the picture inside its line; right of the details.
OJ_INTEGRATION_TEST(ReplayPicture, WideScreen) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    ModeFor mode(oj::render::AspectMode::Auto);
    SizedDevice device(1920, 1080);
    oj::ui::Screen s(device, *oj::Language::byName("english"));
    if (!s.load("loadreplay")) return;
    float fx0 = 0, fy0 = 0, fx1 = 0, fy1 = 0, px0 = 0, py0 = 0, px1 = 0, py1 = 0, tx0 = 0, ty0 = 0, tx1 = 0, ty1 = 0;
    const bool placed = s.pixelRect(oj::ui::kReplayPictureFrame, fx0, fy0, fx1, fy1) &&
                        s.pixelRect(oj::ui::kReplayPicture, px0, py0, px1, py1) &&
                        s.pixelRect("INFO_RACETRACK", tx0, ty0, tx1, ty1);
    check(placed, "the picture, its frame and the details placed");
    if (!placed) return;
    const float in = (fx1 - fx0) * 4.0f / 128.0f;
    check(std::fabs((fx1 - fx0) - 0.3f * 1440.0f) < 0.5f && std::fabs((fx1 - fx0) - (fy1 - fy0)) < 0.5f,
          "the frame square, 432 pixels (" + std::to_string(fx1 - fx0) + " x " + std::to_string(fy1 - fy0) + ")");
    check(std::fabs(px0 - (fx0 + in)) < 0.5f && std::fabs(px1 - (fx1 - in)) < 0.5f &&
              std::fabs(py0 - (fy0 + in)) < 0.5f && std::fabs(py1 - (fy1 - in)) < 0.5f && fx0 > tx1 && fx1 < 1920.0f,
          "the picture inside the frame's line, right of the details");
}

// The window listing two replays, the latest's track of no city place: listed, its details shown without the picture
// (the details' group shown with it, the picture hidden again); Downtown's highlighted: its picture and frame shown,
// its picture drawn; back to the first, both hidden again.
OJ_INTEGRATION_TEST(ReplayPicture, ShownByReplay) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const std::filesystem::path save = std::filesystem::path(exe).parent_path() / L"ReplayPictureTest" / L"SAVE";
    std::error_code ec;
    std::filesystem::remove_all(save.parent_path(), ec);
    const auto now = std::filesystem::file_time_type::clock::now();
    writeReplay(save, L"R20261026110000", 4, "DOWNTOWN ANGEL CITY", now - std::chrono::hours(1));
    writeReplay(save, L"R20261026120000", 2, "NOWHERE", now);
    oj::GameData data(oj::test::gameFiles().root());
    data.tables = oj::ExeTables::stock();
    data.saveDevice.setFolder(save);
    FakeSaveHost host;
    host.data = &data;
    host.texts.load(std::filesystem::path("scripts") / "english.txt");
    TexturesDevice device;
    oj::ui::Screen s(device, host.lang);
    if (!s.load("loadreplay")) return;
    host.base = &s;
    oj::frontend::LoadReplay window;
    window.enter(host, s);
    bool listed = false;
    for (int frame = 0; frame < 200 && !listed; ++frame) {
        window.update(host, s, 0);
        listed = s.menuSize("menu") == 2 && !s.hidden("INFO_RACETRACK");
    }
    check(listed, "the two replays listed, their details shown");
    if (!listed) return;
    window.update(host, s, 0);
    const auto pictureShown = [&] {
        return !s.hidden(oj::ui::kReplayPicture) && !s.hidden(oj::ui::kReplayPictureFrame);
    };
    const auto pictureHidden = [&] {
        return s.hidden(oj::ui::kReplayPicture) && s.hidden(oj::ui::kReplayPictureFrame);
    };
    const size_t first = s.selectedMenuItem("menu");
    const auto& replays = data.saveDevice.replays();
    const size_t nowhere = static_cast<size_t>(s.menuItemValue("menu", first));
    check(nowhere < replays.size() && replays[nowhere].track == "NOWHERE" && pictureHidden(),
          "the latest (NOWHERE) highlighted first: no picture, nor its frame");
    s.selectMenuItem("menu", first == 0 ? 1 : 0);
    window.update(host, s, 0);
    device.drawn.clear();
    s.draw(1.0f);
    const oj::render::TextureId downtown = s.texture("ui_replay_downtown");
    bool drawn = false;
    for (const oj::render::TextureId id : device.drawn) drawn = drawn || (downtown != 0 && id == downtown);
    check(pictureShown() && drawn, "Downtown's replay: its picture (ui_replay_downtown) drawn in its frame");
    s.selectMenuItem("menu", first);
    window.update(host, s, 0);
    check(pictureHidden(), "back to NOWHERE's: both hidden");
    std::filesystem::remove_all(save.parent_path(), ec);
}

// Each city place's replay picture: ui_replay_<district> of the game's files (the speedway's Angel Westside's), eight
// pictures for the nine places; the track's name compared without regard to case; none for a track of no place.
OJ_INTEGRATION_TEST(ReplayPicture, Districts) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::test::NullRenderDevice device;
    oj::ui::Screen s(device, *oj::Language::byName("english"));
    const oj::ExeTables& t = oj::ExeTables::stock();
    std::set<std::string> pictures;
    bool each = t.cityLocations.size() == 9;
    for (const oj::ExeTables::CityLocation& place : t.cityLocations) {
        const std::string picture = oj::frontend::districtPicture(s, t, place.name);
        each = each && picture == "ui_replay_" + place.texture.substr(3);
        pictures.insert(picture);
    }
    check(each && pictures.size() == 8,
          "the nine places' pictures, eight of them (" + std::to_string(pictures.size()) + ")");
    check(oj::frontend::districtPicture(s, t, "southside beach") == "ui_replay_southside" &&
              oj::frontend::districtPicture(s, t, "ANGEL SUPERSPEEDWAY") == "ui_replay_angelwestside",
          "a track's name in any case; the speedway's the Angel Westside's");
    check(oj::frontend::districtPicture(s, t, "NOWHERE").empty() && oj::frontend::districtPicture(s, t, "").empty(),
          "a track of no place: none");
}

// The window drawn with Downtown's picture over black (640 x 480): the picture's middle the mean of its four middle
// texels (drawn smoothed), the frame's line white; with the picture hidden neither drawn there.
OJ_INTEGRATION_TEST(ReplayPicture, Drawn) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const auto file = oj::GameFiles(std::filesystem::current_path())
                          .read(std::filesystem::path("ui") / "textures" / "ui_replay_downtown.ddt");
    oj::Ddt ddt;
    const bool picture = file && ddt.parse(*file) && ddt.width == 64 && ddt.height == 64 &&
                         ddt.format == oj::Ddt::kA8R8G8B8 && ddt.pixels.size() >= 64 * 64 * 4;
    check(picture, "ui_replay_downtown.ddt: 64 x 64, 32 bits");
    if (!picture) return;
    uint32_t mean = 0;
    for (int shift = 0; shift < 24; shift += 8) {
        uint32_t sum = 0;
        for (const size_t texel : {31 * 64 + 31, 31 * 64 + 32, 32 * 64 + 31, 32 * 64 + 32}) {
            uint32_t argb = 0;
            std::memcpy(&argb, ddt.pixels.data() + texel * 4, 4);
            sum += (argb >> shift) & 0xFF;
        }
        mean |= ((sum + 2) / 4) << shift;
    }
    oj::test::HiddenWindow window(640, 480);
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, oj::test::realDesc(window.get()), &error);
    check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return;
    oj::ui::Screen s(*device, *oj::Language::byName("english"));
    const bool loaded = s.load("loadreplay");
    check(loaded, "loadreplay loaded");
    if (!loaded) return;
    s.showContaining("INFO_", false);
    const std::string name = oj::frontend::districtPicture(s, oj::ExeTables::stock(), "DOWNTOWN ANGEL CITY");
    s.setTexture(oj::ui::kReplayPicture, name);
    const auto frame = [&] {
        device->clear(0x1, 0xFF000000u, 1.0f, 0);
        s.draw(4.0f);
    };
    const oj::test::Bitmap shown = oj::test::captureFrame(*device, frame);
    s.hideContaining(oj::ui::kReplayPicture);
    const oj::test::Bitmap hidden = oj::test::captureFrame(*device, frame);
    const bool captured = name == "ui_replay_downtown" && shown.width == 640 && hidden.width == 640;
    check(captured, "Downtown's picture; both frames captured");
    if (!captured) return;
    // The frame: 0.610, 0.259, 0.300 x 0.400 of the screen; its line 4 .. 10 of 128 in.
    const float x0 = 0.610f * 640.0f, y0 = 0.259f * 480.0f, size = 0.300f * 640.0f;
    const int middleX = static_cast<int>(x0 + size / 2.0f), middleY = static_cast<int>(y0 + size / 2.0f);
    const int lineX = static_cast<int>(x0 + size * 7.0f / 128.0f);
    char text[96];
    std::snprintf(text, sizeof text, "the middle %06X (the texels' mean %06X), the line %06X",
                  shown.pixel(middleX, middleY), mean, shown.pixel(lineX, middleY));
    check(within(shown.pixel(middleX, middleY), mean, 6) && within(shown.pixel(lineX, middleY), 0xFFFFFFu, 6),
          std::string("shown: ") + text);
    std::snprintf(text, sizeof text, "the middle %06X, the line %06X", hidden.pixel(middleX, middleY),
                  hidden.pixel(lineX, middleY));
    check(!within(hidden.pixel(middleX, middleY), mean, 6) && !within(hidden.pixel(lineX, middleY), 0xFFFFFFu, 6),
          std::string("hidden: neither the picture nor its frame: ") + text);
}
