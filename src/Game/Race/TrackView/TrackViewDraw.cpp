// The race's drawing: the views of the players, the racers' labels and the grid's camera.

#include "Game/Race/TrackView/TrackView.h"

#include "Core/Base/Log.h"
#include "Core/Base/X87.h"
#include "Core/Career/Money.h"
#include "Core/Data/GameTexts.h"

#include <windows.h>

#include <algorithm>
#include <cmath>

namespace app {

void TrackView::drawRacerLabels() {
    if (net != nullptr || frontEnd.hudDevice == nullptr || frontEnd.hudLanguage == nullptr ||
        frontEnd.hudTexts == nullptr)
        return;
    if (rules.mode == RaceMode::Attract || rules.replayRace || rules.mode == RaceMode::SplitScreen) return;
    if (gameData == nullptr || !gameData->career) return;
    const oj::Career& career = *gameData->career;
    const oj::RaceSession& session = career.session;
    if (!netOverlay) netOverlay = std::make_unique<oj::ui::Screen>(*frontEnd.hudDevice, *frontEnd.hudLanguage);
    oj::ui::Screen& s = *netOverlay;
    const float w = static_cast<float>(frontEnd.hudDevice->width());
    const float h = static_cast<float>(frontEnd.hudDevice->height());
    const char* font = "goodtime11";
    const uint32_t count = std::min<uint32_t>(session.count, static_cast<uint32_t>(session.racers.size()));
    // The racer's car (+0x2C: the race's car of that record).
    const auto carOf = [&](uint32_t i) -> const RaceSlot* {
        for (const std::unique_ptr<RaceSlot>& c : cars) {
            if (c->car.record == static_cast<int32_t>(i)) return c.get();
        }
        return nullptr;
    };
    const RaceSlot* playerCar = nullptr;
    for (uint32_t i = 0; i < count; ++i) {
        if (session.racers[i].member == 0 && session.racers[i].crew == 0) {
            playerCar = carOf(i);
            break;
        }
    }
    // The colour of a racer's nerves.
    const auto nervous = [&](float nerves) {
        float green = (1.0f - (nerves - 1.0f)) * 236.0f;  // [0x006FFB34]
        green = green < 0.0f ? 0.0f : (236.0f < green ? 236.0f : green);
        if (nerves > 1.9f && driversParity) green = 236.0f;  // [0x006FFB30]
        return 0xFFE60000u | ((static_cast<uint32_t>(oj::x87::nearest(green)) & 0xFF) << 8);
    };
    // A point of the car's centre so high above it through the camera: false behind the camera (alpha 0 there).
    const auto project = [&](const RaceSlot& slot, float up, float& x, float& y, float& depth) {
        const oj::Vec4& p = slot.body->position;
        const oj::Mat44& m = first().camera.worldToClip();
        const float px = p[0] * 100.0f, py = p[1] * 100.0f + up, pz = p[2] * 100.0f;
        const float cx = ((m[0] * px + m[2] * pz) + m[1] * py) + m[3];
        const float cy = ((m[4] * px + m[6] * pz) + m[5] * py) + m[7];
        depth = ((m[12] * px + m[14] * pz) + m[13] * py) + m[15];
        if (!(depth > 0.0f)) return false;
        x = (cx / depth * 0.5f + 0.5f) * w;
        y = (0.5f - cy / depth * 0.5f) * h;
        return true;
    };
    const auto withAlpha = [](uint32_t colour, float alpha) {
        return (colour & 0x00FFFFFFu) | ((static_cast<uint32_t>(oj::x87::chop(alpha * 255.0f)) & 0xFF) << 24);
    };
    const auto centred = [&](const std::wstring& text, float x, float y, uint32_t colour) {
        const float width = static_cast<float>(static_cast<uint32_t>(s.freeTextWidth(font, text)));
        s.drawFreeText(font, text, x - width * 0.5f, y, colour);
    };
    if (namesTime() < 0.0f) {
        // 0x004BF8B0: each racer with nerves of 1.9 or more ([0x0079E844] counts the racers 0 .. 7 round, read
        // by nothing) whose car is not the player's: "!" in its nerves' colour centred 1.2 m above the car's
        // centre (alpha 0, then 1 - depth / 100 m in front within 100 m).
        for (uint32_t i = 0; i < count; ++i) {
            const oj::Racer& r = session.racers[i];
            if (r.nerves < 1.9f) continue;
            const RaceSlot* car = carOf(i);
            if (car == nullptr || car == playerCar) continue;
            float x = 0.0f, y = 0.0f, depth = 0.0f;
            if (!project(*car, 120.0f, x, y, depth)) continue;  // [0x007255E4]
            const float alpha = depth < 10000.0f ? 1.0f - depth / 10000.0f : 0.0f;  // [0x007255E8]
            centred(L"!", x, y, withAlpha(nervous(r.nerves), alpha));  // [0x00769A58]
        }
        return;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const oj::Racer& r = session.racers[i];
        const RaceSlot* car = carOf(i);
        if (car == nullptr || car == playerCar) continue;
        const uint32_t colour = r.crew == 0 ? 0xFFFFFFFFu : nervous(r.nerves);
        float x = 0.0f, y = 0.0f, depth = 0.0f;
        if (!project(*car, 135.0f, x, y, depth)) continue;  // [0x007255EC]
        const uint32_t c = withAlpha(colour, depth < 15000.0f ? 1.0f - depth / 15000.0f : 0.0f);  // [0x007255F0]
        const auto crew = static_cast<size_t>(car->car.crew);
        const auto line = [&](size_t index) {
            return index < frontEnd.hudTexts->size() ? frontEnd.hudTexts->at(index) : std::wstring();
        };
        centred(line(108 + crew), x, y, c);
        y += 15.0f;  // [0x006EDB50]
        std::wstring member;
        if (crew < career.crews.size() && r.member >= 0 && r.member < 4) {
            for (char ch : career.crews[crew].memberShortNames[static_cast<size_t>(r.member)]) {
                if (ch == 0) break;
                member.push_back(static_cast<wchar_t>(static_cast<unsigned char>(ch)));
            }
        }
        centred(member, x, y, c);
        const int language = frontEnd.hudLanguage->index;
        if (crew < session.odds.size() && session.odds[crew].staked != 0) {
            y -= 30.0f;  // [0x006EDAD8]
            const std::string money = oj::moneyText(session.odds[crew].staked, language);
            centred(line(oj::kTextBetWord) + L" " + std::wstring(money.begin(), money.end()), x, y, c);
        }
        const oj::CareerBets& bets = career.bets;
        if (r.member == 0 && bets.placed != 0 && r.crew == bets.crew && oj::x87::nearest(bets.amount) > 0) {
            y -= 30.0f;
            const std::string money = oj::moneyText(oj::x87::nearest(bets.amount), language);
            centred(std::wstring(money.begin(), money.end()), x, y, c);
        }
    }
}

void TrackView::drawView(oj::render::IRenderDevice& device, RacePlayer& player, float seconds, float dt) {
    if (player.introPlaying) {
        // The fly-by's first frame holds the count at 99 (it runs down from there while the fly-by plays).
        if (!player.introStarted && race) race->count = 99.0f;
        player.introStarted = true;
        player.introPlaying = player.intro.update(paused ? 0.0f : dt, player.camera, wideScreen, first().director);
        if (!player.introPlaying) {
            oj::log::info("race: the fly-by ended at {:.2f} s", seconds);
            gridCamera(player);
            if (race) countAndSweep(player);
        }
    }
    if (players.size() > 1) hideFollowed(player);
    const float w = static_cast<float>(device.width());
    const float h = static_cast<float>(device.height());
    const auto x0 = static_cast<uint32_t>(player.viewLeft * w);
    const auto y0 = static_cast<uint32_t>(player.viewTop * h);
    const auto x1 = static_cast<uint32_t>(player.viewRight * w);
    const auto y1 = static_cast<uint32_t>(player.viewBottom * h);
    scene->drawCarCube(player.camera, seconds);
    player.postView.clear = player.viewClear;
    // The fixed build's glow at 0.7 where the headlights are on (as the cars' lightsOn: night or wet).
    const oj::render::AtmosphereSet* set = scene->atmosphere();
    player.postView.headlights =
        set != nullptr && set->current != nullptr && (set->current->night != 0 || set->current->wet != 0);
    oj::scene::PostEffects& post = scene->engine().postEffects();
    post.begin(player.camera, player.postView, x0, y0, x1 - x0, y1 - y0, player.director.blendOn,
               alwaysDepthOfField);
    scene->draw(player.camera, seconds);
    const auto smears = [&](float sx0, float sy0, float sx1, float sy1) {
        if (oj::scene::ParticleSystem* system = scene->particles()) {
            system->drawSmears(sx0, sy0, sx1, sy1, player.camera);
        }
    };
    post.end(player.camera, player.postView, player.director.blendAmount, alwaysDepthOfField, smears);
}

void TrackView::gridCamera(RacePlayer& player) {
    const oj::Spl& lines = scene->lines();
    const oj::Spl::GridPoint place = lines.grid.empty() ? oj::Spl::GridPoint{} : lines.grid[0];
    const float fx = std::sin(place.heading), fz = std::cos(place.heading);
    float x = place.x - fx * 900.0f, y = place.y + 300.0f, z = place.z - fz * 900.0f;
    float dx = place.x + fx * 2000.0f - x, dy = place.y + 100.0f - y, dz = place.z + fz * 2000.0f - z;
    float right = 0.0f, up = 0.0f, back = 0.0f;
    if (!cameraOffset.empty() && sscanf_s(cameraOffset.c_str(), "%f,%f,%f", &right, &up, &back) == 3) {
        // Right is (cos h, 0, -sin h) beside forward (sin h, 0, cos h).
        x = place.x + fz * right - fx * back;
        y = place.y + up;
        z = place.z - fx * right - fz * back;
        dx = place.x - x;
        dy = place.y + 100.0f - y;
        dz = place.z - z;
    }
    player.camera.setFov(0.35f * 3.14159265f, 0.0f, false);
    player.camera.setFar(2000000.0f);
    player.camera.resetMatrices();
    player.camera.setPosition(x, y, z);
    oj::rotateY(player.camera.local, -std::atan2(dx, dz));
    oj::rotateX(player.camera.local, -std::atan2(-dy, std::sqrt(dz * dz + dx * dx)));
    player.camera.invalidate();
}

void TrackView::draw(oj::render::IRenderDevice& device, float seconds, float dt) {
    if (racing) dt = dt * drawScale;
    // The world's clock stopped and started again with the race's pause (0x004263E0 and the resumes that clear it
    // inline: 0x00406AB0, 0x00424270, 0x00426B80, 0x00426C40, 0x00426CF0, 0x00426DF0, 0x0044ED90, 0x00455A90).
    if (scene) scene->clock().stop(paused, seconds);
    for (std::unique_ptr<RacePlayer>& p : players) drawView(device, *p, seconds, dt);
    // 0x005A9D20: with a message up the windows under it draw only their FRAME_ elements.
    // 0x00456BA0: the network's overlay, from the race front end's frame; off the network the racers' labels
    // (0x004BF320).
    drawNetOverlay();
    drawRacerLabels();
    frontEnd.draw(device, seconds, dt, !netFrameEnded);
}

}  // namespace app
