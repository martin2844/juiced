#pragma once

#include "Core/Gamble/CareerPoker.h"
#include "Game/Cars/CarView.h"
#include "Game/Frontend/Career/CareerHost.h"
#include <memory>
#include <random>

namespace oj::frontend {

class Gamble {
  public:
    explicit Gamble(uint32_t seed = std::random_device{}()) : m_random(seed) {}
    void enter(CareerHost &, ui::Screen &);
    void update(CareerHost &, ui::Screen &, uint32_t event);
    void leave(CareerHost &);
    void framePreview(CareerHost &, ui::Screen &);
    bool previewReady() const {
        return m_previewView && m_previewView->hasModel() && !m_previewView->busy();
    }
    const poker::CareerHand *game() const {
        return m_game.get();
    }
    int players() const {
        return m_players;
    }
    int crew(int seat) const {
        return 1 + (m_first + seat - 1) % 8;
    }
    float clock() const {
        return m_clock;
    }

  private:
    void menu(ui::Screen &);
    void texts(ui::Screen &);
    void deal(CareerHost &, ui::Screen &);
    void action(CareerHost &, ui::Screen &, poker::Action, int amount = 0);
    void pledge(CareerHost &, ui::Screen &, int cash);
    void reaction(CareerHost &, ui::Screen &, int seat, const char *clip);
    void finish(CareerHost &, ui::Screen &);
    int m_players = 5, m_first = 0, m_buyIn = 1000, m_dealer = 0;
    int m_confirm = -1, m_speaker = -1;
    int m_titleCar = -1;
    bool m_betting = false, m_betTitle = false;
    int m_betCash = 1000, m_pledgeConfirm = -1, m_pendingCash = 0;
    size_t m_garageSize = 0;
    std::vector<std::string> m_carNames;
    std::vector<int> m_carValues;
    std::unique_ptr<poker::CareerHand> m_game;
    Car m_previewCar; // Stable copy: settlement may move or remove the original garage entry.
    std::unique_ptr<scene::CarView> m_previewView;
    int m_previewIndex = -1;
    float m_wait = 0, m_clock = 0;
    std::string m_status;
    std::mt19937 m_random;
};

std::unique_ptr<ui::View3d> makePokerCardGallery(render::IRenderDevice &, const Gamble &);

// Built into the engine, so Gamble does not require modified retail UI archives.
ui::Window gambleWindow();
std::unique_ptr<ui::View3d> makePokerView(render::IRenderDevice &, const Gamble &);

} // namespace oj::frontend
