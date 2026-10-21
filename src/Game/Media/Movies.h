#pragma once
// The stock game's movies (docs/spec/04-interface.md, "Movie"; docs/spec/25-movies.md): RAD's rad3d sample compiled
// into the game (0x0055AB80 .. 0x0055B8F0) puts the frames of a Bink movie into textures and draws them as screen
// quads; a player (0x0055A1D0, vtable 0x006FA2F8) opens movies\<name>.bik, steps it and draws it in a rectangle of the
// screen; the interface plays one movie at a time ([0x0075F838], 0x005AA300 .. 0x005AA4C0). The game's binkw32.dll
// decodes (Game/Media/BinkLibrary.h).

#include "Render/RenderDevice.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace oj {
class CrtRandom;
}

namespace oj::bink {
class Library;
}

namespace oj::movie {

class System;

// The player (0x7C bytes; its base 0x00559850, vtable 0x006FA2D4, is a named texture source of the texture
// manager). Made and kept by System::acquire, given back with System::release.
class Player {
public:
    // vtable +0x00 (0x0055A520 → 0x0055A6A0): the current frame drawn at +0x50, +0x54 scaled by +0x40, +0x44.
    void draw();
    // +0x04 (0x0055A3C0): a step of the movie; false once a movie played once has reached its last frame.
    bool advance();
    // +0x08 (0x0055A530): looped never; played once at its last frame, or without a Bink.
    bool finished() const;
    // +0x0C (0x00559A90): the Bink's FrameNum (+0x0C), 0 without one.
    uint32_t frameNum() const;
    // +0x10 (0x00559A80): +0x34, the frames stepped without waiting for their time (PlayFast).
    void setFast(bool fast) { m_fast = fast; }
    // +0x14 (0x0055A790): the volume (+0x60) and the Bink's (0x005B8B50: the volume in hundredths of decibels).
    void setVolume(float volume);
    // 0x00559A60: held (+0x24 1) and stepped twice.
    void restart();

    // +0x24: held (paused by the code, and at the end of a movie played once).
    bool held = false;
    // +0x20: played once (the interface's element not Looped).
    bool playOnce = false;
    // The refcount word's 0x40000000 (0x00559C91): no file of the name, so no Bink.
    bool noMovie() const { return m_noMovie; }
    const std::string& name() const { return m_name; }

private:
    friend class System;
    Player(System& system, std::string name, bool playOnce, const float (&min)[2], const float (&max)[2]);

    bool open();                    // 0x00559B10
    void copy(bool all);            // 0x0055A580
    void decode(bool all);          // 0x0055A680: BinkDoFrame, then the copy
    uint32_t field(uint32_t offset) const;
    void binkVolume(int32_t volume) const;

    System& m_system;
    std::string m_name;          // +0x2C
    float m_min[2] = {};         // +0x10, +0x14: the rectangle on screen in pixels
    float m_max[2] = {};         // +0x18, +0x1C
    uint32_t m_bink = 0;         // +0x30
    bool m_fast = false;         // +0x34
    float m_scaleX = 0.0f;       // +0x40: the rectangle's width / the movie's
    float m_scaleY = 0.0f;       // +0x44
    float m_offsetX = 0.0f;      // +0x48 (0): the frame's share moved right
    float m_offsetY = 0.0f;      // +0x4C (0)
    int32_t m_x = 0;             // +0x50: the rectangle's left, cut to whole pixels
    int32_t m_y = 0;             // +0x54
    float m_alpha = 1.0f;        // +0x58
    float m_volume = 1.0f;       // +0x60
    bool m_first = false;        // +0x64: set as it opens, cleared by the dead first-frame branch of the advance
    uint32_t m_track = 0;        // +0x68: the sound track asked for
    bool m_noMovie = false;
    int m_users = 1;
};

// What the players take from the game.
struct Game {
    std::filesystem::path folder;   // the game's folder (binkw32.dll)
    int languageTrack = 7;          // [0x0075F79C]'s method +0x08: the language's index, the movies' sound track
    uint32_t audioFlags = 0;        // [0x0071E718]
    CrtRandom* random = nullptr;    // the game's rand() (the numbered movies' choice)
};

class System {
public:
    System(render::IRenderDevice& device, Game game);
    ~System();
    System(const System&) = delete;
    System& operator=(const System&) = delete;

    // The one made last; nullptr when there is none.
    static System* get();

    // [0x0075F780]: the window is the active one (WM_ACTIVATEAPP); inactive, the players stand still and silent.
    void setActive(bool active) { m_active = active; }
    // Called after a movie's quad: the 3D renderer's current material ended ([0x0075F16C], [0x00719540] 0x10009).
    void setAfterDraw(std::function<void()> afterDraw) { m_afterDraw = std::move(afterDraw); }

    // 0x0055A9B0: the player of that name if one is kept (another user of it), else a new one (0x0055A7C0) that
    // opens movies\<name>.bik at once; min, max the rectangle on screen in pixels.
    Player* acquire(const std::string& name, bool playOnce, const float (&min)[2], const float (&max)[2]);
    // The user's reference given back: the last one, or any of a player without a movie, frees it (0x0055A300 and
    // its close, 0x0055A140).
    void release(Player* player);

    // The interface's movie: the player [0x0075F838] and its name [0x0075DBD8].
    Player* current = nullptr;
    std::string currentName;
    // [0x0075F834]: the element that drew it last (written, never read); OpenJuice keeps the element's place.
    int currentElement = -1;
    // [0x0075F840]: an element drew the movie this frame (written, never read).
    bool drawn = false;
    // [0x0075F064]: a folder of the game opened this frame (0x004FCA50 through 0x004F9970): no movie is opened or
    // stepped by the interface until the next frame.
    bool hold = false;
    // 0x005AA340: the interface's movie given back.
    void closeCurrent();
    // 0x0040CCD9: the frame's start clears [0x0075F064] and [0x0075F840].
    void newFrame() {
        hold = false;
        drawn = false;
    }

private:
    friend class Player;
    struct Rad3d;
    struct Image;

    bool startRad3d();                                                    // 0x0055B7C0
    std::unique_ptr<Image> makeImage(uint32_t width, uint32_t height, bool alpha);   // 0x0055B740
    std::unique_ptr<Image> makeSingle(uint32_t width, uint32_t height, bool alpha);  // 0x0055B6A0
    std::unique_ptr<Image> makeTiles(uint32_t width, uint32_t height, bool alpha, uint32_t tile);  // 0x0055B060
    render::TextureId makeTexture(uint32_t width, uint32_t height, uint32_t format, uint32_t bytes);  // 0x0055AD80
    void freeImage(std::unique_ptr<Image>& image);                        // 0x0055B790
    bool lock(void** bits, int32_t* pitch, uint32_t* surface, uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h);
    void unlock();
    void blit(float x, float y, float scaleX, float scaleY, float alpha);  // 0x0055B5A0 / 0x0055B3E0
    void states(float alpha);                                              // 0x0055AF90
    void quad(float x, float y, float scaleX, float scaleY, uint32_t width, uint32_t height, float alpha);  // 0x0055AE40

    render::IRenderDevice& m_device;
    Game m_game;
    bink::Library* m_library = nullptr;
    bool m_active = true;
    std::function<void()> m_afterDraw;
    std::unique_ptr<Rad3d> m_rad3d;              // [0x0075F1D8] (and [0x0075F1E0])
    uint32_t m_surfaces[10] = {};                 // [0x0075B324]: Bink's surface type by the rad3d index
    std::unique_ptr<Image> m_image;               // [0x0075F1BC]: the one image all movies share
    uint32_t m_open = 0;                          // [0x0075F1C0]: the movies open
    bool m_firstFrames = false;                   // [0x0075F1C4]: written 0, never set
    uint32_t m_imageWidth = 0, m_imageHeight = 0;  // [0x0075F1C8], [0x0075F1CC]: the size the image was made for
    uint32_t m_frameWidth = 0, m_frameHeight = 0;  // [0x0075F1D0], [0x0075F1D4]: the size of the movie drawn
    std::map<std::string, Player*> m_players;     // the texture manager's movies by name
};

}  // namespace oj::movie
