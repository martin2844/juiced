#include "Game/Race/FrontEnd/RaceFrontEnd.h"

#include "Core/Base/GameFiles.h"
#include "Core/Base/Log.h"
#include "Core/Base/X87.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/OnlineRecords.h"
#include "Core/Race/Racer.h"
#include "Game/Frontend/Network/OnlineFriendWindows.h"
#include "Game/Media/Music.h"

#include <windows.h>

#include <algorithm>
#include <utility>

namespace oj {

void RaceFrontEnd::load(const RaceRules& rules, scene::Engine* engine, ff::Wheels* wheels, float effectsVolume,
                        CrtRandom& loading) {
    if (hudDevice == nullptr || hudLanguage == nullptr || hudTexts == nullptr) return;
    const RaceType type = rules.type;
    // 0x004563E0: with more than one player (or none) the HUD is hud_split, without the map's models.
    hudWindow = rules.players != 1 ? RaceWindowId::HudSplit
                : type == RaceType::Circuit || type == RaceType::PointToPoint || type == RaceType::Unused8
                    ? RaceWindowId::HudRace
                : type == RaceType::Sprint ? RaceWindowId::HudSprint
                : type == RaceType::Cruise ? RaceWindowId::HudCruise
                                           : RaceWindowId::HudSolo;
    // 0x004563E0: the race front end's manager with the bank race, the effects' volume the settings'; its
    // windows play through it.
    raceUi = UiSound{};
    raceUi.open(GameFiles(std::filesystem::current_path()), "race");
    gameSounds().raceFrontEnd = true;
    raceUi.volume = effectsVolume;
    if (wheels != nullptr) wheelsOpened = {wheels->hasDevice(0), wheels->hasDevice(1)};
    ui::Screen::defaultSound = [this](const char* name) { raceUi.play(name); };
    // 0x004563E0: the manager keeps the message windows; by the race's type (one player) hud_challenge_results
    // (types 0 .. 2) or hud_cruise_results (4) with the HUD's window (the HUD's own here, made last); blank ..
    // closedown; the in-game options' (0x00484E20), the online windows' (0x0047FB30); hud_spectator; the save
    // windows' (0x004666E0). The options' and online windows are the options' front end's here.
    raceWindows = std::make_unique<ui::WindowManager>(*hudDevice, *hudLanguage);
    for (const char* name : {"msgbox", "msgbox_critical", "msgbox_network"}) raceWindows->keep(name);
    if (rules.players == 1 && type >= RaceType::TimedLap && type <= RaceType::Speed)
        raceWindows->keep("hud_challenge_results");
    if (rules.players == 1 && type == RaceType::Cruise) raceWindows->keep("hud_cruise_results");
    for (const char* name : {"blank", "blank_split", "hud_pause", "hud_race_complete", "hud_race_results",
                             "hud_sprint_results", "hud_replay", "restart_race", "start_replay", "restart_replay",
                             "closedown", "hud_spectator"}) {
        raceWindows->keep(name);
    }
    options.reset();
    raceOptions();
    hud = std::make_unique<RaceHud>(*hudDevice, *hudLanguage, *hudTexts);
    if (!hud->load(raceWindowName(hudWindow), engine)) {
        hud.reset();
        return;
    }
    // 0x004563E0's table (0x00456A24): the map's models (0x0044A100) for every type of one player but the cruise's
    // and type 8's (no HUD there): the challenges' and the test track's too.
    if (rules.players == 1 && type != RaceType::Cruise && type != RaceType::Unused8)
        hud->prepareMap(m_host.hudRace(), loading);
}

void RaceFrontEnd::closeWindows() {
    hud.reset();
    complete.reset();
    results.reset();
    challengeResults.reset();
    sprintResults.reset();
    cruiseResults.reset();
    pauseWindow.reset();
    message.reset();
    netMessage.reset();
    critical.reset();
    screenWindow.reset();
    hudReplay.reset();
    options.reset();
    raceWindows.reset();
    criticalClose = false;
    noControllerNote = true;
    noControllerNote2 = true;
}

Frontend& RaceFrontEnd::raceOptions() {
    if (!options) {
        options = std::make_unique<Frontend>(*hudDevice, *hudLanguage, *hudTexts, m_host.gameData(),
                                             m_host.saveFolder());
        options->useSounds(raceUi);
        options->keepRaceWindows();
    }
    return *options;
}

void RaceFrontEnd::windowsFrame(float dt) {
    raceUi.frame(dt);  // 0x005A6AB0
    if (criticalClose) {
        critical.reset();
        criticalClose = false;
    }
    windowOpened = false;
    openAskedWindow();
    const uint32_t e = std::exchange(uiEvent, 0u);
    const uint32_t e2 = uiEvent2;
    const uint32_t windowEvent = windowOpened ? 0u : e;
    const uint32_t windowEvent2 = windowOpened ? 0u : std::exchange(uiEvent2, 0u);
    if (critical != nullptr) {
        // 0x005A6B2F: the note of a lost controller takes the events (it has no buttons); the windows wait.
        critical->event(e, e2);
    } else if (netMessage != nullptr) {
        // The network's message's SELECT closes it, or for a note of type 2 the message (0x005A9200).
        const ui::MessageClose close = netMessage->event(e, e2);
        if (close == ui::MessageClose::Network) netMessage.reset();
        if (close == ui::MessageClose::Message) message.reset();
    } else if (message != nullptr) {
        if (message->event(e, e2) != ui::MessageClose::None) message.reset();
    } else if (saveHighScores.isOpen() && m_host.gameData() != nullptr) {
        saveHighScores.update(*this);
    } else if (saveReplay.isOpen() && m_host.gameData() != nullptr) {
        saveReplay.update(*this);
    } else {
        windowFrame(dt, windowEvent, windowEvent2);
    }
}

void RaceFrontEnd::playerMessages(float dt, size_t players) {
    if (hud != nullptr) {
        for (size_t i = 0; i < players && i < 2; ++i) hud->messages(i).update(dt);
    }
}

void RaceFrontEnd::windowFrame(float dt, uint32_t event, uint32_t event2) {
    switch (window.id) {
    case RaceWindowId::OptionsInGame:
    case RaceWindowId::OnlineInGame: {
        if (options == nullptr) break;
        FrontendInput in;
        in.ui = uiFrame;
        in.ui.event = event;
        in.second = m_host.players() > 1;
        in.ui2.event = event2;
        options->update(dt, in);
        if (options->takeInputUi()) m_host.useUi();
        if (const std::string next = options->takeRaceWindow(); !next.empty()) askWindow(next);
        break;
    }
    case RaceWindowId::Pause:
        if (pauseWindow != nullptr) m_host.followWindow(pauseWindow->frame(m_host.hudRace(), event, event2));
        break;
    case RaceWindowId::HudSolo:
    case RaceWindowId::HudRace:
    case RaceWindowId::HudSprint:
    case RaceWindowId::HudCruise:
    case RaceWindowId::HudSplit:
        if (hudShown()) hud->update(m_host.hudRace(), dt);
        break;
    case RaceWindowId::RaceComplete:
        if (complete != nullptr) {
            const std::string next = complete->update(m_host.hudRace(), dt);
            if (!next.empty()) askWindow(next);
        }
        break;
    case RaceWindowId::RaceResults:
        if (results != nullptr) m_host.followWindow(results->frame(m_host.hudRace(), dt, event));
        break;
    case RaceWindowId::ChallengeResults:
        if (challengeResults != nullptr) m_host.followWindow(challengeResults->frame(m_host.hudRace(), event));
        break;
    case RaceWindowId::SprintResults:
        if (sprintResults != nullptr) m_host.followWindow(sprintResults->frame(m_host.hudRace(), dt, event));
        break;
    case RaceWindowId::CruiseResults:
        if (cruiseResults != nullptr) m_host.followWindow(cruiseResults->frame(m_host.hudRace(), event));
        break;
    case RaceWindowId::Closedown:
        // 0x0044FAC0: after its three frames, with no message window open, the game phase "load" with 1.
        if (closedownFrames > 0) {
            --closedownFrames;
        } else if (message == nullptr) {
            unloadAsked = true;
        }
        break;
    case RaceWindowId::RestartRace:
        // 0x00455A90: its time (3) less 1 each frame; with the window's transitions over and the time below 0 the
        // race starts again.
        restartTime = restartTime - 1.0f;
        if ((screenWindow == nullptr || screenWindow->transitionsDone()) && restartTime < 0.0f &&
            m_host.raceState() != nullptr) {
            m_host.restartRace();
        }
        break;
    case RaceWindowId::StartReplay:
    case RaceWindowId::RestartReplay:
        // 0x00455D60 / 0x00456030: its frames less 1; at 0 with the window's transitions over the play starts
        // (Stock problem: past 0 the count goes on down and the play never starts).
        if (--replayFrames == 0 && (screenWindow == nullptr || screenWindow->transitionsDone()) &&
            m_host.raceState() != nullptr) {
            m_host.startReplay(window.id == RaceWindowId::StartReplay);
        }
        break;
    case RaceWindowId::Replay:
        if (hudReplay != nullptr) {
            m_host.followReplayWindow(
                hudReplay->frame(m_host.hudRace(), m_host.replay(), dt, event, uiFrame.anyInput));
        }
        break;
    default: break;
    }
}

void RaceFrontEnd::raceMessage(ui::MessageType type, const std::wstring& text, int32_t* answer, bool startNo) {
    messageNumber = messageNumber + 1 < 9999 ? messageNumber + 1 : 1;
    if (hudDevice == nullptr || hudLanguage == nullptr) return;
    message = std::make_unique<RaceMessage>(windows());
    if (!message->open(type, text, answer, startNo)) message.reset();
}

int32_t RaceFrontEnd::netNote(ui::MessageType type, const std::wstring& text, int32_t* answer) {
    if (saveBusy != 0) return 0;
    if (hudDevice == nullptr || hudLanguage == nullptr) return 0;
    auto m = std::make_unique<RaceMessage>(windows());
    if (!m->open(type, text, answer, false, "msgbox_network")) return 0;
    netMessage = std::move(m);
    return messageNumber;
}

bool RaceFrontEnd::openCritical(uint32_t priority, const std::wstring& text) {
    if (critical != nullptr && !(priority < criticalPriority) && !criticalClose) return false;
    if (hudDevice == nullptr || hudLanguage == nullptr) return false;
    auto m = std::make_unique<RaceMessage>(windows());
    if (!m->open(ui::MessageType::Note, text, nullptr, false, "msgbox_critical")) return false;
    critical = std::move(m);
    criticalPriority = priority;
    criticalClose = false;
    return true;
}

const std::wstring& RaceFrontEnd::jetText(size_t line) {
    static const std::wstring none;
    if (jetTexts.size() == 0 && hudLanguage != nullptr) {
        jetTexts.load(std::filesystem::path("scripts") /
                      ("jettext" + std::string(1, hudLanguage->controlsLetter) + ".txt"));
    }
    return line < jetTexts.size() ? jetTexts.at(line) : none;
}

void RaceFrontEnd::newHighScore() {
    GameData* data = m_host.gameData();
    if (data == nullptr || data->saving == 0) return;
    saveHighScores.enter(*data, m_host.saveFolder());
    raceUi.play("CLICK");  // 0x00466630
}

void RaceFrontEnd::openAskedWindow() {
    if (windowAsked.empty()) return;
    RaceWindowShown asked(std::exchange(windowAsked, {}));
    if (_stricmp(asked.name.c_str(), window.name.c_str()) == 0) return;
    leaveWindow();
    window = std::move(asked);
    windowOpened = true;
    // The results' and the replay's windows let the controller go as they enter (0x00450C23, 0x0045148E,
    // 0x00453258, 0x00454C9F, 0x0044DBAF): the keyboard's map UI.
    switch (window.id) {
    case RaceWindowId::ChallengeResults:
    case RaceWindowId::CruiseResults:
    case RaceWindowId::RaceResults:
    case RaceWindowId::SprintResults:
    case RaceWindowId::Replay: m_host.useUi(); break;
    default: break;
    }
    if (!enterWindow() && window.id != RaceWindowId::Blank) {
        log::info("race: the window {} is not made yet", window.name);
    }
    const RaceState* race = m_host.raceState();
    log::info("race: {} at the count {:.2f}", window.name, race != nullptr ? race->count : 0.0f);
}

void RaceFrontEnd::leaveWindow() {
    switch (window.id) {
    case RaceWindowId::HudSolo:
    case RaceWindowId::HudRace:
    case RaceWindowId::HudSprint:
    case RaceWindowId::HudCruise:
    case RaceWindowId::HudSplit:
        if (hudShown()) hud->leave();
        break;
    case RaceWindowId::Replay:
        if (hudReplay != nullptr) hudReplay->leave(m_host.replay());
        break;
    case RaceWindowId::RaceResults:
        // 0x00453420, hud_race_results' leave: an online career's crew race reports our player's finish to the
        // statistics (0x004ECD70; sent with the next report).
        if (net::matchmaking() != nullptr && net::onlineHosted().mode == net::OnlineKind::Career &&
            net::onlineHosted().career.team > 1 && net::stats::reporter() != nullptr) {
            net::stats::raceFinished();
        }
        break;
    case RaceWindowId::OptionsInGame:
    case RaceWindowId::OnlineInGame:
        if (options != nullptr) options->closeWindows();
        break;
    case RaceWindowId::SprintResults:
        // 0x00454D80, hud_sprint_results' leave: the 3D world's voices paused (its manager's +0x20 with 1).
        m_host.pauseWorldVoices();
        break;
    case RaceWindowId::RestartRace:
    case RaceWindowId::StartReplay:
    case RaceWindowId::RestartReplay:
        // 0x00455F50, the leave of restart_race, start_replay and restart_replay: with the sound system the game's
        // sounds given back whatever the race (the speech, the 3D world's voices, the music's song where it was, the
        // race front end's player) and the race front end watching the controllers again (+0x38 1): a sprint's next
        // heat goes on with the song of the last.
        pauseGameSounds(false, &raceUi);
        m_host.setSoundsHeld(false);
        m_host.setSoundsPaused(m_host.paused());
        break;
    default: break;
    }
}

bool RaceFrontEnd::enterWindow() {
    switch (window.id) {
    case RaceWindowId::HudSolo:
    case RaceWindowId::HudRace:
    case RaceWindowId::HudSprint:
    case RaceWindowId::HudCruise:
    case RaceWindowId::HudSplit:
        if (!hudShown()) return false;
        hud->enter(m_host.hudRace());
        return true;
    case RaceWindowId::RaceComplete:
        if (hudDevice == nullptr) return false;
        complete = std::make_unique<RaceComplete>(windows(), *hudTexts);
        if (!complete->enter(m_host.hudRace(), hud.get())) complete.reset();
        return true;
    case RaceWindowId::Pause:
        if (hudDevice == nullptr) return false;
        pauseWindow = std::make_unique<RacePause>(windows(), *hudTexts);
        if (!pauseWindow->enter(m_host.hudRace())) pauseWindow.reset();
        return true;
    case RaceWindowId::ChallengeResults:
        // 0x00450B60: its enter's challenge result (0x00450C28) first.
        if (RaceState* race = m_host.raceState()) race->challengeResult();
        if (hudDevice != nullptr) {
            challengeResults = std::make_unique<ChallengeResults>(windows(), *hudTexts);
            if (!challengeResults->enter(m_host.hudRace(), hud.get())) challengeResults.reset();
        }
        return true;
    case RaceWindowId::CruiseResults:
        if (hudDevice == nullptr) return false;
        cruiseResults = std::make_unique<CruiseResults>(windows(), *hudTexts);
        if (!cruiseResults->enter(m_host.hudRace(), hud.get())) cruiseResults.reset();
        m_host.netCruiseBoard();  // 0x004B0A50 from its enter on the network
        return true;
    case RaceWindowId::SprintResults:
        if (hudDevice == nullptr) return false;
        sprintResults = std::make_unique<SprintResults>(windows(), *hudTexts);
        if (!sprintResults->enter(m_host.hudRace(), hud.get())) sprintResults.reset();
        return true;
    case RaceWindowId::OptionsInGame:
        if (hudDevice == nullptr) return false;
        // 0x0047FF60 with the game object's +0x3C: opts_ingame_menu over it.
        raceOptions().startRaceOptions(m_host.pausingPlayer());
        return true;
    case RaceWindowId::OnlineInGame:
        if (hudDevice == nullptr) return false;
        // 0x004739C0 in the race's windows: the online window of [0x0072182C] over it.
        raceOptions().startRaceOnline();
        return true;
    case RaceWindowId::RaceResults:
        if (hudDevice == nullptr) return false;
        results = std::make_unique<RaceResults>(windows(), *hudTexts);
        if (!results->enter(m_host.hudRace(), hud.get())) results.reset();
        return true;
    case RaceWindowId::Closedown: {
        // 0x0044F9C0: three frames to wait; the replay stopped (+0x828 0); the race paused as 0x004263E0 (its
        // sounds with it: pauseSounds); the HUD's +0x38 cleared.
        m_host.replay().playing = 0;
        m_host.syncReplayFlags();
        closedownFrames = 3;
        m_host.setPaused(true);
        screenWindow = std::make_unique<RaceScreenWindow>(windows());
        if (!screenWindow->load("closedown")) screenWindow.reset();
        return true;
    }
    case RaceWindowId::Spectator: {
        if (hudDevice == nullptr) return false;
        // 0x00455880: with fewer than two players in the race RP_NEXTCAR, RP_NEXTCARTEXT and RP_BG1 hidden.
        screenWindow = std::make_unique<RaceScreenWindow>(windows());
        const net::NetSession* session = m_host.netSession();
        if (!screenWindow->load("hud_spectator")) {
            screenWindow.reset();
        } else if (session != nullptr && session->racing() < 2) {
            for (const char* e : {"RP_NEXTCAR", "RP_NEXTCARTEXT", "RP_BG1"}) screenWindow->hide(e);
        }
        return true;
    }
    case RaceWindowId::RestartRace:
        // 0x00455940: the time 3 (+0x10 1, 0 with the statistics outside a replay; read by nothing); the speech
        // stopped (0x0040B600 inlined), the game's sounds paused as start_replay's (the speech, the 3D world's
        // voices, the music, the race front end's player); the HUD's +0x38 cleared.
        restartTime = 3.0f;
        if (Speech* speech = gameSounds().speech) speech->stop();
        pauseGameSounds(true, &raceUi);
        m_host.setSoundsHeld(true);
        screenWindow = std::make_unique<RaceScreenWindow>(windows());
        if (!screenWindow->load("restart_race")) screenWindow.reset();
        return true;
    case RaceWindowId::StartReplay:
    case RaceWindowId::RestartReplay:
        // 0x00455BD0 / 0x00455E20: the speech stopped (0x0040B600); start_replay: the 3D world at once at 1 x the
        // effects' volume (down 20, up 50, its player's master) and the race a replay's ([G+0x6C]+0x68); then with
        // the sound system the game's sounds paused (as 0x0040D8E0: the speech, the 3D world's voices, the music,
        // the race front end's player); the HUD's +0x38 cleared, five frames to wait.
        if (Speech* speech = gameSounds().speech) speech->stop();
        if (window.id == RaceWindowId::StartReplay) {
            m_host.worldSoundsAtOnce();
            m_host.replay().replayRace = 1;
        }
        pauseGameSounds(true, &raceUi);
        // The play's resume (0x00426B80 ..) gives them back as the race goes on.
        m_host.setSoundsPaused(true);
        m_host.syncReplayFlags();
        replayFrames = 5;
        screenWindow = std::make_unique<RaceScreenWindow>(windows());
        if (!screenWindow->load(window.name)) screenWindow.reset();
        return true;
    case RaceWindowId::Replay:
        if (hudDevice == nullptr) return false;
        hudReplay = std::make_unique<HudReplay>(windows(), *hudTexts);
        if (!hudReplay->enter(m_host.hudRace(), m_host.replay())) hudReplay.reset();
        return true;
    case RaceWindowId::BlankSplit:
        if (hudDevice == nullptr) return false;
        // The record 0x00720584: the views' divider (DIVIDER) alone.
        screenWindow = std::make_unique<RaceScreenWindow>(windows());
        if (!screenWindow->load("blank_split")) screenWindow.reset();
        return true;
    default: return false;
    }
}

void RaceFrontEnd::draw(render::IRenderDevice& device, float seconds, float dt, bool presence) {
    // 0x005A9D48: every element with no message up, with the note of a lost controller or the network's message up.
    ui::Screen::setDrawingUnderMessage(message != nullptr && messageNumber != 0 && critical == nullptr &&
                                               netMessage == nullptr
                                           ? ui::Screen::Under::kFramesOnly
                                           : ui::Screen::Under::kAll);
    drawWindow(seconds, dt);
    ui::Screen::setDrawingUnderMessage(ui::Screen::Under::kAll);
    if (message != nullptr) message->draw(seconds, dt);
    // The race front end's black over its windows, clearing by the frame time.
    if (raceFade > 0.0f) {
        const auto alpha = static_cast<uint32_t>(x87::chop(std::min(raceFade, 1.0f) * 255.0f)) & 0xFF;
        const float w = static_cast<float>(device.width());
        const float h = static_cast<float>(device.height());
        const render::QuadVertex corners[4] = {
            {0.0f, 0.0f, 0.0f, 0.0f, alpha << 24},
            {w, 0.0f, 1.0f, 0.0f, alpha << 24},
            {0.0f, h, 0.0f, 1.0f, alpha << 24},
            {w, h, 1.0f, 1.0f, alpha << 24},
        };
        device.drawQuad(corners, 0, render::Blend::Alpha);
        raceFade = std::clamp(raceFade - dt, 0.0f, 1.0f);
    }
    // 0x005A6D15: over the black the network's message (+0x3C), then the note of a lost controller (+0x34).
    if (netMessage != nullptr) netMessage->draw(seconds, dt);
    if (critical != nullptr) critical->draw(seconds, dt);
    // 0x0045846D: after the race front end's window manager the note of an invitation (0x004E1860), unless the
    // frame ended in the network's watch.
    if (hud != nullptr && presence) frontend::drawPresenceIcon(device, hud->screen(), window.name, true);
}

void RaceFrontEnd::drawWindow(float seconds, float dt) {
    switch (window.id) {
    case RaceWindowId::HudSolo:
    case RaceWindowId::HudRace:
    case RaceWindowId::HudSprint:
    case RaceWindowId::HudCruise:
    case RaceWindowId::HudSplit:
        if (hudShown()) hud->draw(seconds, dt);
        break;
    case RaceWindowId::RaceComplete:
        if (complete != nullptr) complete->draw(seconds, dt);
        break;
    case RaceWindowId::RaceResults:
        if (results != nullptr) results->draw(seconds, dt);
        break;
    case RaceWindowId::ChallengeResults:
        if (challengeResults != nullptr) challengeResults->draw(seconds, dt);
        break;
    case RaceWindowId::SprintResults:
        if (sprintResults != nullptr) sprintResults->draw(seconds, dt);
        break;
    case RaceWindowId::CruiseResults:
        if (cruiseResults != nullptr) cruiseResults->draw(seconds, dt);
        break;
    case RaceWindowId::Pause:
        if (pauseWindow != nullptr) pauseWindow->draw(seconds, dt);
        break;
    case RaceWindowId::OptionsInGame:
    case RaceWindowId::OnlineInGame:
        if (options != nullptr) options->draw();
        break;
    case RaceWindowId::Replay:
        if (hudReplay != nullptr) hudReplay->draw(seconds, dt);
        break;
    case RaceWindowId::Closedown:
    case RaceWindowId::RestartRace:
    case RaceWindowId::Spectator:
    case RaceWindowId::BlankSplit:
    case RaceWindowId::StartReplay:
    case RaceWindowId::RestartReplay:
        if (screenWindow != nullptr) screenWindow->draw(seconds, dt);
        break;
    default: break;
    }
}

}  // namespace oj
