#include "Game/Frontend/Career/Gamble.h"

#include "Core/Base/Text.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Cars/CarRepair.h"
#include "Game/Frontend/WindowTools.h"
#include "Game/Media/Movies.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <numeric>
#include <thread>

namespace oj::frontend {
namespace {
constexpr const char *names[] = {"You", "TK", "Biggi", "Sue", "Maria", "Jack", "Melody", "Poppa", "Carlos"};

// Estimate using only this player's cards and the public board. Never inspect opponents' hole cards or deck.
float equity(const std::array<poker::Card, 2> &hole, const std::vector<poker::Card> &board, int opponents,
             std::mt19937 &rng) {
    std::vector<poker::Card> unseen;
    for (int c = 0; c < 52; ++c)
        if (c != hole[0] && c != hole[1] && std::find(board.begin(), board.end(), c) == board.end())
            unseen.push_back(c);
    float wins = 0;
    constexpr int samples = 64;
    for (int n = 0; n < samples; ++n) {
        std::shuffle(unseen.begin(), unseen.end(), rng);
        auto shared = board;
        size_t at = 0;
        while (shared.size() < 5)
            shared.push_back(unseen[at++]);
        auto own = shared;
        own.insert(own.end(), hole.begin(), hole.end());
        const auto rank = poker::evaluate(own);
        bool beaten = false;
        int tied = 1;
        for (int i = 0; i < opponents; ++i) {
            auto cards = shared;
            cards.push_back(unseen[at++]);
            cards.push_back(unseen[at++]);
            const auto other = poker::evaluate(cards);
            beaten |= other > rank;
            if (other == rank)
                ++tied;
        }
        if (!beaten)
            wins += 1.0f / tied;
    }
    return wins / samples;
}
} // namespace

void Gamble::enter(CareerHost &host, ui::Screen &s) {
    m_previewView.reset();
    m_previewIndex = -1;
    m_game.reset();
    m_confirm = -1;
    m_titleCar = -1;
    m_betting = m_betTitle = false;
    m_pledgeConfirm = -1;
    m_clock = 0;
    m_speaker = -1;
    m_status = "Texas Hold'em / cash and pink slips";
    const auto *data = host.gameData();
    m_carNames.clear();
    m_carValues.clear();
    if (data && data->career) {
        for (const auto &car : data->career->garage) {
            m_carNames.push_back(car.def ? car.def->displayName : car.modelName());
            m_carValues.push_back(car.def ? saleWorth(car, data->catalogue) : 0);
        }
    }
    m_garageSize = m_carNames.size();
    if (data && host.engine()) {
        scene::CarView::Setup setup;
        setup.element = "TITLE_CAR_VIEW";
        setup.zoom = .65f;
        setup.shadow = false;
        m_previewView = std::make_unique<scene::CarView>(*host.engine(), s, setup, data->catalogue, data->tables);
    }
    framePreview(host, s);
    s.setText("TITLE", L"GAMBLE");
    s.setText("HINT", L"Choose with D-pad / Confirm: A or Enter / Back: B or Esc");
    host.setMusicVolume(host.settingsMusic() * 0.35f, false);
    menu(s);
    texts(s);
    reaction(host, s, 1, "_GREETING1");
}

void Gamble::leave(CareerHost &host) {
    m_previewView.reset();
    m_previewIndex = -1;
    host.setMusicVolume(host.settingsMusic(), false);
    if (auto *movies = movie::System::get())
        movies->closeCurrent();
    m_game.reset();
}

void Gamble::framePreview(CareerHost &host, ui::Screen &s) {
    const bool show = m_titleCar >= 0 && (!m_game || !m_game->hand.finished());
    for (const char *id : {"TITLE_CAR_BACK", "TITLE_CAR_EDGE", "TITLE_CAR_VIEW", "TITLE_CAR_NAME", "TITLE_CAR_LOADING"})
        s.setHidden(id, !show);
    if (!show)
        return;
    const auto *data = host.gameData();
    if (m_previewView && m_previewIndex != m_titleCar && data && data->career &&
        static_cast<size_t>(m_titleCar) < data->career->garage.size()) {
        m_previewView->clear();
        m_previewCar = data->career->garage[m_titleCar];
        m_previewIndex = m_titleCar;
        m_previewView->show(m_previewCar, nullptr, 0, 3.74f, 0.0f, false, host.viewLoading());
        m_previewView->setCameraPlace({0.0f, 110.0f, -560.0f, 0.0f});
    }
    if (m_previewView)
        m_previewView->frame(host.frameTime(), host.viewLoading());
    if (m_previewView && m_previewView->busy() && host.engine()) {
        // Native GPU uploads are serviced on the render thread. Once per frame makes
        // each serial mesh upload cost a full frame; drain briefly while this car loads.
        const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(2);
        do {
            host.engine()->device().letLoadingRun();
            m_previewView->frame(0, host.viewLoading());
            if (!m_previewView->busy()) break;
            std::this_thread::yield();
        } while (std::chrono::steady_clock::now() < until);
    }
    if (auto *view = dynamic_cast<scene::View3d *>(s.view("TITLE_CAR_VIEW")); view && host.engine()) {
        const auto &device = host.engine()->device();
        view->camera().setAspect((.15f * device.height()) / std::max(1.0f, .24f * device.width()));
    }
    s.setText("TITLE_CAR_NAME", s.fitText("TITLE_CAR_NAME", toWide(m_carNames[m_titleCar])));
    s.setText("TITLE_CAR_LOADING", m_previewView && m_previewView->busy() ? L"Loading car..." : L"Preview unavailable");
    s.setHidden("TITLE_CAR_LOADING", previewReady());
}

void Gamble::menu(ui::Screen &s) {
    s.clearMenu("Menu");
    const auto add = [&](const std::string &text, const std::string &command) {
        s.addCommandItem("Menu", s.fitText("Menu", toWide(text)), 0, command);
    };
    if (!m_game) {
        add("Players: " + std::to_string(m_players), "players");
        add("Rivals: " + std::string(names[crew(1)]) + " + " + std::to_string(m_players - 2), "rivals");
        add("Cash buy-in: $" + std::to_string(m_buyIn), "buyin");
        add(m_titleCar < 0 ? "Available title: none" : "Title: " + m_carNames[m_titleCar], "title");
        add("Take a seat", "deal");
        add("Back to Career", "back");
    } else if (m_game->hand.finished()) {
        add("Set up next hand", "again");
        add("Back to Career", "back");
    } else if (m_game->hand.actor() == 0) {
        const auto &hand = m_game->hand;
        const auto &me = hand.seats()[0];
        if (m_betting) {
            add("Cash: $" + std::to_string(m_betCash), "amount");
            add(m_betTitle ? "Pink slip: $" + std::to_string(m_game->titleValue(0)) : "Add pink slip: no",
                "wager_title");
            s.setMenuItemEnabled("Menu", "wager_title", m_game->titleAvailable(0));
            const int total = m_betCash + (m_betTitle ? m_game->titleValue(0) : 0);
            const int paid = m_betTitle ? m_game->pledgeWager(m_betCash) : total;
            add(paid >= 0 ? "Bet $" + std::to_string(paid) : "Adjust cash to bet", "submit");
            auto candidate = hand;
            const bool valid =
                m_betTitle ? m_game->canPledge(m_betCash)
                           : (m_betCash > hand.toCall() ? candidate.act(poker::Action::Raise, me.round + m_betCash)
                                                        : m_betCash == std::min(hand.toCall(), me.chips) &&
                                                              candidate.act(poker::Action::CheckCall));
            s.setMenuItemEnabled("Menu", "submit", valid);
            add("Back to actions", "cancel_bet");
        } else {
            add("Fold", "fold");
            add(hand.toCall() ? "Call $" + std::to_string(std::min(hand.toCall(), me.chips)) : "Check", "call");
            add("Bet / raise...", "wager");
            add("Minimum raise: $" + std::to_string(hand.minimumRaiseTo()), "raise");
            add("All cash: $" + std::to_string(me.chips), "allin");
            s.setMenuItemEnabled("Menu", "raise", me.mayRaise && me.round + me.chips >= hand.minimumRaiseTo());
            s.setMenuItemEnabled("Menu", "allin", me.chips <= hand.toCall() || me.mayRaise);
            s.selectMenuItem("Menu", 1);
        }
    } else {
        add(std::string(names[crew(m_game->hand.actor())]) + " is thinking...", "waiting");
        s.setMenuItemEnabled("Menu", "waiting", false);
    }
}

void Gamble::texts(ui::Screen &s) {
    s.setText("STATUS", toWide(m_status));
    for (int seat = 1; seat <= 4; ++seat) {
        const std::string id = std::to_string(seat);
        const bool show = seat < m_players;
        s.setHidden("HEAD" + id, !show || seat != m_speaker);
        s.setHidden("FACE" + id, !show);
        s.setHidden("FRAME" + id, !show);
        s.setTexture("FACE" + id, kLeaderPictures[crew(seat)]);
        s.setHidden("NAME" + id, !show);
        s.setHidden("STACK" + id, !show);
        if (!show)
            continue;
        s.setText("NAME" + id, toWide(names[crew(seat)]));
        std::string status = "Ready";
        if (m_game) {
            const auto &p = m_game->hand.seats()[seat];
            status = p.folded ? "Folded" : "$" + std::to_string(p.chips);
            if (m_game->pledged(seat) && !m_game->hand.finished())
                status += " / title in";
            if (!p.folded && !p.chips)
                status = "All in";
            if (m_game->hand.finished() && !p.folded)
                status += " / " + poker::label(p.hole[0]) + " " + poker::label(p.hole[1]);
        }
        s.setText("STACK" + id, toWide(status));
        s.setColour(s.find("FRAME" + id), m_game && m_game->hand.actor() == seat ? 0xFF00B4FF : 0xFF6F468F);
    }
    s.setTexture("DEALER_FACE", kLeaderPictures[1 + (m_first + m_players - 1) % 8]);
    s.setText("DEALER_LABEL", L"Dealer");
    const char *street[] = {"Preflop", "Flop", "Turn", "River", "Result"};
    s.setText("POT", m_game ? toWide(std::string(street[static_cast<int>(m_game->hand.street())]) + " / Pot: $" +
                                     std::to_string(m_game->hand.pot()))
                            : L"Underground Hold'em");
    std::string info = "Titles are optional: wager a car during your turn at its cash value.";
    if (!m_game && m_titleCar >= 0)
        info = "Available: " + m_carNames[m_titleCar] + " / Sale value: $" + std::to_string(m_carValues[m_titleCar]);
    if (m_game) {
        const auto &me = m_game->hand.seats()[0];
        info = "Your stack: $" + std::to_string(me.chips) + " / " + poker::label(me.hole[0]) + " " +
               poker::label(me.hole[1]);
        if (me.folded)
            info += " / Folded";
        if (m_game->titles())
            info += m_game->pledged(0) ? " / Title wagered"
                                       : " / Title available: $" + std::to_string(m_game->titleValue(0));
        if (m_betting)
            info = "Cash + title = $" + std::to_string(m_betCash + (m_betTitle ? m_game->titleValue(0) : 0)) +
                   " / Min raise adds $" + std::to_string(m_game->hand.minimumRaiseTo() - me.round);
    }
    s.setText("INFO", toWide(info));
    if (m_titleCar < 0 || (m_game && m_game->hand.finished()))
        for (const char *id :
             {"TITLE_CAR_BACK", "TITLE_CAR_EDGE", "TITLE_CAR_VIEW", "TITLE_CAR_NAME", "TITLE_CAR_LOADING"})
            s.setHidden(id, true);
    const bool result = m_game && m_game->hand.finished();
    for (const char *id :
         {"RESULT_BACK", "RESULT_STRIPE", "RESULT_TRIBAL", "RESULT_TITLE", "RESULT_VALUE", "RESULT_DETAIL"})
        s.setHidden(id, !result);
    if (result) {
        const bool paid = m_game->settled();
        const int delta = m_game->equityDelta(0);
        s.setText("RESULT_TITLE", !paid       ? L"PAYMENT FAILED"
                                  : delta > 0 ? L"YOU WIN"
                                  : delta < 0 ? L"YOU LOSE"
                                              : L"EVEN HAND");
        s.setText("RESULT_VALUE",
                  !paid ? L"Career unchanged"
                        : toWide(std::string(delta < 0 ? "-$" : "+$") + std::to_string(std::abs(delta)) + " total"));
        const int cash = m_game->cashDelta();
        std::string detail = "Cash " + std::string(cash < 0 ? "-$" : "+$") + std::to_string(std::abs(cash));
        detail += " / Cars +" + std::to_string(m_game->carsWon());
        if (m_game->pledged(0))
            detail += m_game->carLost() ? " / Your car lost" : " / Your car returned";
        s.setText("RESULT_DETAIL", paid ? toWide(detail) : L"See the error below. No payout applied.");
        if (paid)
            s.setText("INFO", L"The total includes cash and car value. Uncalled bets are returned.");
        s.setColour(s.find("RESULT_STRIPE"), delta < 0 ? 0xFFFF4477 : 0xFF00B4FF);
    }
}

void Gamble::reaction(CareerHost &host, ui::Screen &s, int seat, const char *clip) {
    // Only one speaking clip at a time: no overlapping banter or four video decoders racing to play audio.
    for (int n = 1; n <= 4; ++n)
        s.setHidden("HEAD" + std::to_string(n), true);
    m_speaker = seat;
    if (seat <= 0 || seat >= m_players)
        return;
    const int element = s.find("HEAD" + std::to_string(seat));
    s.setHidden("HEAD" + std::to_string(seat), false);
    const std::string who = names[crew(seat)];
    s.playMovie(element, who + "\\" + who + clip, false);
    s.setMovieVolume(element, std::bit_cast<float>(host.settingsValue(save::Settings::kSpeechVolume)));
    s.holdMovie(element, false);
}

void Gamble::deal(CareerHost &host, ui::Screen &s) {
    auto *data = host.gameData();
    if (!data || !data->career)
        return;
    try {
        std::vector<int> rivals;
        for (int i = 1; i < m_players; ++i)
            rivals.push_back(crew(i));
        m_game = std::make_unique<poker::CareerHand>(*data->career, rivals, m_buyIn, m_dealer % m_players,
                                                     m_titleCar < 0 ? std::nullopt : std::optional<size_t>(m_titleCar),
                                                     m_random(), &data->catalogue);
        m_status = "Blinds posted. Preflop.";
        m_wait = 1.2f;
        reaction(host, s, 1, "_ACCEPT_BET1");
    } catch (const std::exception &e) {
        m_status = e.what();
    }
    menu(s);
    texts(s);
}

void Gamble::finish(CareerHost &host, ui::Screen &s) {
    if (!m_game || m_game->settled())
        return;
    auto *data = host.gameData();
    if (!data || !data->career)
        return;
    try {
        m_game->settle(*data->career);
        m_status = "Cash and cars settled. Car values are included in the total above.";
        int speaker = 1;
        for (int i = 1; i < m_players; ++i)
            if (m_game->equityDelta(i) > m_game->equityDelta(speaker))
                speaker = i;
        const int gain = m_game->equityDelta(speaker);
        const char *clip = gain == 0 ? "_OKAY1" : gain > 0 ? "_WINS_CASH1" : "_LOSES_CASH1";
        for (int i = 0; i < m_players; ++i)
            if (i != speaker && m_game->titleOwner(i) == speaker)
                clip = "_WIN_PINKSLIP1";
        reaction(host, s, speaker, clip);
        if (data->saving)
            host.request("autosave", SwitchMode::Push);
    } catch (const std::exception &e) {
        m_status = e.what();
    }
    menu(s);
    texts(s);
}

void Gamble::action(CareerHost &host, ui::Screen &s, poker::Action what, int amount) {
    const int actor = m_game->hand.actor();
    if (!m_game->hand.act(what, amount))
        return;
    m_betting = false;
    const std::string who = actor == 0 ? "You" : names[crew(actor)];
    m_status = who + (what == poker::Action::Fold    ? " folded."
                      : what == poker::Action::Raise ? " raised."
                                                     : " checked / called.");
    if (actor)
        reaction(host, s, actor, what == poker::Action::Fold ? "_DECLINE_BET1" : "_OKAY1");
    m_wait = 1.8f;
    if (m_game->hand.finished())
        finish(host, s);
    else {
        menu(s);
        texts(s);
    }
}

void Gamble::pledge(CareerHost &host, ui::Screen &s, int cash) {
    const int actor = m_game->hand.actor();
    if (!m_game->pledge(cash)) {
        m_status = "That title wager is not legal. Adjust the cash amount.";
        texts(s);
        return;
    }
    m_betting = false;
    m_status = std::string(actor == 0 ? "You" : names[crew(actor)]) + " wagered a title ($" +
               std::to_string(m_game->titleValue(actor)) + ") + $" + std::to_string(cash) + " cash.";
    if (actor)
        reaction(host, s, actor, "_ACCEPT_BET1");
    m_wait = 1.8f;
    if (m_game->hand.finished())
        finish(host, s);
    else {
        menu(s);
        texts(s);
    }
}

void Gamble::update(CareerHost &host, ui::Screen &s, uint32_t event) {
    m_clock += host.frameTime();
    framePreview(host, s);
    if (m_pledgeConfirm != -1) {
        const int answer = m_pledgeConfirm;
        m_pledgeConfirm = -1;
        if (answer == 1 && m_game)
            pledge(host, s, m_pendingCash);
        return;
    }
    if (m_confirm != -1) {
        const int answer = m_confirm;
        m_confirm = -1;
        if (answer == 1)
            deal(host, s);
        return;
    }
    if (m_game && !m_game->hand.finished() && m_game->hand.actor() != 0) {
        m_wait -= host.frameTime();
        if (m_wait <= 0) {
            const auto *clip = s.elementMovie(s.find("HEAD" + std::to_string(m_speaker)));
            if (clip && !clip->finished() && m_wait > -6.0f)
                return;
            const auto &h = m_game->hand;
            const auto &seat = h.seats()[h.actor()];
            int opponents = 0;
            for (const auto &p : h.seats())
                if (!p.folded)
                    ++opponents;
            const float chance = equity(seat.hole, h.board(), opponents - 1, m_random);
            const float price =
                static_cast<float>(std::min(h.toCall(), seat.chips)) / std::max(1, h.pot() + h.toCall());
            const int titleCash = std::min(seat.chips, std::max(0, h.toCall() - m_game->titleValue(h.actor())));
            if (m_game->titleAvailable(h.actor()) && chance > 0.62f &&
                (h.toCall() > seat.chips || m_random() % 5 == 0) && m_game->canPledge(titleCash))
                pledge(host, s, titleCash);
            else if (h.toCall() && chance + 0.04f < price && m_random() % 6)
                action(host, s, poker::Action::Fold);
            else if (seat.mayRaise && (chance > 0.6f || m_random() % 18 == 0) &&
                     seat.chips + seat.round >= h.minimumRaiseTo())
                action(host, s, poker::Action::Raise,
                       std::min(seat.chips + seat.round, h.minimumRaiseTo() + h.pot() / 2));
            else
                action(host, s, poker::Action::CheckCall);
        }
        return;
    }
    if (event == input::kBack && m_betting) {
        m_betting = false;
        menu(s);
        texts(s);
        return;
    }
    if (event == input::kBack) {
        if (m_game && !m_game->hand.finished()) {
            m_status = "Finish this hand before leaving. Choose Fold to give up your stakes.";
            texts(s);
        } else
            host.request("CMSMainMenu", SwitchMode::Replace);
        return;
    }
    if (event != input::kSelect)
        return;
    if (!s.menuItemEnabled("Menu", s.selectedMenuItem("Menu")))
        return;
    const auto cmd = s.menuItemCommand("Menu", s.selectedMenuItem("Menu"));
    host.sound("CLICK");
    if (cmd == "players")
        m_players = m_players == 5 ? 2 : m_players + 1;
    else if (cmd == "rivals") {
        m_first = (m_first + 1) % 8;
        reaction(host, s, 1, "_GREETING1");
    } else if (cmd == "buyin")
        m_buyIn = m_buyIn == 1000 ? 5000 : m_buyIn == 5000 ? 10000 : m_buyIn == 10000 ? 50000 : 1000;
    else if (cmd == "title")
        m_titleCar = m_titleCar + 1 >= static_cast<int>(m_garageSize) ? -1 : m_titleCar + 1;
    else if (cmd == "deal") {
        std::string warning = "Cash at risk: $" + std::to_string(m_buyIn) + ".";
        try {
            std::vector<int> rivals;
            for (int i = 1; i < m_players; ++i)
                rivals.push_back(crew(i));
            const auto *data = host.gameData();
            if (!data || !data->career)
                return;
            poker::CareerHand preview(*data->career, rivals, m_buyIn, m_dealer % m_players,
                                      m_titleCar < 0 ? std::nullopt : std::optional<size_t>(m_titleCar), m_random(),
                                      &data->catalogue);
            if (m_titleCar >= 0) {
                warning += "\nOptional title: " + m_carNames[m_titleCar] + " ($" +
                           std::to_string(preview.titleValue(0)) + ").";
                warning += "\nIt stays yours unless you wager it during a turn."
                           "\nTitles count as cash at their sale value."
                           "\nPrizes pay cars plus cash; split titles may pay cash.";
            }
        } catch (const std::exception &e) {
            m_status = e.what();
            texts(s);
            return;
        }
        warning += "\nDeal this hand?";
        host.message(MessageType::YesNo, toWide(warning), m_titleCar >= 0, &m_confirm, true);
        return;
    } else if (cmd == "back") {
        host.request("CMSMainMenu", SwitchMode::Replace);
        return;
    } else if (cmd == "again") {
        ++m_dealer;
        enter(host, s);
        return;
    } else if (m_game && !m_game->hand.finished()) {
        const auto &h = m_game->hand;
        const auto &p = h.seats()[0];
        if (cmd == "wager") {
            m_betting = true;
            m_betTitle = false;
            m_betCash = std::min(p.chips, std::max(1000, h.minimumRaiseTo() - p.round));
        } else if (cmd == "cancel_bet")
            m_betting = false;
        else if (cmd == "amount") {
            std::vector<int> amounts{0,
                                     100,
                                     500,
                                     1000,
                                     2500,
                                     5000,
                                     10000,
                                     p.chips,
                                     std::min(p.chips, h.toCall()),
                                     std::clamp(h.toCall() - m_game->titleValue(0), 0, p.chips),
                                     std::clamp(h.minimumRaiseTo() - p.round - m_game->titleValue(0), 0, p.chips),
                                     std::min(p.chips, h.minimumRaiseTo() - p.round)};
            std::erase_if(amounts, [&](int n) { return n < 0 || n > p.chips; });
            std::sort(amounts.begin(), amounts.end());
            amounts.erase(std::unique(amounts.begin(), amounts.end()), amounts.end());
            auto it = std::upper_bound(amounts.begin(), amounts.end(), m_betCash);
            m_betCash = it == amounts.end() ? amounts.front() : *it;
        } else if (cmd == "wager_title" && m_game->titleAvailable(0))
            m_betTitle = !m_betTitle;
        else if (cmd == "submit") {
            if (m_betTitle) {
                if (!m_game->canPledge(m_betCash))
                    return;
                m_pendingCash = m_betCash;
                host.message(MessageType::YesNo,
                             toWide("Wager " + m_carNames[m_titleCar] + " ($" + std::to_string(m_game->titleValue(0)) +
                                    ")\nplus $" + std::to_string(m_betCash) + " cash?\nTotal: $" +
                                    std::to_string(m_betCash + m_game->titleValue(0)) + ".\nWager: $" +
                                    std::to_string(m_game->pledgeWager(m_betCash)) +
                                    ". Excess becomes chips.\nYou can lose this car."),
                             true, &m_pledgeConfirm, true);
                return;
            }
            action(host, s, m_betCash > h.toCall() ? poker::Action::Raise : poker::Action::CheckCall,
                   p.round + m_betCash);
            return;
        } else if (cmd == "fold") {
            action(host, s, poker::Action::Fold);
            return;
        } else if (cmd == "call") {
            action(host, s, poker::Action::CheckCall);
            return;
        } else if (cmd == "raise") {
            action(host, s, poker::Action::Raise, h.minimumRaiseTo());
            return;
        } else if (cmd == "allin") {
            action(host, s, p.chips <= h.toCall() ? poker::Action::CheckCall : poker::Action::Raise, p.round + p.chips);
            return;
        }
        menu(s);
        texts(s);
        for (size_t i = 0; i < s.menuSize("Menu"); ++i)
            if (s.menuItemCommand("Menu", i) == cmd)
                s.selectMenuItem("Menu", i);
        return;
    }
    framePreview(host, s);
    menu(s);
    texts(s);
    for (size_t i = 0; i < s.menuSize("Menu"); ++i)
        if (s.menuItemCommand("Menu", i) == cmd)
            s.selectMenuItem("Menu", i);
}
} // namespace oj::frontend
