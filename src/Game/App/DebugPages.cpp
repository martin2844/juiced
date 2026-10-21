#include "Game/App/DebugPages.h"

#include "Core/Base/Log.h"
#include "Render/RenderDevice.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace oj::debug {

namespace {

// The platform's timer ([game + 4] vt+4, vtable 0x006FDB80): +0x10 the counter (0x005A2370, of which the profiler
// keeps the low 32 bits), +0x14 its frequency (0x005A2390).
uint32_t ticks() {
    LARGE_INTEGER c{};
    QueryPerformanceCounter(&c);
    return static_cast<uint32_t>(c.QuadPart);
}
uint64_t frequency() {
    LARGE_INTEGER f{};
    QueryPerformanceFrequency(&f);
    return static_cast<uint64_t>(f.QuadPart);
}

// 0x00409370: a text widened character by character (sign extended).
std::wstring widen(const char* text) {
    std::wstring out;
    if (text == nullptr) return out;
    for (const char* p = text; *p != '\0'; ++p) {
        out.push_back(static_cast<wchar_t>(static_cast<uint16_t>(static_cast<int16_t>(static_cast<signed char>(*p)))));
    }
    return out;
}

// An unsigned 32-bit value as the stock code loads it (fild, plus 2^32 below 0).
float unsignedFloat(uint32_t v) { return static_cast<float>(static_cast<double>(v)); }

std::unique_ptr<View> g_view;
Pages* g_pages = nullptr;                   // [0x0075F7A0]
std::unique_ptr<Profiler> g_profiler;       // [0x0075F7A4]; [0x0075F7A8], the second column's, is never made
std::unique_ptr<MemoryPage> g_memoryPage;   // [0x0075F7B0]
std::array<const Tracker*, 8> g_trackers{};  // [0x0075F7EC] .. [0x0075F808]
// [0x0075F7DC]: set while the memory page reads the trackers; read by nothing.
uint32_t g_trackersBusy = 0;

// [0x007116D0]: the bytes of the game's pool (0x02000000, made by the main function 0x00418361); [0x0075F7B8] and
// [0x0075F7BC] the most of it used since the last load and ever (0x005A10AB). OpenJuice allocates from the system and
// has no pool: both 0.
constexpr uint32_t kPoolSize = 0x02000000;
uint32_t g_poolMax = 0;
uint32_t g_poolEverMax = 0;

}  // namespace

// --- View ---

void View::rect(float& x0, float& y0, float& x1, float& y1) const {
    x0 = 0.0f;
    y0 = 0.0f;
    x1 = device != nullptr ? unsignedFloat(device->width()) + x0 : x0;
    y1 = device != nullptr ? unsignedFloat(device->height()) + y0 : y0;
}

void View::line(const float a[2], const float b[2], uint32_t argb) const {
    if (device != nullptr) device->drawLine(a[0], a[1], b[0], b[1], argb);
}

void View::fill(float a[2], const float b[2], uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3) const {
    if (device == nullptr) return;
    auto snap = [](float v, float edge) {
        if (-2.0f < v && v < 2.0f) return -0.5f;
        if (edge - 2.0f < v && v < edge + 2.0f) return edge;
        return v;
    };
    a[0] = snap(a[0], unsignedFloat(device->width()));
    a[1] = snap(a[1], unsignedFloat(device->height()));
    // A strip of four: a, across, down, b.
    const render::QuadVertex corners[4] = {
        {a[0], a[1], 0.0f, 0.0f, c0},
        {b[0], a[1], 0.0f, 0.0f, c1},
        {a[0], b[1], 0.0f, 0.0f, c2},
        {b[0], b[1], 0.0f, 0.0f, c3},
    };
    device->drawQuad(corners, 0, render::Blend::Alpha);
}

// --- Pages ---

Pages::Pages(const View* view, const Font* font, void* platform)
    : m_current(m_pages.end()), m_view(view), m_font(font), m_platform(platform) {}

Pages::~Pages() {
    for (Page* p : m_pages) delete p;
}

Pages* Pages::make(const View* view, const Font* font, void* platform) {
    if (g_pages != nullptr) {
        ++g_pages->m_count;
        return g_pages;
    }
    g_pages = new Pages(view, font, platform);
    return g_pages;
}

void Pages::release() {
    if (g_pages != nullptr && --g_pages->m_count == 0) {
        delete g_pages;
        g_pages = nullptr;
    }
}

Page* Pages::add(Page::Draw draw, void* data, const View* view, const Font* font, void* platform) {
    Page* p = new Page;
    p->draw = draw;
    p->data = data;
    p->view = view != nullptr ? view : m_view;
    p->font = font != nullptr ? font : m_font;
    p->platform = platform != nullptr ? platform : m_platform;
    p->unused = 0;
    // The node goes before the list's first (std::list's head +0x00 is its first node).
    m_pages.push_front(p);
    return p;
}

bool Pages::setVisible(bool visible) {
    const bool old = m_visible;
    m_visible = visible;
    if (visible && !m_pages.empty() && m_current == m_pages.end()) m_current = m_pages.begin();
    return old;
}

void Pages::next(bool force) {
    if (!m_visible && !force) return;
    ++m_current;
    if (m_current == m_pages.end()) m_current = m_pages.begin();
}

void Pages::previous() {
    if (!m_visible) return;
    if (m_current == m_pages.begin()) m_current = m_pages.end();
    --m_current;
}

int Pages::remove(Page* page) {
    for (auto it = m_pages.begin(); it != m_pages.end(); ++it) {
        if (*it == page && m_current == it) {
            next(true);
            break;
        }
    }
    // Stock problem: a list of the one page removed leaves the current node (+0x20) on the node freed.
    const bool currentGoes = m_current != m_pages.end() && *m_current == page;
    m_pages.remove(page);
    if (currentGoes) m_current = m_pages.end();
    delete page;
    return 0;
}

void Pages::line(const char* text, const float place[2], const uint32_t* argb, const Font* font) const {
    if (font == nullptr) font = m_font;
    const std::wstring wide = widen(text);
    // Stock problem: the font is the game's +0x74, never set: the call goes through address 0. Not drawn here.
    if (font != nullptr) font->draw(place, wide.empty() ? nullptr : wide.c_str(), *argb, 0);
}

void Pages::drawCurrent() const {
    if (!m_visible || m_current == m_pages.end()) return;
    const Page* p = *m_current;
    p->draw(p->data, p->view, p->font, p->platform);
}

Pages* pages() { return g_pages; }

// --- Profiler ---

// The profiler's page object ([0x0075F7AC], 0x10 bytes, vtable 0x006FD95C, 0x0059CF60): one for both columns, its
// page drawing [0x0075F7A4] and [0x0075F7A8] (0x0059D080).
struct ProfilerPage {
    uint32_t count = 1;       // +0x04
    Page* page = nullptr;     // +0x08
    Pages* pages = nullptr;   // +0x0C
};

namespace {

ProfilerPage* g_profilerPage = nullptr;  // [0x0075F7AC]

void drawProfilers(void* /*data*/, const View* /*view*/, const Font* /*font*/, void* /*platform*/) {
    if (g_profiler) g_profiler->draw();
}

ProfilerPage* makeProfilerPage(const View* view, const Font* font, void* platform) {
    if (g_profilerPage != nullptr) {
        ++g_profilerPage->count;
        return g_profilerPage;
    }
    auto* p = new ProfilerPage;
    p->pages = Pages::make(view, font, platform);
    p->page = p->pages->add(&drawProfilers, p, view, font, platform);
    g_profilerPage = p;
    return p;
}

// 0x0059D030: the manager let go; its page stays in the manager's list.
void releaseProfilerPage(ProfilerPage* p) {
    if (p == nullptr || --p->count != 0) return;
    Pages::release();
    if (g_profilerPage == p) g_profilerPage = nullptr;
    delete p;
}

}  // namespace

Profiler::Profiler(void* platform, const View* view, const Font* font, uint32_t column)
    : m_platform(platform), m_view(view), m_font(font), m_column(column) {
    for (Timer& t : m_timers) t = Timer{};
    m_current = 0;
    m_count = 1;
    m_timers[0].name = column == 0 ? "CPU Frame" : "VU1 Frame";
    m_page = makeProfilerPage(view, font, platform);
}

Profiler::~Profiler() { releaseProfilerPage(m_page); }

int Profiler::add(const char* name) {
    if (m_count >= kTimers) return 0;
    m_timers[static_cast<size_t>(m_count)].name = name;
    return m_count++;
}

void Profiler::start(int timer) {
    Timer& t = m_timers[static_cast<size_t>(timer)];
    const uint32_t now = ticks();
    ++t.starts;
    t.start = now;
    t.parent = m_current;
    m_current = timer;
}

void Profiler::stop() {
    const int i = m_current;
    Timer& t = m_timers[static_cast<size_t>(i)];
    const uint32_t now = ticks();
    const uint32_t start = t.start;
    t.total += static_cast<uint32_t>(now - start);
    t.end = now;
    if (i != 0) {
        t.start = start - m_timers[0].start;
        t.end = now - m_timers[0].start;
    }
    m_current = t.parent;
}

void Profiler::stopFrame() {
    Timer& t = m_timers[0];
    t.end = ticks();
    t.total += static_cast<uint32_t>(t.end - t.start);
}

void Profiler::draw() {
    const float base = static_cast<float>(static_cast<int32_t>(m_column * 200 + 40));
    m_view->begin();
    // A frame of 60 Hz in the timer's ticks; the page spans three of them across 452 pixels.
    const uint64_t perFrame = frequency() / 60;
    const uint64_t span = perFrame * 3;
    const uint32_t markColour = 0x40141480;
    // The frames' marks: down from the base 508 pixels at 61 and a frame and two frames on.
    {
        const float a[2] = {61.0f, base};
        const float b[2] = {61.0f, base + 508.0f};
        m_view->line(a, b, markColour);
    }
    const uint64_t onePixels = perFrame * 452;
    for (uint64_t frames = 1; frames <= 2; ++frames) {
        const uint64_t at = (onePixels * frames) / span + 0x3D;
        const float x = static_cast<float>(static_cast<double>(at));
        const float a[2] = {x, base};
        const float b[2] = {x, base + 508.0f};
        m_view->line(a, b, markColour);
    }
    // The timers still running ended now, from the last started.
    while (m_current != 0) {
        const int i = m_current;
        Timer& t = m_timers[static_cast<size_t>(i)];
        const uint32_t now = ticks();
        const uint32_t start = t.start;
        t.end = now;
        t.total += static_cast<uint32_t>(now - start);
        t.start = start - m_timers[0].start;
        t.end = now - m_timers[0].start;
        m_current = t.parent;
    }
    m_timers[0].end -= m_timers[0].start;
    m_timers[0].start = 0;
    // Each timer's bar from its start to its end, 8 high, a row of 25 each: within the frame of 60 Hz red (the second
    // column blue), over it dimmer.
    std::array<uint32_t, kTimers> from{};
    std::array<uint32_t, kTimers> to{};
    for (int i = 0; i < m_count; ++i) {
        const Timer& t = m_timers[static_cast<size_t>(i)];
        from[static_cast<size_t>(i)] = static_cast<uint32_t>((static_cast<uint64_t>(t.start) * 452) / span) + 0x3C;
        to[static_cast<size_t>(i)] = static_cast<uint32_t>((static_cast<uint64_t>(t.end) * 452) / span) + 0x3D;
    }
    int32_t row = 0;
    for (int i = 0; i < m_count; ++i, row += 25) {
        const Timer& t = m_timers[static_cast<size_t>(i)];
        float a[2] = {unsignedFloat(from[static_cast<size_t>(i)]), base + static_cast<float>(row)};
        const float b[2] = {unsignedFloat(to[static_cast<size_t>(i)]), base + static_cast<float>(row) + 8.0f};
        if (!t.shown) continue;
        uint32_t colour;
        if (static_cast<uint64_t>(m_timers[0].end) > perFrame) {
            colour = m_column == 0 ? 0x5AFF1414u : 0x5A1414FFu;
        } else {
            colour = m_column == 0 ? 0x32801414u : 0x32141480u;
        }
        m_view->fill(a, b, colour, colour, colour, colour);
    }
    // The labels: the name, the share of a frame of 60 Hz smoothed (0.95 of the last, 0.05 of this one) and its most,
    // black at 29 and green at 30 a pixel higher.
    const float perFrameF = static_cast<float>(static_cast<double>(perFrame));
    row = 0;
    for (int i = 0; i < m_count; ++i, row += 25) {
        Timer& t = m_timers[static_cast<size_t>(i)];
        const float ran = unsignedFloat(t.end - t.start);
        const float share = ran * 100.0f * 0.05f / perFrameF;
        t.average = share + t.average * 0.95f;
        if (t.average > t.maximum) t.maximum = t.average;
        char text[48];
        std::snprintf(text, sizeof text, "  %s - % 3.1f : % 3.1f", t.name.c_str(), static_cast<double>(t.average),
                      static_cast<double>(t.maximum));
        const std::wstring wide = widen(text);
        const float y = static_cast<float>(row) + base - 5.0f;
        const float shadow[2] = {29.0f, y};
        const float front[2] = {30.0f, y - 1.0f};
        // Stock problem: the font (+0x10) is the game's +0x74, never set: the calls go through address 0. Not drawn.
        if (m_font != nullptr) {
            m_font->draw(shadow, wide.c_str(), 0xFF000000u, 0);
            m_font->draw(front, wide.c_str(), 0xFF32FF32u, 0);
        }
    }
    m_view->end();
}

Profiler* profiler() { return g_profiler.get(); }

// --- Trackers ---

std::array<const Tracker*, 8>& trackers() { return g_trackers; }

int trackerCount() {
    int n = 0;
    for (const Tracker* t : g_trackers) {
        if (t != nullptr) ++n;
    }
    return n;
}

void fillEntry(MemoryPage::Entry& entry, const Tracker& tracker) {
    entry.name = widen(tracker.name);
    entry.groups.clear();
    entry.size = 0;
    for (const Allocation* a = tracker.first; a != nullptr; a = a->next) entry.size += a->size;
}

void fillGroups(const Tracker& tracker, uint32_t most, MemoryPage::Entry& entry) {
    // The allocations by file and tag (0x18 bytes each), in the order first met.
    struct Sum {
        std::wstring file;
        uint32_t tag = 0;
        uint32_t size = 0;
    };
    std::vector<Sum> sums;
    for (const Allocation* a = tracker.first; a != nullptr; a = a->next) {
        const std::wstring file = widen(a->file);
        const uint32_t tag = a->tag & 0x0FFFFFFFu;
        auto it = std::find_if(sums.begin(), sums.end(), [&](const Sum& s) { return s.tag == tag && s.file == file; });
        if (it != sums.end()) {
            it->size += a->size;
        } else {
            sums.push_back({file, tag, a->size});
        }
    }
    entry.name = widen(tracker.name);
    entry.groups.clear();
    entry.groups.reserve(most);
    size_t smallest = 0;
    auto findSmallest = [&] {
        uint32_t least = 0xFFFFFFFFu;
        for (size_t k = 0; k < entry.groups.size(); ++k) {
            if (entry.groups[k].size < least) {
                least = entry.groups[k].size;
                smallest = k;
            }
        }
    };
    for (const Sum& s : sums) {
        // The file's name from its last \ or / (that character kept), looked for from its last but one character
        // down to its second. Stock problem: an empty name is taken from the character before it; empty here.
        std::wstring name;
        if (!s.file.empty()) {
            size_t at = s.file.size() - 1;
            while (at > 0 && s.file[at] != L'\\' && s.file[at] != L'/') --at;
            name = s.file.substr(at);
        }
        if (most == 0 || entry.groups.size() < most) {
            entry.groups.push_back({name, s.size});
            if (entry.groups.size() == most) findSmallest();
        } else if (entry.groups[smallest].size < s.size) {
            entry.groups[smallest] = {name, s.size};
            findSmallest();
        }
    }
    // std::sort over reversed iterators by the bytes (0x005A4B90): largest first.
    std::sort(entry.groups.begin(), entry.groups.end(),
              [](const MemoryPage::Group& a, const MemoryPage::Group& b) { return a.size > b.size; });
}

// --- MemoryPage ---

namespace {

void drawMemoryPage(void* data, const View* /*view*/, const Font* /*font*/, void* /*platform*/) {
    static_cast<MemoryPage*>(data)->draw();
}

// A node of the tree the page draws (0x28 bytes, 0x0059E220): its name, its value (bytes), its parent, its children.
struct TreeNode {
    std::wstring name;
    float value = 0.0f;          // +0x10
    TreeNode* parent = nullptr;  // +0x14
    std::vector<std::unique_ptr<TreeNode>> children;  // +0x18

    // 0x0059E2B0: a child with a name and a value.
    TreeNode* add(const std::wstring& childName, float childValue) {
        auto c = std::make_unique<TreeNode>();
        c->name = childName;
        c->value = childValue;
        c->parent = this;
        children.push_back(std::move(c));
        return children.back().get();
    }
};

// The tree's drawer (vtables 0x006FD994 / 0x006FD9A0; 0x0059DE80 makes one on its stack).
struct TreeDrawer {
    TreeNode* root = nullptr;        // +0x04
    void* platform = nullptr;        // +0x08
    const View* view = nullptr;      // +0x0C
    const Font* font = nullptr;      // +0x10
    float x = 0.0f;                  // +0x14: where the bars start
    float scale = 0.0f;              // +0x18: pixels a byte
    uint32_t toggle = 0;             // +0x1C: the bars' colours by turns

    // 0x0059E740: the widest name with its indent (15 a level).
    void widest(const TreeNode& node, uint32_t indent, float& most) const {
        // Stock problem: the font is never set (the call goes through address 0); 0 wide here.
        const uint32_t w = (font != nullptr ? font->width(node.name) : 0) + indent;
        const float f = unsignedFloat(w);
        if (f > most) most = f;
        for (const auto& c : node.children) widest(*c, indent + 15, most);
    }

    // 0x0059E590: a node's line: its name at (indent, y) in the colour of the level, a bar from x as long as its
    // bytes, 0.5 of a line high above y, colours by turns, and its kilobytes ("%.02fk") 6 past the bar's end in yellow.
    // Its children are not drawn.
    void drawNode(uint32_t level, const TreeNode& node, int32_t indent, const int32_t& y) {
        static const uint32_t kNames[4] = {0xC8FF6400u, 0xC864FFD2u, 0xC8FF0000u, 0xC80000C8u};
        static const uint32_t kBars[2] = {0xC800B4C8u, 0xC8007885u};
        const float place[2] = {static_cast<float>(indent), static_cast<float>(y)};
        if (font != nullptr) font->draw(place, node.name.c_str(), kNames[level & 3], 0);
        const float line = unsignedFloat(font != nullptr ? font->height() : 0);
        float a[2] = {x, static_cast<float>(y) - line * 0.5f};
        toggle = (toggle - 1) & 1;
        const float b[2] = {scale * node.value + x, static_cast<float>(y)};
        view->fill(a, b, kBars[toggle], kBars[toggle], kBars[toggle], kBars[toggle]);
        char text[32];
        std::snprintf(text, sizeof text, "%.02fk", static_cast<double>(node.value * 0.0009765625f));
        const std::wstring wide = widen(text);
        const float at[2] = {b[0] + 6.0f, static_cast<float>(y)};
        if (font != nullptr) font->draw(at, wide.empty() ? nullptr : wide.c_str(), 0xFFFFFF00u, 0);
    }

    // vtable 0x006FD994 +0x04 (0x0059E410), called by nothing: the root taken, with `sum` its value summed again.
    void setRoot(TreeNode* node, bool sum) {
        root = node;
        if (sum) root->value = total(*root);
    }
    // 0x0059E7D0: a node with children their values summed (recursively) as its own.
    static float total(TreeNode& node) {
        if (node.children.empty()) return node.value;
        node.value = 0.0f;
        for (auto& c : node.children) node.value = total(*c) + node.value;
        return node.value;
    }
    // vtable 0x006FD9A0 +0x04 (0x0059E360), called by nothing: the root taken, x past the widest name (+ 40) and the
    // scale of the view's width less that and 100.
    void setUp(TreeNode* node) {
        root = node;
        x = 0.0f;
        widest(*root, 0, x);
        x = x + 40.0f;
        float x0, y0, x1, y1;
        view->rect(x0, y0, x1, y1);
        scale = (x1 - x0 - x - 100.0f) / root->value;
    }
    // vtable 0x006FD9A0 +0x08 (0x0059E3D0), called by nothing: the root drawn at (20, 40).
    void drawAll() {
        const int32_t y = 40;
        toggle = 0;
        view->begin();
        drawNode(0, *root, 20, y);
        view->end();
    }
};

}  // namespace

MemoryPage::MemoryPage(void* platform, const View* view, const Font* font)
    : m_platform(platform), m_view(view), m_font(font) {
    m_selected = 0;
    m_state = 0;
    m_pages = Pages::make(view, font, platform);
    m_page = m_pages->add(&drawMemoryPage, this, view, font, platform);
}

// 0x0059D910. Stock problem: its page stays in the manager's list and the manager is not let go.
MemoryPage::~MemoryPage() = default;

void MemoryPage::draw() {
    if (m_state == 0) {
        float x0, y0, x1, y1;
        m_view->rect(x0, y0, x1, y1);
        const int32_t width = static_cast<int32_t>(x1 - x0);
        if (!m_entries.empty()) drawTree();
        m_view->begin();
        float a[2] = {x0, y0};
        const float b[2] = {x1, y1};
        m_view->fill(a, b, 0x3C000000u, 0x3C000000u, 0x3C000000u, 0x3C000000u);
        m_view->end();
        // "Please Wait..." across the middle, 0.9 of a line down, red. Stock problem: the font is never set (the
        // calls go through address 0).
        const std::wstring text = widen("Please Wait...");
        if (m_font != nullptr) {
            const uint32_t across = static_cast<uint32_t>(width) - m_font->width(text);
            const float place[2] = {unsignedFloat(across >> 1), unsignedFloat(m_font->height()) * 0.9f};
            m_font->draw(place, text.empty() ? nullptr : text.c_str(), 0xFFFF0000u, 0);
        }
        m_state = 1;
    } else if (m_state == 1) {
        gather();
        m_state = 2;
    } else if (m_state == 2) {
        drawTree();
    }
}

void MemoryPage::gather() {
    g_trackersBusy = 1;
    const int count = trackerCount();
    if (m_selected < 0 || m_selected >= count) {
        // 0x0059DBCF: nothing to gather (always on the PC: no tracker); the flag stays set.
        return;
    }
    m_entries.resize(static_cast<size_t>(count));
    float x0, y0, x1, y1;
    m_view->rect(x0, y0, x1, y1);
    const uint32_t line = m_font != nullptr ? m_font->height() : 0;
    const int32_t height = static_cast<int32_t>(y1 - y0);
    const int32_t left = height - static_cast<int32_t>((m_entries.size() + 1) * line);
    const int32_t rows = static_cast<int32_t>(
        (unsignedFloat(static_cast<uint32_t>(left)) - unsignedFloat(static_cast<uint32_t>(height)) * 0.02f) - 40.0f);
    // Stock problem: with no font (always) the division is by 0.
    const uint32_t most = line != 0 ? static_cast<uint32_t>(rows) / line : 0;
    // The entries by their names to the trackers' slots.
    std::map<std::wstring, int32_t> slots;
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (static_cast<int>(i) < trackerCount() && g_trackers[i] != nullptr) fillEntry(m_entries[i], *g_trackers[i]);
        slots[m_entries[i].name] = static_cast<int32_t>(i);
    }
    // std::sort over reversed iterators by the bytes (0x0059ED20): largest first.
    std::sort(m_entries.begin(), m_entries.end(), [](const Entry& a, const Entry& b) { return a.size > b.size; });
    Entry& chosen = m_entries[static_cast<size_t>(m_selected)];
    const int32_t slot = slots[chosen.name];
    if (slot >= 0 && slot < trackerCount() && g_trackers[static_cast<size_t>(slot)] != nullptr) {
        fillGroups(*g_trackers[static_cast<size_t>(slot)], most, chosen);
    }
    g_trackersBusy = 0;
}

void MemoryPage::drawTree() {
    g_trackersBusy = 1;
    TreeNode root;
    root.name = widen("Total");
    for (const Entry& e : m_entries) {
        TreeNode* n = root.add(e.name, unsignedFloat(e.size));
        root.value = n->value + root.value;
        for (const Group& g : e.groups) n->add(g.name, unsignedFloat(g.size));
    }
    TreeDrawer drawer;
    drawer.root = &root;
    drawer.platform = m_platform;
    drawer.view = m_view;
    drawer.font = m_font;
    drawer.x = 0.0f;
    drawer.widest(root, 0, drawer.x);
    drawer.x = drawer.x + 40.0f;
    float x0, y0, x1, y1;
    m_view->rect(x0, y0, x1, y1);
    // The view's x and width (+0x14's +0x00, +0x08): (width + x) - x, less the names' room and 100, over the total.
    drawer.scale = (((x1 - x0) + x0 - x0) - drawer.x - 100.0f) / root.value;
    drawer.toggle = 0;
    const int32_t y = 40;
    m_view->begin();
    drawer.drawNode(0, root, 20, y);
    m_view->end();
    g_trackersBusy = 0;
}

MemoryPage* memoryPage() { return g_memoryPage.get(); }

// --- The game's debug objects ---

void makeDebugObjects(render::IRenderDevice& device, void* platform) {
    g_view = std::make_unique<View>();
    g_view->device = &device;
    // The game's +0x74, the pages' font, is 0 (0x0040BB21; nothing sets it).
    const Font* font = nullptr;
    Pages::make(g_view.get(), font, platform);
    g_profiler = std::make_unique<Profiler>(platform, g_view.get(), font, 0);
    g_memoryPage = std::make_unique<MemoryPage>(platform, g_view.get(), font);
}

Stopwatch::Stopwatch() {
    m_scale = 1.0f;
    m_frequency = static_cast<float>(frequency());
    LARGE_INTEGER c{};
    QueryPerformanceCounter(&c);
    m_last = static_cast<uint64_t>(c.QuadPart);
}

float Stopwatch::lap() {
    LARGE_INTEGER c{};
    QueryPerformanceCounter(&c);
    const uint64_t now = static_cast<uint64_t>(c.QuadPart);
    const uint64_t ran = now - m_last;
    m_last = now;
    return static_cast<float>(static_cast<double>(ran) *
                              (static_cast<double>(m_scale) / static_cast<double>(m_frequency)));
}

void Stopwatch::restart() {
    LARGE_INTEGER c{};
    QueryPerformanceCounter(&c);
    m_last = static_cast<uint64_t>(c.QuadPart);
    m_scale = 1.0f;
}

void poolStats() {
    log::debugPrint("++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    log::debugPrint("\nPOOL STATS:\n");
    log::debugPrint("Pool size        =% 7d\n", kPoolSize);
    log::debugPrint("Max pool usage   =% 7d (%d percent)\n", g_poolMax, (g_poolMax * 100) / kPoolSize);
    log::debugPrint("Max ever usage   =% 7d (%d percent)\n", g_poolEverMax, (g_poolEverMax * 100) / kPoolSize);
    log::debugPrint("\n++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
}

}  // namespace oj::debug
