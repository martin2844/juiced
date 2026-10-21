// vid_tutorial: the tutorial movies.

#include "Game/Frontend/Menus/VidTutorial.h"

#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

// vid_tutorial's enter (0x0046A040): MOVIE (+0x10) and MENU (+0x0C) kept, MENU's first item highlighted, COVER shown,
// MOVIE plays whatis (whatis_spanish in Spanish) looped (0x005AA300 with 1) at the effects' volume (MOVIE's +0x114
// [G+0x64]+0x58, the settings' +0x04), COVER hidden.
void VidTutorial::enter(WindowHost& host, ui::Screen& s) {
    s.selectMenuItem("MENU", 0);
    s.showContaining("COVER", false);
    const int movie = s.find("MOVIE");
    if (movie >= 0) {
        s.playMovie(movie, host.language().index == 4 ? "whatis_spanish" : "whatis", true);
        s.setMovieVolume(movie, std::bit_cast<float>(host.settingsValue(save::Settings::kEffectsVolume)));
    }
    s.hideContaining("COVER", true);
}

// vid_tutorial's update (0x0046A0D0): SELECT with MENU and MOVIE: the highlighted item names the movie played looped
// on MOVIE (0x005AA300 with 1), COVER hidden: 180, boot (bootleg), boomerang, what (whatis, whatis_spanish in
// Spanish), 360, donut, jturn, drift, combo; done goes back to the window that asked for it ([0x0072158C], mode 1);
// then MOVIE's volume the effects' volume ([G+0x64]+0x58) and CLICK. BACK (CLICK) goes back as done.
void VidTutorial::update(WindowHost& host, ui::Screen& s, uint32_t event) {
    if (event == input::kSelect) {
        const int movieElement = s.find("MOVIE");
        if (s.find("MENU") < 0 || movieElement < 0) return;
        const std::string item = s.menuItemCommand("MENU", s.selectedMenuItem("MENU"));
        static const std::pair<const char*, const char*> kMovies[] = {
            {"180", "180"},   {"boot", "bootleg"}, {"boomerang", "boomerang"}, {"360", "360"},
            {"donut", "donut"}, {"jturn", "jturn"}, {"drift", "drift"},         {"combo", "combo"},
        };
        const char* movie = nullptr;
        for (const auto& [name, file] : kMovies) {
            if (_stricmp(item.c_str(), name) == 0) movie = file;
        }
        if (_stricmp(item.c_str(), "what") == 0) movie = host.language().index == 4 ? "whatis_spanish" : "whatis";
        if (movie != nullptr) {
            s.playMovie(movieElement, movie, true);
            s.hideContaining("COVER", true);
        } else if (_stricmp(item.c_str(), "done") == 0 && !host.switchAsked()) {
            host.request(host.shared().tutorialReturn, SwitchMode::Replace);
        }
        s.setMovieVolume(movieElement, std::bit_cast<float>(host.settingsValue(save::Settings::kEffectsVolume)));
        host.sound("CLICK");
    } else if (event == input::kBack) {
        host.sound("CLICK");
        if (!host.switchAsked()) host.request(host.shared().tutorialReturn, SwitchMode::Replace);
    }
}

}  // namespace oj::frontend
