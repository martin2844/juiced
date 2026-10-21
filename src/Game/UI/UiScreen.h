#pragma once
// One window of the interface on screen (docs/spec/04-interface.md): its .ui file, its texts in the game's language,
// its textures and fonts, and the state of its elements (transitions, buttons, menus).

#include "Core/Data/Language.h"
#include "Core/Formats/D8f.h"
#include "Game/UI/Components/ColourChart.h"
#include "Game/UI/Components/Dial.h"
#include "Game/UI/Components/Menu.h"
#include "Game/UI/Components/Meter.h"
#include "Game/UI/Components/Movie.h"
#include "Game/UI/Components/Slider.h"
#include "Game/UI/Components/Text.h"
#include "Game/UI/Components/Ticker.h"
#include "Game/UI/Components/Transition.h"
#include "Game/UI/Components/View3d.h"
#include "Game/UI/UiFile.h"
#include "Render/RenderDevice.h"

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace oj::movie {
class Player;
}

namespace oj::ui {

// The resolution class of the interface: 640, or 800 / 1024 / 1280 / 1600 once the back buffer reaches 800 x 600,
// 1024 x 768, 1280 x 960, 1600 x 1200. It picks the font files; glyphs are drawn at their own pixel size.
uint32_t resolutionClass(uint32_t width, uint32_t height);

// A picture of the interface's textures (0x00517EC0 and the like) with the material TexShape2D (0x0052CF20): stage 0
// clamped, then the texture's own sampler states (its .ddt header: most of the interface's are point sampled) unless
// the stage has them already ([0x0075B298]); a texture the interface did not read is drawn with linear filters and
// wrapping, none with the colours only.
void drawPicture(render::IRenderDevice& device, const render::QuadVertex (&corners)[4], render::TextureId texture,
                 render::Blend blend);

class Screen : public std::enable_shared_from_this<Screen> {
public:
    // The interface manager's sound (0x005A8860) for the window's own: SELECT as a button's or a menu's highlight
    // moves (0x005A9F70, 0x005B11A0). None: silent.
    std::function<void(const char*)> sound;
    // The sound a window takes as it is made: the race front end's manager's while a race runs (its windows are made
    // by the race's code, not by the front end).
    static inline std::function<void(const char*)> defaultSound;

    Screen(render::IRenderDevice& device, const Language& language);
    ~Screen();
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

    // The views of the 3d elements of the windows loaded after this.
    void setView3dFactory(View3dFactory factory) { m_viewFactory = std::move(factory); }
    // ui	extures\<name>.ddt (else <name>_<language letter>.ddt), loaded once by this window.
    render::TextureId texture(const std::string& name);
    // The same loaded anew, the caller's to keep (none kept by the window).
    render::TextureId loadTexture(const std::string& name);

    // ui\windows\<name>.ui and ui\windows\<language>\<name>.<ext>, relative to the current directory. fromDisk: the
    // window manager's +0x20 (0x005A9490's last argument, 0 in every request of the game): the window's file from the
    // disk's folders and its texts by the formats "%s<language>\%s.<ext>" (the French ".fra").
    bool load(const std::string& name, bool fromDisk = false);
    // The last load read its file.
    bool loaded() const { return m_loaded; }
    // The window manager's switch waiting a frame (+0xBC 2) as the window is drawn.
    void setSwitchWaiting(bool waiting) { m_switchWaiting = waiting; }
    const std::string& name() const { return m_name; }

    // Draw a label for runtime overlays using the game's font atlas, in back-buffer pixels.
    void drawOverlayText(const std::wstring& text, float x, float y, float height,
                         uint32_t colour, float maxWidth);

    // The text of an element whose text the code sets (instead of a Caption line).
    void setText(const std::string& element, const std::wstring& text);
    // The text an element shows: the text set, else its Caption line.
    std::wstring textOf(const std::string& element) const;
    // 0x005A8F50: a text for an element's width (in pixels, to the nearest) in its font: as it is when it fits, else
    // characters taken off its end until it fits with "..." after it.
    std::wstring fitText(const std::string& element, const std::wstring& text);
    // 0x005A8F50 with the width given (pixels, compared unsigned as the stock code does).
    std::wstring fitTextWithin(const std::string& element, const std::wstring& text, int most);
    // 0x005A9060: a text for a width (pixels) in an element's font: as it is when it fits, else "..." and its end,
    // its first characters taken off one by one until it fits (empty for a text of one character that does not).
    std::wstring fitTextEnd(const std::string& element, const std::wstring& text, int most);
    // 0x005B3260: an element's width in pixels (its width x the screen's x the screen's scale 1, cut toward zero);
    // -1 without the element.
    int pixelWidth(const std::string& element) const;
    // The same to the nearest (the code's own conversion on a CPU with SSE2, CVTSS2SI: 0x004791B0).
    int pixelWidthNearest(const std::string& element) const;
    // 0x005B52C0: a ticker's text measured, its move started again (see drawTicker).
    void measureTicker(int element);
    // 0x005A5F20's end: every ticker of the window (type 8) measured and started again (0x005B52C0).
    void restartTickers();
    // A menu element (type 6) of that name.
    bool isMenu(const std::string& element) const;
    // 0x005AF860: the menu's help ticker takes the highlighted item's help again (an item there).
    void refreshMenuHelp(const std::string& menu);

    // A text broken into lines at line feeds and at spaces to fit its box (the stock code sets it on some texts).
    void setMultiline(const std::string& element, bool on);
    // A multiline text drawn inside its box only (+0x13C) and scrolling up (+0x140): its first line starts a line
    // height below the box and rises 30 pixels a second (+0x148) of the front end's frame time; once the box's height
    // and the lines' are passed it starts again (0x005B1CB0). The scroll (+0x144) back to 0.
    void setScrolling(const std::string& element, bool clip, bool scroll);
    // Elements whose name holds the part (case counts, as strstr) are hidden; animate: an element with a transition
    // goes out instead and is hidden when that ends (0x005A9AD0).
    void hideContaining(const std::string& part, bool animate = false);
    void setHidden(const std::string& element, bool hidden);
    bool hidden(const std::string& element) const;
    // 0x005A9BB0: a hidden element of that name (case aside) is shown; animate: its transition starts again.
    void show(const std::string& element, bool animate);
    // The element's transition written by the code (its +0x34 record): in, shown and its transition from the start
    // (remaining its duration, the delay again, progress and alpha 0); out, unless going out already, going out with
    // the delay again, progress 0 and nothing remaining. Without a transition out hides it.
    void restartTransition(const std::string& element, bool out);
    // 0x005A9C20: hidden elements whose name holds the part are shown; animate: their transitions start again, else
    // they show as at the end of them.
    void showContaining(const std::string& part, bool animate);
    // The picture of an element in place of its Texture line (0x005B3130).
    void setTexture(const std::string& element, const std::string& texture);
    // An element's transition started again and the element shown (an element's +0x34: time left its duration, the
    // delay left the delay, progress and alpha 0, coming in; +0x58 0); nothing without a transition.
    void restartTransition(const std::string& element);
    // 0x005AA0A0: the transition of each element that has one started again, coming in (as above), and it shown.
    void restartTransitions();
    // 0x0046C970 on an element's transition: its delay (+0x14), below 0 none, past 9999 10000.
    void setTransitionDelay(const std::string& element, float delay);
    // A Meter's value (+0xF0); the shown value (+0xEC) moves 0.15 of the way to it at each draw, or jumps there.
    void setMeter(const std::string& element, float value, bool jump = false);
    // A Meter's shown value (+0xEC); 0 without the element.
    float meterShown(const std::string& element) const;
    void setMeterShown(const std::string& element, float value);  // +0xEC
    // A Meter the player moves (+0xFC) and whether it takes the held direction now (+0xF8).
    void setMeterSlider(const std::string& element, bool slider);
    // A Slider's value (+0xC0, 0 .. 1) and whether it takes the direction held (+0xD4).
    float sliderValue(const std::string& element) const;
    void setSliderValue(const std::string& element, float value);
    void setSliderActive(const std::string& element, bool active);
    // A ColourChart: the colour of the cell under its cursor (+0xD8); a colour whose cell the next draw puts the
    // cursor on (+0xDC, +0xD4); the cursor's column and row (+0xC0, +0xC4).
    uint32_t chartColour(const std::string& element) const;
    void chartFind(const std::string& element, uint32_t argb);
    bool chartFinding(const std::string& element) const;
    int chartColumn(const std::string& element) const;
    int chartRow(const std::string& element) const;
    // A menu item's colour (+0x50).
    void setMenuItemColour(const std::string& menu, size_t index, uint32_t argb);
    void setMeterActive(const std::string& element, bool active);

    // Buttons: the highlighted button starts at the one marked Highlight; UpButton, DownButton, LeftButton and
    // RightButton name a button's neighbours. Names are matched without regard to case.
    std::vector<std::string> buttons() const;
    const std::string& currentButton() const { return m_current; }
    // 0x005A99F0: every button's highlight off, then the button's on (0x005B2930 each: its text modifier started
    // again, its alpha 1); the highlighted button draws in its HighFont (+0x120).
    void setCurrentButton(const std::string& name);
    // One step of the highlight (input::kUp .. kRight); false when the button names no visible neighbour that way.
    bool moveHighlight(uint32_t direction);
    // A button's neighbour (input::kUp .. kRight) in place of its UpButton .. RightButton line (+0x154 up, +0x174
    // down, 32 characters each; 0x005A99F0 makes the links again).
    void setNeighbour(const std::string& button, uint32_t direction, const std::string& name);
    // Colours the code gives an element in place of its Font, HighFont and Colour.
    void setFontColours(const std::string& element, uint32_t font, uint32_t high);
    // The font of a text in place of its Font line (0x005B1A20); the colour stays.
    void setFont(const std::string& element, const std::string& font);
    // The Font name of an element; empty without one.
    std::string fontOf(const std::string& element) const;
    void setBoxColour(const std::string& element, uint32_t argb);

    // Menus (Begin Menu): the items of the .ui file (AddItem, AddTexturedItem) and those the code adds. A menu keeps
    // the items it shows (+0x11C) and every item added at the end (+0x12C): an item put at a place is only shown, one
    // taken out only leaves the shown ones.
    // 0x005AEEF0: both lists emptied, the highlight, the widest item and the first item shown 0 (the items' coming
    // in is left as it is).
    void clearMenu(const std::string& menu);
    // 0x005AECF0: the shown items back to every item added at the end (those put at a place freed; the code's changes
    // to the others stay); the items come in again.
    void restoreMenu(const std::string& menu);
    void addMenuItem(const std::string& menu, const std::wstring& text, float value = 0.0f,
                     const std::string& texture = {}, const std::string& highTexture = {});
    // An item with a name (+0x28) the code looks for (0x005AF230).
    void addCommandItem(const std::string& menu, const std::wstring& text, float value, const std::string& command);
    // The same at a place of the list (0x005AF230's last argument): -1 at the end (of both lists), another place
    // among the shown items only (0x005B1420; past the end: last).
    void insertCommandItem(const std::string& menu, int place, const std::wstring& text, float value,
                           const std::string& command);
    // 0x005AFAD0: the first item of that name (case aside) is taken out; false when there is none.
    bool removeMenuItem(const std::string& menu, const std::string& command);
    // 0x005AFAA0: the item at that place taken out.
    void removeMenuItemAt(const std::string& menu, size_t index);
    // The item's +0x54: its picture drawn adding (ADDITIVE of AddTexturedItem).
    void setMenuItemAdditive(const std::string& menu, size_t index, bool additive);
    // 0x005AFA10 and the item's +0x00: the first item of the name enabled or not (a disabled item is passed over).
    void setMenuItemEnabled(const std::string& menu, const std::string& command, bool enabled);
    // The item's +0x00 by its place.
    void setMenuItemEnabledAt(const std::string& menu, size_t index, bool enabled);
    bool menuItemEnabled(const std::string& menu, size_t index) const;
    // 0x005AF420: the item's text.
    void setMenuItemText(const std::string& menu, size_t index, const std::wstring& text);
    std::wstring menuItemText(const std::string& menu, size_t index) const;
    // The item's value, pictures (+0x48, +0x4C) and name (+0x28).
    void setMenuItemValue(const std::string& menu, size_t index, float value);
    void setMenuItemTextures(const std::string& menu, size_t index, const std::string& texture,
                             const std::string& highTexture);
    void setMenuItemCommand(const std::string& menu, size_t index, const std::string& command);
    // 0x005AF3C0: the item's help (+0x14; the help ticker's text when it is highlighted).
    void setMenuItemHelp(const std::string& menu, size_t index, const std::wstring& help);
    size_t menuSize(const std::string& menu) const;
    // keepPosition: the highlight glides to the item instead of jumping (0x005AF8A0).
    void selectMenuItem(const std::string& menu, size_t index, bool keepPosition = false);
    // The highlight's place (+0x148): the item it is at, or between two while it glides; 0 without the menu.
    float menuPosition(const std::string& menu) const;
    // The menu's +0x178 (1 when made): a menu at 0 takes no window events (0x005B11A0).
    void setMenuActive(const std::string& menu, bool active);
    // The code's settings of a menu: its alignment (+0x158: 0 centre, 1 left, 2 right; the Align property's until
    // set), its items cut to its box whatever its ClipMenu (+0x16C), its text modifier freed (+0x10C: the highlighted
    // item drawn plain).
    void setMenuAlign(const std::string& menu, int align);
    void setMenuClipped(const std::string& menu, bool clipped);
    void removeMenuModifier(const std::string& menu);
    size_t selectedMenuItem(const std::string& menu) const;
    float menuItemValue(const std::string& menu, size_t index) const;
    std::string menuItemCommand(const std::string& menu, size_t index) const;
    // The engine's 2D text (0x005186C0) as the race front end draws it on its own: a text in a font of ui\fonts (by
    // the screen's resolution) at a place in pixels, its line's top left; and its width (0x0051E690).
    void drawFreeText(const std::string& fontName, const std::wstring& text, float x, float y, uint32_t colour);
    // A picture of the windows' textures on a quad of one colour (0x00517EC0, the network overlay's voice icons).
    void drawFreePicture(const std::string& textureName, float x0, float y0, float x1, float y1, uint32_t colour);
    float freeTextWidth(const std::string& fontName, const std::wstring& text);

    // The window event for the menus (0x005B11A0): a vertical menu takes UP and DOWN, a horizontal one LEFT and
    // RIGHT; true when the selection moved. A menu taking the second player's (+0x1A0) takes secondEvent when event
    // is none.
    bool menuEvent(uint32_t event, uint32_t secondEvent = 0);
    // The menu's +0x1A0 (MenuState::secondPlayer).
    void setMenuSecondPlayer(const std::string& menu, bool on);

    // Transitions jump to their end, as when an event comes while the window still moves in (0x005AA160).
    void finishTransitions();
    // 0x005AA120: every transition of the window has reached its end.
    bool transitionsDone() const;
    // 0x005AA0E0: every element with a transition that is not going out goes out.
    void transitionsOut();

    // Elements by their place in the window's list (the order they are drawn in and searched in): the code that keeps
    // elements (the race's HUD) finds them once and changes them each frame.
    // The first element of a name (case aside, the window's list in order; -1 when none).
    int find(const std::string& name) const;
    int size() const { return static_cast<int>(m_window.elements.size()); }
    uint32_t width() const { return m_device.width(); }
    const std::string& nameOf(int element) const;
    // 0x005B19C0, 0x005B1980: the text (0x005B27B0), then the element's text modifier (+0x108, made by the text's
    // constructor, or by a FontModifier line 0x005B1B20) started again (0x005B14A0's: +0x08 = +0x04, +0x18 1, +0x1C and
    // +0x20 0, +0x24 1.5); the transition (+0x34) is left as it is.
    void setText(int element, const std::wstring& text);
    // 0x0042E480 on an element's text (+0x10C, its Caption until the code sets one): more written after it, the
    // transition left as it is.
    void appendText(int element, const std::wstring& more);
    void clearText(int element);                           // 0x005B1A00
    void setHidden(int element, bool hidden);              // +0x58
    bool hidden(int element) const;
    // 0x005B2930: a text's highlight (+0x120); its alpha back to 1.
    void setHighlighted(int element, bool on);
    // 0x00447530: a text's alpha (+0x130), held within 0 .. 1; the text is drawn at it, not at its colour's.
    void setTextAlpha(int element, float alpha);
    // +0x11C and +0x150: a text's colour in place of its Font line's.
    void setFontColour(int element, uint32_t argb);
    // 0x00434D20: the four corner colours, each corner's alpha also its most.
    void setColour(int element, uint32_t argb);
    // 0x0042E4F0: each corner's alpha byte, the value held within 0 .. 1 (the most stays).
    void setAlpha(int element, float alpha);
    // The element's place and size (+0xA4, +0xA8, +0x94, +0x98; fractions of the screen) in place of its .ui line's.
    void setPlace(int element, float x, float y, float w, float h);
    void placeOf(int element, float& x, float& y, float& w, float& h) const;
    void setTexture(int element, const std::string& texture);  // 0x005B3130
    // +0x5C .. +0x68: the texture coordinates in place of the UVs line's.
    void setUVs(int element, float u0, float u1, float v0, float v1);
    void setMeter(int element, float value);                    // +0xF0
    // Dials: the angle the needle turns to (+0xCC; jump: the shown one, +0xC8, too), the range of NeedleData (+0xC0,
    // +0xC4) and the needle's colour (+0xD4).
    void setDial(int element, float angle, bool jump = false);
    float dialMin(int element) const;
    float dialMax(int element) const;
    void setNeedleColour(int element, uint32_t argb);
    // 0x005A9CA0: the element moves to the end of the window's list.
    void moveToEnd(int element);
    // 0x005B1BF0: the fonts of the element (Font and HighFont) step every character but the space by `advance`
    // pixels (the font object's +0x10, +0x14; every text of those fonts in the window).
    void setFixedAdvance(int element, int advance);
    // 0x005B2A80 and the window's list: a Box of no properties at the end of the list, its colour 0.
    int addBox(const std::string& name, float x, float y, float w, float h);

    // dt: the frame time; scroll: the direction held (input::kUp .. kRight, sliding Meters).
    void update(float dt, uint32_t scroll = 0);
    void draw(float seconds, float dt = 0.0f);
    // A window under the message window (0x005A9D20): its elements are drawn (and step, 0x005B2BE0) only when their
    // name holds "FRAME_" (kFramesOnly, with [0x0071E710] set), or not at all (kNone); kAll otherwise.
    enum class Under { kAll, kFramesOnly, kNone };
    void setUnderMessage(Under under) { m_under = under; }
    // The same for every screen drawn while it is set (the race's front end, whose windows OpenJuice draws one by one).
    static void setDrawingUnderMessage(Under under) { s_under = under; }
    // The view of a 3d element, nullptr when there is none.
    View3d* view(const std::string& element);

    // Movie elements (0x005AA290, docs/spec/04-interface.md "Movie"). 0x005AA300: the interface's movie given back,
    // the element's movie (+0xC0) the name, looped (+0x100) as asked and its volume (+0x114) 1; it opens at the
    // element's next draw.
    void playMovie(int element, const std::string& name, bool looped);
    // The volume the element's movie plays at (+0x114).
    void setMovieVolume(int element, float volume);
    // The interface's movie ([0x0075F838]) when it is the element's (its name the one playing, case aside); nullptr
    // otherwise.
    movie::Player* elementMovie(int element) const;
    // 0x00435030 on the element: its movie held and stepped twice (0x00559A60 inlined), or the hold taken off.
    void holdMovie(int element, bool hold);
    // The element's movie (+0xC0); empty for none.
    const std::string& movieName(int element) const;
    // The message windows of the front end the movies are muted under (the window manager's +0x34 msgbox_critical,
    // +0x3C msgbox_network, +0x44 msgbox and +0x108 the number of its message).
    struct MovieMessages {
        bool critical = false;
        bool network = false;
        bool message = false;
        int number = 0;
    };
    static void setMovieMessages(const MovieMessages& m) { s_movieMessages = m; }

private:
    // An element's own Caption line (the text each element takes at the window's load into its +0x10C): elements
    // of one name each keep theirs, as cmscarcollection2's second TITLE ("CAR COLLECTION").
    std::wstring caption(const Element& e) const;
    struct Font {
        D8f data;
        std::vector<render::TextureId> pages;
        // Each page's sampler states (a texture the font makes: linear, wrapping), set as the font material sets them
        // (0x0052B230: the texture's own, not clamped).
        std::vector<render::SamplerParams> samplers;
        int ascent = 0;   // above the baseline
        int descent = 0;  // below it
        bool fixed = false;    // +0x10
        int fixedAdvance = 0;  // +0x14
        int height() const { return ascent + descent; }
    };
    struct State {
        bool hidden = false;
        std::array<std::string, 5> neighbours;  // set by the code, by input::kUp .. kRight
        std::array<uint32_t, 4> corners{};  // +0x10 .. +0x1C: Colour, VertexColour, or the code's
        float switchAlpha = 0.0f;           // +0x134: highlighted while the manager's switch waits
        bool highlighted = false;           // +0x120
        bool placed = false;                // the code moved the element
        bool hasUVs = false;                // the code set the texture coordinates
        float u0 = 0.0f, u1 = 1.0f, v0 = 0.0f, v1 = 1.0f;
        float x = 0.0f, y = 0.0f;           // +0xA4, +0xA8
        float w = 0.0f, h = 0.0f;           // +0x94, +0x98
        bool hasTransition = false;
        Transition transition;
        // The menu's text modifier (+0x10C, 0x2C bytes, made by the menu's constructor 0x005AE8D0): stepped with the
        // highlighted item (0x005B14C0), started again when the highlight moves (0x005B14A0). A text's or a button's
        // (+0x108, made by its constructor alike, 0x005B17E1): stepped and drawn with while it is highlighted in a
        // HighFont, started again by a text set. FontModifier <ONCE|LOOP|BOUNCE> <start> <growth> <speed> <reach>
        // (0x005AC8C0; no window has one) makes either anew (0x005B1B20, 0x005AF0B0).
        TextModifier modifier;
        // A picture set by the code
        bool hasTexture = false;
        std::string texture;
        // The parts of the element's kind.
        TextState text;
        TickerState ticker;
        MenuState menu;
        DialState dial;
        MeterState meter;
        SliderState slider;
        ColourChartState chart;
        MovieState movie;
    };

    const Element* element(const std::string& name) const;
    int indexOf(const std::string& name) const;
    Font* font(const std::string& name);
    void buildMenu(size_t index);
    void widenMenu(size_t index, const MenuItem& item, bool textured);
    float textWidth(const Font& f, const std::wstring& text) const;
    void elementRect(const Element& e, const State& s, float& x0, float& y0, float& x1, float& y1) const;
    void drawBox(const Element& e, const State& s, float seconds);
    void drawText(const Element& e, State& s, bool highlighted);
    Under m_under = Under::kAll;
    static inline Under s_under = Under::kAll;
    bool drawn(const Element& e) const {
        const Under under = m_under != Under::kAll ? m_under : s_under;
        return under == Under::kAll || (under == Under::kFramesOnly && e.name.find("FRAME_") != std::string::npos);
    }
    bool m_loaded = false;
    bool m_switchWaiting = false;  // the manager's switch waiting (its +0xBC 2)
    float m_drawTime = 0.0f;  // the frame time of the draw (the front end's +0x104)
    float m_drawSeconds = 0.0f;  // the time of the draw
    void drawDial(const Element& e, State& s);
    void drawMenu(const Element& e, State& s);
    void drawMeter(const Element& e, State& s);
    void slideMeter(const Element& e, State& s, float dt, uint32_t scroll);
    void slide(State& s, float dt, uint32_t scroll);
    void drawSlider(const Element& e, State& s);
    void moveChart(State& s, float dt, uint32_t scroll);
    void drawChart(const Element& e, State& s);
    void openMovie(size_t index, State& s);  // 0x005AA3A0
    void drawMovie(size_t index, State& s);  // 0x005AA4C0, after the box
    static MovieMessages s_movieMessages;
    static void showState(State& s, bool animate);
    void drawString(Font* f, uint32_t colour, const std::wstring& text, float x, float baseline,
                    const TextModifier* modifier = nullptr);
    void drawMenuArrows(const Element& e, State& s, uint32_t colour);  // 0x005B0D87 .. 0x005B1172
    void drawLineElement(const Element& e, State& s);                  // 0x005B3340
    void drawTicker(const Element& e, State& s);
    void menuHelp(size_t menu);  // 0x005AF860
    void quad(float x0, float y0, float x1, float y1, const uint32_t (&colours)[4], render::TextureId texture,
              float u0, float u1, float v0, float v1, bool additive);

    render::IRenderDevice& m_device;
    Language m_language;
    std::string m_name;
    Window m_window;
    TextTable m_texts;
    std::vector<State> m_states;  // per element of m_window
    std::vector<size_t> m_order;  // the window's list: the elements in the order they are drawn and searched
    std::string m_current;        // the highlighted button
    std::map<std::string, render::TextureId> m_textures;
    std::map<std::string, std::unique_ptr<Font>> m_fonts;
    View3dFactory m_viewFactory;
    std::vector<std::unique_ptr<View3d>> m_views;  // per element of m_window
};

}  // namespace oj::ui
