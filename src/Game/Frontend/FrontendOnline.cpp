// The front end's side of the online sessions (docs/spec/28-network.md, "The session windows"): the online career,
// a session hosted or joined (what the session windows and the friends' ask through NetworkHost).

#include "Game/Frontend/Frontend.h"
#include "Game/Frontend/FrontendInternal.h"

#include "Core/Career/Career.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Data/GameTexts.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetSession.h"
#include <algorithm>
#include <cmath>
#include <format>

namespace oj {

using namespace frontend;

namespace {

// The race of a network session (0x00473600 / 0x00473760 / 0x00473190): [G+0x6C] mode 1, type 5, one player (+0x58), no
// opponents (+0x5C), one lap (+0x50).
void onlineRace(RaceSetup& race) {
    race.mode = RaceMode::Custom;
    race.type = RaceType::Circuit;
    race.players = 1;
    race.opponents = 0;
    race.laps = 1;
}

}  // namespace

bool Frontend::onlineCareer() const {
    return net::matchmaking() != nullptr && net::onlineHosted().mode == net::OnlineKind::Career;
}

// 0x004A44E0: the player's first racer's member slot 0 and its cars chosen ([G+0x80]+4: +0x5F4, +0x614 1);
// CMSOnlineRaceSetup for a solo session ([0x00728CEC] 1) or a race of type 4, else CMSOnlineCrewRaceSetup; CLICK.
void Frontend::onlineCareerRaceSetup() {
    if (m_data != nullptr && m_data->career) {
        CareerProfile& p = m_data->career->profile;
        p.raceMembers[0] = 0;
        p.raceCarsChosen = 1;
    }
    const bool solo =
        net::onlineHosted().career.team == 1 || (m_data != nullptr && m_data->race.type == RaceType::Cruise);
    request(solo ? "CMSOnlineRaceSetup" : "CMSOnlineCrewRaceSetup", SwitchMode::Replace);
    sound("CLICK", 1.0f, 0xF);
}

// 0x004739A0: the matchmaking there and the arcade: 0x00473600, else 0x00473760.
void Frontend::onlineHostIntoLobby() {
    if (net::matchmaking() != nullptr && net::onlineHosted().mode == net::OnlineKind::Arcade) {
        onlineHostArcade();
    } else {
        onlineHostCareer();
    }
}

// 0x00473600: each of the GP user's local players added as ours (its nick, 0x004E9E40 with EDX 0 and EDI the player);
// the transport hosting the arcade session's name (0x00728D0C) with our players, 6 at most, its own address (-1), and
// with the matchmaking not answering LAN searches (+0x10 0); the stage 0 (0x004EA850); with GP the users' state 1 and
// public (0x004EEAF0); the race (mode 1, type 5, 1 player, no opponents, 1 lap); CLICK; link_lobby.
void Frontend::onlineHostArcade() {
    net::NetGame* net = net::netGame();
    if (net == nullptr) return;
    for (int32_t player : net::onlineUserIds()) net->session.addOwnPlayer(net::onlineNick(player), 0, player, voiceSettings());
    net->transport.host(net::nameText(net::onlineHosted().arcade.name),
                        static_cast<uint32_t>(net->session.players().size()), 6);
    if (net::matchmaking() != nullptr) net->transport.setAnswering(false);
    net->session.setStage(net::SessionStage::Lobby);
    if (net::onlineUsersPresent()) net::setOnlineUsersState(1, net::onlineHosted().arcade.access == 0 ? 1 : 0);
    if (m_data != nullptr) onlineRace(m_data->race);
    sound("CLICK", 1.0f, 0xF);
    request("link_lobby", SwitchMode::Replace);
}

// 0x00473760: as the arcade's, then the team's other places (team - 1) taken by CPU crew mates (the first local player's
// nick, ":CPU<k>", EDI -1); the career session's name (0x00728CC4) and 2 at most for a pink slip (0x00728D00), else 6;
// the users' state 1 and public (0x00728CC0's access 0). Stock problem: GP's user object ([0x0075F040]) is read without
// a check (there always is one online); here none passes over its players.
void Frontend::onlineHostCareer() {
    net::NetGame* net = net::netGame();
    if (net == nullptr) return;
    const net::CareerSession& hosted = net::onlineHosted().career;
    const std::vector<int32_t> players = net::onlineUserIds();
    for (int32_t player : players) net->session.addOwnPlayer(net::onlineNick(player), 0, player, voiceSettings());
    const std::u16string first = net::onlineNick(players.empty() ? -1 : players.front());
    for (int32_t k = 1; k < hosted.team; ++k) net->session.addOwnPlayer(first, k, -1, voiceSettings());
    net->transport.host(net::nameText(hosted.name), static_cast<uint32_t>(net->session.players().size()),
                        hosted.pinkslip != 0 ? 2 : 6);
    if (net::matchmaking() != nullptr) net->transport.setAnswering(false);
    net->session.setStage(net::SessionStage::Lobby);
    if (net::onlineUsersPresent()) net::setOnlineUsersState(1, hosted.access == 0 ? 1 : 0);
    if (m_data != nullptr) onlineRace(m_data->race);
    sound("CLICK", 1.0f, 0xF);
    request("link_lobby", SwitchMode::Replace);
}

// 0x004734E0: the session's common settings of the position (0x004E6440) and its career flag as the online mode
// ([0x0075EF88]). The matchmaking there and the arcade: 0x00473190, 1. The career: without a career save the message
// closed and "You must have a Career Mode save game" (type 1), with an empty garage "You don't have any cars in your
// garage", 0; the session's career settings (0x004E6570) checked against the garage with notes (0x004A4590), 0 when
// they fail; 0x004732D0, 1.
bool Frontend::onlineJoinSession(int32_t position) {
    int32_t access = 0, playing = 0, career = 0;
    std::array<char16_t, 16> name{};
    net::readSession(position, access, name, playing, career);
    net::onlineHosted().mode = static_cast<net::OnlineKind>(career);
    if (net::matchmaking() != nullptr && career == 0) {
        onlineJoinArcade(position);
        return true;
    }
    const Career* c = m_data != nullptr ? m_data->career.get() : nullptr;
    if (c == nullptr || c->head == 0) {
        if (m_message) m_message->close = true;
        message(ui::MessageType::Continue, m_gameTexts.at(kTextNoCareer), false, nullptr);
        return false;
    }
    if (c->garage.empty()) {
        if (m_message) m_message->close = true;
        message(ui::MessageType::Continue, m_gameTexts.at(kTextNoCars), false, nullptr);
        return false;
    }
    net::CareerSession session;
    net::readCareer(position, session);
    size_t note = 0;
    if (!net::careerCheck(*c, session.team, session.lower, session.upper, session.criteriaType, session.criteria,
                          session.pinkslip, true, &note)) {
        if (note != 0) message(ui::MessageType::Continue, m_gameTexts.at(note), false, nullptr);
        return false;
    }
    onlineJoinCareer(position);
    return true;
}

// 0x00473190: the session's arcade settings read (for the users' state); each local player of GP's user added as ours;
// the settings kept as the hosted arcade one (0x00728D08, 0x004E64F0); the matchmaking's join of the position (Stock
// problem: a raw index of its results) with our players; the users' state 1 and public; the race (1, 5, 1, 0, 1). The
// caller waits for the session.
void Frontend::onlineJoinArcade(int32_t position) {
    net::NetGame* net = net::netGame();
    if (net == nullptr) return;
    net::ArcadeSession settings;
    net::readArcade(position, settings);
    for (int32_t player : net::onlineUserIds()) net->session.addOwnPlayer(net::onlineNick(player), 0, player, voiceSettings());
    net::readArcade(position, net::onlineHosted().arcade);
    if (net::Matchmaking* mm = net::matchmaking()) {
        mm->join(static_cast<uint32_t>(position), static_cast<uint32_t>(net->session.players().size()));
    }
    if (net::onlineUsersPresent()) net::setOnlineUsersState(1, settings.access == 0 ? 1 : 0);
    if (m_data != nullptr) onlineRace(m_data->race);
}

// 0x004732D0: as the arcade's, with the team's CPU crew mates added (team - 1, the first player's nick) and the
// settings kept as the hosted career one (0x00728CC0, 0x004E6570); the join's players count them.
void Frontend::onlineJoinCareer(int32_t position) {
    net::NetGame* net = net::netGame();
    if (net == nullptr) return;
    net::CareerSession settings;
    net::readCareer(position, settings);
    const std::vector<int32_t> players = net::onlineUserIds();
    for (int32_t player : players) net->session.addOwnPlayer(net::onlineNick(player), 0, player, voiceSettings());
    const std::u16string first = net::onlineNick(players.empty() ? -1 : players.front());
    for (int32_t k = 1; k < settings.team; ++k) net->session.addOwnPlayer(first, k, -1, voiceSettings());
    net::readCareer(position, net::onlineHosted().career);
    if (net::Matchmaking* mm = net::matchmaking()) {
        mm->join(static_cast<uint32_t>(position), static_cast<uint32_t>(net->session.players().size()));
    }
    if (net::onlineUsersPresent()) net::setOnlineUsersState(1, settings.access == 0 ? 1 : 0);
    if (m_data != nullptr) onlineRace(m_data->race);
}

}  // namespace oj
