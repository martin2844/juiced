#pragma once
// The debug pages of the stock engine (docs/spec/01-startup.md, "Debug pages"): a manager of pages, one of which the
// renderer's frame draws while the pages are visible, the profiler with its page and the memory page. The game's start
// makes all three; nothing on the PC makes the pages visible, so none is ever drawn. The profiler's timers run every
// frame all the same. Also the stock's other debug leftovers of the game object: the pool's statistics.

#include <array>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace oj::render {
class IRenderDevice;
}

namespace oj::debug {

// The font the pages draw with: the game object's +0x74, which nothing sets on the PC (0, 0x0040BB21). Its methods as
// the pages call them.
class Font {
public:
    virtual ~Font() = default;
    virtual uint32_t width(const std::wstring& text) const = 0;  // +0x08
    virtual uint32_t height() const = 0;                         // +0x0C (with 0, 0)
    // +0x14: a text at a place (pixels), in a colour (ARGB), flags 0.
    virtual void draw(const float place[2], const wchar_t* text, uint32_t argb, uint32_t flags) const = 0;
};

// The game's 2D view ([game + 0x70], 0x00517850 made by 0x00518AE0): the screen in pixels, its rectangle the back
// buffer's.
struct View {
    render::IRenderDevice* device = nullptr;

    // 0x00517310: its rectangle's corners (+0x14: x, y, width, height; here 0, 0 and the back buffer's size).
    void rect(float& x0, float& y0, float& x1, float& y1) const;
    // 0x00516880 / 0x00516D20: the view's scene begun and ended (the device's frame is already open here).
    void begin() const {}
    void end() const {}
    // 0x00517A60: a line between two points in one colour (the 2D vertices: x, y, colour; a line list).
    void line(const float a[2], const float b[2], uint32_t argb) const;
    // 0x00517C20: a rectangle from the corner a to the corner b, the corners' colours (a's, then across, then down);
    // a's x and y within 2 of an edge of the screen (the renderer's +0x198, +0x19C) put on it (0x00517730,
    // 0x005177C0: -0.5 at 0).
    void fill(float a[2], const float b[2], uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3) const;
};

class Pages;

// A page (0x18 bytes): what draws it, with the page's values as its arguments.
struct Page {
    using Draw = void (*)(void* data, const View* view, const Font* font, void* platform);
    Draw draw = nullptr;          // +0x00
    void* data = nullptr;         // +0x04
    const View* view = nullptr;   // +0x08
    const Font* font = nullptr;   // +0x0C
    void* platform = nullptr;     // +0x10
    uint32_t unused = 0;          // +0x14: cleared when added, read by nothing
};

// The pages' manager ([game + 0x78] = [0x0075F7A0], 0x24 bytes, vtable 0x006FD934).
class Pages {
public:
    // 0x0059CBC0: no page, hidden; the view, the font and the platform every page takes by default.
    Pages(const View* view, const Font* font, void* platform);
    ~Pages();  // 0x0059CC70

    // 0x0059CB30: the game's one manager made, else the one there counted again (+0x04).
    static Pages* make(const View* view, const Font* font, void* platform);
    // Its count lowered; at 0 it goes.
    static void release();

    // +0x04 (0x0059CCB0): a page at the end of the list; the values not given (0) the manager's.
    Page* add(Page::Draw draw, void* data, const View* view = nullptr, const Font* font = nullptr,
              void* platform = nullptr);
    // +0x08 (0x0059CD90): shown (+0x1C) or not; shown with pages and none current, the first. The old value.
    bool setVisible(bool visible);
    // +0x0C (0x0059CE30): shown (or forced) the current page the next one, past the last the first.
    void next(bool force);
    // +0x10 (0x0059CE60): shown, the current page the one before, before the first the last.
    void previous();
    // +0x14 (0x0059CDD0): a page taken off; the current one first goes to the next (forced). 0.
    int remove(Page* page);
    // +0x18 (0x0059CB20).
    bool visible() const { return m_visible; }
    // +0x1C (0x0059CE80): a line of text at a place in a colour, in the font given or the manager's.
    void line(const char* text, const float place[2], const uint32_t* argb, const Font* font) const;

    // The renderer's frame (0x00514B5A): while shown, the current page drawn.
    void drawCurrent() const;

private:
    uint32_t m_count = 1;            // +0x04
    std::list<Page*> m_pages;        // +0x0C
    std::list<Page*>::iterator m_current;  // +0x20: the list's end when there is none
    const View* m_view = nullptr;    // +0x10
    const Font* m_font = nullptr;    // +0x14
    void* m_platform = nullptr;      // +0x18
    bool m_visible = false;          // +0x1C
};

// [0x0075F7A0]: the game's manager, made at the game's start (0x0040C68D); none before.
Pages* pages();

// The profiler ([0x0075F7A4], 0xC58 bytes, vtable 0x006FD954, made by 0x0059CED0 / 0x0059D0B0 at the game's start,
// 0x0040C6BB): 30 named timers of the platform's timer (its low 32 bits, 0x005A2370), timer 0 the frame ("CPU Frame",
// "VU1 Frame" for the second column, which the PC never makes, [0x0075F7A8]); a page of the manager draws them
// (0x0059CF60, the callback 0x0059D080 → 0x0059D2B0).
class Profiler {
public:
    static constexpr int kTimers = 30;
    // A timer (0x68 bytes from +0x14).
    struct Timer {
        uint64_t total = 0;      // +0x18: the ticks it ran, summed
        float maximum = 0.0f;    // +0x20: the most of a frame (%) it took
        uint32_t start = 0;      // +0x24
        uint32_t end = 0;        // +0x28
        uint32_t starts = 0;     // +0x2C
        float average = 0.0f;    // +0x30: of a frame (%), smoothed
        int32_t parent = 0;      // +0x34: the timer that ran when it started
        bool shown = true;       // +0x38
        std::string name;        // +0x3C
    };

    // 0x0059D0B0: column 0 "CPU Frame", else "VU1 Frame"; its page made (0x0059CF60).
    Profiler(void* platform, const View* view, const Font* font, uint32_t column);
    ~Profiler();  // 0x0059D1A0

    // 0x0059D220: a named timer (its index), 0 when the 30 are taken.
    int add(const char* name);
    // 0x0059D270: a timer started (the running one its parent).
    void start(int timer);
    // The callers' stop (0x0040CE1F, 0x004260A1): the running timer's end and total; but for timer 0 its start and
    // end made relative to timer 0's start; its parent running again.
    void stop();
    // The renderer's frame (0x00514B28): timer 0's end and total, the running timer left as it is.
    void stopFrame();
    // 0x0059D2B0: the page.
    void draw();

    const Timer& timer(int i) const { return m_timers[static_cast<size_t>(i)]; }

private:
    void* m_platform = nullptr;      // +0x08
    const View* m_view = nullptr;    // +0x0C
    const Font* m_font = nullptr;    // +0x10
    std::array<Timer, kTimers> m_timers{};
    int32_t m_current = 0;           // +0xC48
    int32_t m_count = 1;             // +0xC4C
    uint32_t m_column = 0;           // +0xC50
    struct ProfilerPage* m_page = nullptr;  // +0xC54
};

// [0x0075F7A4]: the game's profiler, made at the game's start; none before.
Profiler* profiler();

// The trackers of allocations the memory page reads ([0x0075F7EC] .. [0x0075F808]): nothing on the PC registers one.
struct Allocation {
    uint32_t size = 0;           // +0x0C
    uint32_t tag = 0;            // +0x10: its low 28 bits (a line)
    const char* file = nullptr;  // +0x14
    const Allocation* next = nullptr;  // +0x18
};
struct Tracker {
    const Allocation* first = nullptr;  // +0x08
    const char* name = nullptr;         // +0x24
};
// The registry: 8 slots, all empty.
std::array<const Tracker*, 8>& trackers();
// 0x005A4170: the slots not empty.
int trackerCount();

// The memory page ([0x0075F7B0], 0x34 bytes, vtable 0x006FD990, made by 0x0059D970 / 0x0059D870 at the game's start,
// 0x0040C6D4): its page (0x0059DA00 → 0x0059DA10) waits a frame ("Please Wait..."), then gathers the trackers, then
// draws them as a tree.
class MemoryPage {
public:
    // A group of a tracker's allocations (0x14 bytes): the name, the bytes.
    struct Group {
        std::wstring name;
        uint32_t size = 0;  // +0x10
    };
    // A tracker's entry (0x24 bytes): its name, its bytes, its largest groups.
    struct Entry {
        std::wstring name;
        uint32_t size = 0;          // +0x10
        std::vector<Group> groups;  // +0x14
    };

    MemoryPage(void* platform, const View* view, const Font* font);  // 0x0059D870
    ~MemoryPage();                                                   // 0x0059D910

    // 0x0059DA10: state 0 the screen darkened and "Please Wait...", 1 the trackers gathered, 2 the tree drawn.
    void draw();

private:
    void gather();    // 0x0059DBB0
    void drawTree();  // 0x0059DE80

    uint32_t m_state = 0;            // +0x08
    void* m_platform = nullptr;      // +0x0C
    const View* m_view = nullptr;    // +0x10
    const Font* m_font = nullptr;    // +0x14
    Page* m_page = nullptr;          // +0x18
    Pages* m_pages = nullptr;        // +0x1C
    std::vector<Entry> m_entries;    // +0x20
    int32_t m_selected = 0;          // +0x30
};

// 0x005A41E0: an entry of a tracker: its name, the bytes of its allocations; no groups.
void fillEntry(MemoryPage::Entry& entry, const Tracker& tracker);
// 0x005A4250: the tracker's allocations grouped by file and tag, the largest `most` groups (all with 0) by their
// file's name from its last \ or /, largest first.
void fillGroups(const Tracker& tracker, uint32_t most, MemoryPage::Entry& entry);

// [0x0075F7B0]: the game's memory page, made at the game's start; none before.
MemoryPage* memoryPage();

// The game's start (0x0040C68D .. 0x0040C6D4): the pages' manager ([game + 0x78]), the profiler and the memory page,
// with the game's 2D view, its font (0) and the platform ([game + 4]). The stock game never frees them.
void makeDebugObjects(render::IRenderDevice& device, void* platform);

// 0x0040D9F0, as a race's load ends (0x0046348E): the pool's statistics to the debugger's output.
void poolStats();

// A timer of the platform (0x005A23E0 makes one, 0x005A2250, vtable 0x006FDB80): the race's load times its parts with
// two (0x00461390).
class Stopwatch {
public:
    Stopwatch();
    // +0x04 (0x005A22A0): the seconds since it was last asked or started, the counter kept.
    float lap();
    // +0x08 (0x005A23B0): started again.
    void restart();

private:
    float m_scale = 1.0f;      // +0x08
    float m_frequency = 0.0f;  // +0x0C
    uint64_t m_last = 0;       // +0x10
};

}  // namespace oj::debug
