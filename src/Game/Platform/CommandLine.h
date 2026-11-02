#pragma once
// The command line as the stock game reads it (docs/spec/01-startup.md), plus OpenJuice's -setup and, in the debug
// builds only, OpenJuice's switches of scripted runs, which start with "--" (the release builds read them as the stock
// game does: as no switch).

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace oj {

struct LaunchOptions {
    // Switches of WinMain
    float scale = 0.0f;           // -1 .. -7: 2, 4, 6, 8, 32, 64, 100
    bool localEffects = false;    // -localfx
    bool debugEffects = false;    // -debugfx
    bool pureDevice = true;       // cleared by -nonpured3d
    bool multithreadedD3D = false;  // -mtd3d
    bool rf = false;              // -rf: depth of field for every view ([0x0075F14C]); read, the fixed build draws none
    bool presentIntervalOne = false;  // -r
    uint32_t audioFlagsOr = 0;    // -x: | 8
    bool audioFlagsOne = false;   // -n: audio flags = 1
    bool softwareVertexProcessing = false;  // -s

    // Arguments of the main function
    bool instantRace = false;     // -i
    bool wet = false;             // -wet
    bool m = true;                // cleared by -m: the instant race's gearbox (1, else 0)
    bool cruise = false;          // -cruise: compared, then nothing (the stock code uses no result)
    bool automod = false;         // -automod N
    long automodValue = 0;
    bool host = false;            // -host...
    bool connect = false;         // -connect <address>
    std::string connectAddress;
    std::wstring playerName;      // -name <name>, 30 characters at most
    std::wstring email;           // -email <address>, 50 characters at most
    std::wstring password;        // -ppass <password>, 30 characters at most

    // OpenJuice
    bool setup = false;           // -setup: the game's configuration tool (JuicedConfig.exe) only, no game

    // OpenJuice's switches of scripted runs, the debug builds only
    uint64_t frames = 0;          // --frames N: quit after N frames
    // --profile N: the frames' layers timed from the N-th frame (IRenderDevice::profileMark), their table logged at
    // the end
    uint64_t profile = 0;
    std::string screen = "disclaimer";  // --screen <window>: the first interface window (the stock game opens with
                                        // "disclaimer", "loading" with -i); a window past the ones before it has
                                        // the state they leave
    std::string capture;          // --capture <file>: the last frame written as a BMP
    // --headlights N: the fixed build's headlights (the registry's Headlights 0 .. 2) whatever the settings and --test
    std::optional<uint32_t> headlights;
    // --world-light N: the fixed build's world light (the registry's WorldLight 0 .. 2) whatever the settings and
    // --test
    std::optional<uint32_t> worldLight;
    std::optional<uint32_t> sky;  // --sky N: the fixed build's sky (the registry's Sky 0 .. 2) whatever the settings
    std::optional<uint32_t> carPaint;  // --car-paint 0 / 1: CAR PAINT for the run (with --test)
    std::string shadowMap;        // --shadow-map <file>: the first light's shadow map at the last frame, grey, as a BMP
    std::string sunMap;           // --sun-map <file>: the sun's cascades' atlas at the last frame, grey, as a BMP
    // --reflections N: the graphics options' REFLECTION 0 .. 3 (OFF .. HI) or 4 (the fixed build's ULTRA) for --track
    std::optional<uint32_t> reflections;
    // --glow N: the graphics options' LIGHT FLARES for --track: 0 OFF, 1 STOCK, 2 CUSTOM
    std::optional<uint32_t> glow;
    // --caster-pixels X: the shadow maps' casters under X pixels across from the view left out (0: none)
    std::optional<float> casterPixels;
    // --user-settings: with --test the fixed build's renderer settings of the registry kept (the anti-aliasing, the
    // texture filter, the effects), the window still hidden
    bool userSettings = false;
    // --cube-map <file>: the cars' cubes at the last frame as BMPs, <file>_stock_<face> and with REFLECTIONS ULTRA
    // <file>_cars_<face>
    std::string cubeMap;
    std::string occlusionMap;     // --ao-map <file>: the last ambient occlusion (WORLD LIGHT HI) as a BMP; beside it
                                  // the lights' reflection, the street lamps' shade, the first lamp's map and
                                  // the light in the haze
    std::string saves;            // --saves <folder>: the folder holding SAVE, instead of OpenJuice.ini's
    std::optional<uint32_t> seed;  // --seed N: the game object's rand() seed instead of the timer's
    // --press F:KEY,F-G:KEY,...: key KEY (a DIKEYBOARD_ name without the prefix, e.g. RETURN, or a keyboard code)
    // goes down at frame F and up at the next, or held from frame F until frame G.
    struct Press {
        uint64_t down = 0;
        uint64_t up = 0;
        uint8_t code = 0;
    };
    std::vector<Press> presses;
    bool test = false;            // --test: hidden, windowed, no system changes, no configuration tool
    std::string track;            // --track <Track>/<Route>[/<time of day>[/<weather>]]: instead of the interface, a
                                  // view of the route from behind its first grid place (the race's time of day 1 .. 4
                                  // and weather 2 wet as in the race)
    std::string car;              // --car <name>: with --track, a stock car of cardefs.txt's <name> on each grid place
    int cars = 0;                 // --cars N: with --car, on the first N grid places only (0: each)
    std::string carAhead;         // --car-ahead <right>,<forward>: with --car, the second car that far from the first
                                  // grid place in cm (to see the first's headlights on it)
    std::string trackCamera;      // --track-camera <right>,<up>,<back>[,<right>,<forward>]: with --track, the
                                  // camera's place from the first grid place in cm (default 0,300,900); it looks at
                                  // the place 1 m up (or at the point the last two give from it), held there through
                                  // the race when given
    std::string net;              // --net <lan>,<game>,<to>[,<ip>]: the network's ports (8666 and 8667), the LAN port
                                  // the search's broadcasts go to and the address (a.b.c.d) they go to and this
                                  // machine gives as its own: two games on one machine (127.0.0.1)
    bool autopilot = false;       // --autopilot: the player's car in a race is driven by its competitor's AI (+8 = 1)
                                  // from the start (not of the game: to watch a race through)
    uint32_t replayCaptureFrame = 0; // --replay-capture-frame N: with --test only,
                                    // fixed 1/60 updates, capture/exit at replay time N/60.
    int players = 0;              // --players N: the race's players ([G+0x6C]+0x58) and, with more than one, a split
                                  // screen race (mode 3), as multi's "split" leaves them (not of the game: to see a
                                  // split screen race with one device)

    // Every word of the command line, in order (quotes removed), as the main function receives them.
    std::vector<std::string> arguments;
};

LaunchOptions parseCommandLine(const char* commandLine);

}  // namespace oj
