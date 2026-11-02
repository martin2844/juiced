// The race's drawing: the views of the players, the racers' labels and the grid's camera.

#include "Game/Race/TrackView/TrackView.h"

#include "Core/Base/Log.h"
#include "Core/Base/X87.h"
#include "Core/Career/Money.h"
#include "Core/Data/GameTexts.h"
#include "Core/Saves/GraphicsConfig.h"
#include "Game/Scene/FlameLights.h"
#include "Game/Scene/Headlights.h"
#include "Game/Scene/NearestFade.h"
#include "Render/Display.h"

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
    // The colour of a racer's nerves: the labels blink above 1.9, the "!" (drawn from 1.9 on) on every frame of the
    // parity (0x004BF9BC tests no nerves).
    const auto nervous = [&](float nerves, bool always) {
        float green = (1.0f - (nerves - 1.0f)) * 236.0f;  // [0x006FFB34]
        green = green < 0.0f ? 0.0f : (236.0f < green ? 236.0f : green);
        if ((always || nerves > 1.9f) && driversParity) green = 236.0f;  // [0x006FFB30]
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
            centred(L"!", x, y, withAlpha(nervous(r.nerves, true), alpha));  // [0x00769A58]
        }
        return;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const oj::Racer& r = session.racers[i];
        const RaceSlot* car = carOf(i);
        if (car == nullptr || car == playerCar) continue;
        const uint32_t colour = r.crew == 0 ? 0xFFFFFFFFu : nervous(r.nerves, false);
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
    // The fixed build's wide screens (Render/Display): the player's view widened by the picture's shape (Hor+, Vert+
    // on a picture narrower than 4:3); a split screen's views by the screen's, their 4:3 aspects the stock's.
    // The game options' field of view (GraphicsConfig::fov) widens it further, both ways.
    const oj::render::Display shape = oj::render::display(device.width(), device.height());
    const float view = oj::fovScale(oj::graphicsConfig().fov);
    player.camera.setWiden(shape.widen * view, shape.widenY * view);
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
    // --track-camera: the camera held at its place through the race (the scene seen from one place).
    if (!cameraOffset.empty() && !player.introPlaying) gridCamera(player);
    if (players.size() > 1) hideFollowed(player);
    const float w = static_cast<float>(device.width());
    const float h = static_cast<float>(device.height());
    const auto x0 = static_cast<uint32_t>(player.viewLeft * w);
    const auto y0 = static_cast<uint32_t>(player.viewTop * h);
    const auto x1 = static_cast<uint32_t>(player.viewRight * w);
    const auto y1 = static_cast<uint32_t>(player.viewBottom * h);
    device.profileMark("car reflections cube");
    scene->drawCarCube(player.camera, seconds);
    device.profileMark("race: view");
    // WORLD LIGHT HI and ULTRA: the view's depth for its ambient occlusion (TrackScene::draw multiplies it in).
    oj::render::TextureId occlusionDepth = 0;
    if (ambientOcclusion) {
        if (!occlusion) occlusion = std::make_unique<oj::scene::AmbientOcclusion>(device);
        device.profileMark("occlusion depth");
        occlusionDepth = occlusion->drawDepth(*scene, player.camera, x1 - x0, y1 - y0,
                                              sunShadows && oj::scene::lightSet(lighting).roadLights);
        device.profileMark("race: view");
    }
    scene->setAmbientOcclusion(occlusionDepth, oj::scene::occlusionView(player.camera), occlusionOn);
    // WORLD LIGHT ULTRA in the light case's set (a dry day's): the sun's cascades about the view
    // (Game/Scene/SunShadows) in place of the light tiles' sun; in the other cases, or when they cannot be drawn, the
    // tiles (b9 off).
    oj::render::Renderer3d& renderer = scene->engine().renderer();
    const oj::scene::LightSet& set = oj::scene::lightSet(lighting);
    sunShadowsDrawn = false;
    if (sunShadows && set.sunCascades) {
        if (!sunCascades) sunCascades = std::make_unique<oj::scene::SunShadows>(device);
        device.profileMark("sun shadow maps");
        const auto casters = casterView(player, static_cast<float>(y1 - y0));
        if (casterPixels > 0.0f) scene->setCasterView(&casters);
        sunShadowsDrawn =
            sunCascades->draw(*scene, player.camera, renderer.lightDirection, sunShadowsSet && players.size() == 1);
        scene->setCasterView(nullptr);
        device.profileMark("race: view");
    }
    if (!sunShadowsDrawn) {
        const float off = 0.0f;
        renderer.programs().setShared("SunShadowsOn", &off, sizeof off);
    }
    if (!sunShadowsDrawn && sunShadowsSet) {
        const int32_t off = 0;
        device.setPixelShaderConstantB(oj::scene::kSunBool, &off, 1);
        if (sunCascades) sunCascades->unbind();
    }
    sunShadowsSet = sunShadowsDrawn;
    // WORLD LIGHT ULTRA in the set's case (a wet night's): the lights reflected in the road, from the same view depth
    // (TrackScene::draw adds them after the lens flares).
    const oj::render::LightReflectionView reflection = oj::scene::reflectionView(player.camera, 1.0f);
    scene->setLightReflections(sunShadows && set.roadLights && occlusionDepth != 0 ? &reflection : nullptr);
    // And the light in the haze, as thick as the set's.
    const oj::render::VolumetricLightView haze = oj::scene::volumetricView(player.camera, hazeLights, set.haze);
    scene->setVolumetricLight(!hazeLights.empty() && occlusionDepth != 0 ? &haze : nullptr);
    // And the cars' flames' light on the view's surfaces.
    const oj::render::PointLightView flames = oj::scene::flameView(player.camera, flameLights);
    scene->setPointLights(!flameLights.empty() && occlusionDepth != 0 ? &flames : nullptr);
    // And the cars' shadows from the street lamps drawn this frame (TrackScene::draw multiplies them in).
    if (lampShadowsDrawn && occlusionDepth != 0) {
        const oj::render::LampShadowView lamps = lampShadows->view(player.camera);
        scene->setLampShadows(&lamps, lampShadows->maps());
    } else {
        scene->setLampShadows(nullptr, {});
    }
    player.postView.clear = player.viewClear;
    oj::scene::PostEffects& post = scene->engine().postEffects();
    post.begin(player.camera, player.postView, x0, y0, x1 - x0, y1 - y0, player.director.blendOn);
    scene->draw(player.camera, seconds);
    device.profileMark("post effects");
    const auto smears = [&](float sx0, float sy0, float sx1, float sy1) {
        if (oj::scene::ParticleSystem* system = scene->particles()) {
            system->drawSmears(sx0, sy0, sx1, sy1, player.camera);
        }
    };
    post.end(player.camera, player.postView, player.director.blendAmount, smears);
    device.profileMark("race: view");
}

void TrackView::gridCamera(RacePlayer& player) {
    const oj::Spl& lines = scene->lines();
    const oj::Spl::GridPoint place = lines.grid.empty() ? oj::Spl::GridPoint{} : lines.grid[0];
    const float fx = std::sin(place.heading), fz = std::cos(place.heading);
    float x = place.x - fx * 900.0f, y = place.y + 300.0f, z = place.z - fz * 900.0f;
    float dx = place.x + fx * 2000.0f - x, dy = place.y + 100.0f - y, dz = place.z + fz * 2000.0f - z;
    float right = 0.0f, up = 0.0f, back = 0.0f, aimRight = 0.0f, aimForward = 0.0f;
    const int read = cameraOffset.empty() ? 0
                                          : sscanf_s(cameraOffset.c_str(), "%f,%f,%f,%f,%f", &right, &up, &back,
                                                     &aimRight, &aimForward);
    if (read >= 3) {
        // Right is (cos h, 0, -sin h) beside forward (sin h, 0, cos h).
        x = place.x + fz * right - fx * back;
        y = place.y + up;
        z = place.z - fx * right - fz * back;
        dx = place.x + fz * aimRight + fx * aimForward - x;
        dy = place.y + 100.0f - y;
        dz = place.z - fx * aimRight + fz * aimForward - z;
    }
    player.camera.setFov(0.35f * 3.14159265f, 0.0f, false);
    player.camera.setFar(2000000.0f);
    player.camera.resetMatrices();
    player.camera.setPosition(x, y, z);
    oj::rotateY(player.camera.local, -std::atan2(dx, dz));
    oj::rotateX(player.camera.local, -std::atan2(-dy, std::sqrt(dz * dz + dx * dx)));
    player.camera.invalidate();
}

oj::scene::TrackScene::CasterView TrackView::casterView(RacePlayer& player, float height) const {
    oj::scene::TrackScene::CasterView view;
    const oj::Mat34& w = player.camera.world();
    view.eye = {w[3], w[7], w[11], 0.0f};
    // The projection's y scale: 1 over the tangent of half the angle up.
    view.pixelsPerUnit = height * 0.5f * player.camera.projection()[5];
    view.pixels = casterPixels;
    return view;
}

void TrackView::draw(oj::render::IRenderDevice& device, float seconds, float dt) {
    if (racing) dt = dt * drawScale;
    device.profileMark("race: lights");
    // The fixed build's headlights (JuicedConfigExt's Headlights): the lights of the cars whose headlights are shown,
    // the nearest the first player's camera first (six at most), for the frame's night shaders; turned off, none.
    // With HEADLIGHTS ULTRA their shadow maps (Game/Scene/HeadlightShadows: the first player's car's own, the cars
    // near it, the others) and the surfaces' own light taken where they shine.
    std::vector<oj::scene::CarHeadlights> lights;
    std::vector<oj::scene::HeadlightShadows::Light> shadowLights;
    if (headlights > 0.0f) {
        struct Lit {
            float distance;
            size_t car;
            oj::scene::CarHeadlights lights;
        };
        std::vector<Lit> lit;
        const RaceSlot* own = players.empty() ? nullptr : players.front()->slot;
        const oj::Mat34* eye = players.empty() ? nullptr : &players.front()->camera.world();
        oj::Mat34 ownRoot{};
        bool ownLit = false;
        for (size_t i = 0; i < cars.size(); ++i) {
            const RaceSlot& s = *cars[i];
            oj::scene::CarFlare* right = s.headFlares[0];
            oj::scene::CarFlare* left = s.headFlares[1];
            if (right == nullptr || left == nullptr || right->node == nullptr || left->node == nullptr) continue;
            if (!right->on && !left->on) continue;
            oj::render::Node* root = right->node;
            while (root->parent() != nullptr) root = root->parent();
            const oj::Mat34& r = root->world();
            const float dx = eye != nullptr ? r[3] - (*eye)[3] : 0.0f;
            const float dy = eye != nullptr ? r[7] - (*eye)[7] : 0.0f;
            const float dz = eye != nullptr ? r[11] - (*eye)[11] : 0.0f;
            // A tunnel's light by a dry day coming on or going out (Game/Scene/TunnelLights).
            const float level = s.lightsOn ? 1.0f : s.tunnelLights.level;
            lit.push_back(
                {dx * dx + dy * dy + dz * dz, i, {right->node->world(), left->node->world(), r, true, level}});
            if (&s == own) {
                ownRoot = r;
                ownLit = true;
            }
        }
        std::stable_sort(lit.begin(), lit.end(), [](const Lit& a, const Lit& b) { return a.distance < b.distance; });
        if (lit.size() > static_cast<size_t>(oj::scene::kHeadlights)) lit.resize(oj::scene::kHeadlights);
        for (const Lit& l : lit) {
            lights.push_back(l.lights);
            if (!headlightShadows) continue;
            const RaceSlot& s = *cars[l.car];
            const oj::Mat34& r = l.lights.root;
            const float dx = r[3] - ownRoot[3], dy = r[7] - ownRoot[7], dz = r[11] - ownRoot[11];
            const float away = ownLit ? std::sqrt(dx * dx + dy * dy + dz * dz) : oj::scene::kNearShadows + 1.0f;
            shadowLights.push_back({l.lights, s.model, static_cast<uint32_t>(l.car),
                                    oj::scene::shadowTier(&s == own, away)});
        }
    }
    // The race's light case (Game/Scene/LightSets; a dry day's with lights shown, in a tunnel, the tunnel's): its set
    // drawn, its pixel shaders' variants bound.
    lighting = scene ? oj::scene::lightCase(scene->night(), scene->wet(), !lights.empty()) : oj::render::kBaseLightCase;
    const oj::scene::LightSet& set = oj::scene::lightSet(lighting);
    device.setLightCase(lighting);
    // SKY HI and ULTRA: the procedural clouds by the set's look (Game/Scene/Clouds).
    if (scene) scene->setClouds(sky != 0 ? &set : nullptr, sky == 2);
    // SHADOWS HI and ULTRA in the set's cases (by night): the street lamps nearest a point ahead of the first player's
    // camera draw the cars into their maps, each fading in and out (w its shadow's share). The row's own level, the
    // user 2026-10-29; it hung off WORLD LIGHT ULTRA before.
    lampShadowsDrawn = false;
    if (shadowsLevel >= 2 && ambientOcclusion && scene && set.lampShadows && !players.empty()) {
        std::vector<oj::Vec4> lamps;
        const oj::Vec4 centre = oj::scene::lampShadowCentre(players.front()->camera.world());
        const auto found = scene->lampsWithin(centre, oj::scene::kLampShadowReach, oj::scene::kLampShadows + 1);
        const auto weights = oj::scene::rankFade(oj::scene::TrackScene::distances(found), oj::scene::kLampShadows,
                                                 oj::scene::kLampShadowBand);
        for (size_t i = 0; i < weights.size(); ++i) {
            const float weight = weights[i] * oj::scene::distanceFade(found[i].distance, oj::scene::kLampShadowFull,
                                                                      oj::scene::kLampShadowReach);
            if (weight <= 0.0f) continue;
            const oj::Mat34& w = found[i].node->world();
            lamps.push_back({w[3], w[7], w[11], weight});
        }
        if (!lamps.empty()) {
            if (!lampShadows) lampShadows = std::make_unique<oj::scene::LampShadows>(device);
            device.profileMark("lamp shadow maps");
            lampShadowsDrawn = lampShadows->draw(*scene, lamps) > 0;
            device.profileMark("race: lights");
        }
    }
    // HEADLIGHTS ULTRA: the shadow maps first, their texel widths in the lights' constants.
    std::array<float, oj::scene::kHeadlights> texels{};
    const bool shadowed = headlightShadows && scene && !shadowLights.empty();
    if (shadowed) {
        if (!shadows) shadows = std::make_unique<oj::scene::HeadlightShadows>(device);
        device.profileMark("headlight shadow maps");
        oj::scene::TrackScene::CasterView view;
        if (casterPixels > 0.0f && !players.empty()) {
            RacePlayer& first = *players.front();
            view = casterView(first, (first.viewBottom - first.viewTop) * static_cast<float>(device.height()));
            scene->setCasterView(&view);
        }
        texels = shadows->draw(*scene, shadowLights, ++shadowFrame);
        scene->setCasterView(nullptr);
        device.profileMark("race: lights");
    } else if (shadows) {
        shadows->unbind();
    }
    // The set's share of their strength (by day in the rain weaker in the daylight).
    const float strength = headlights * set.headlights;
    // WORLD LIGHT ULTRA in the set's cases (by night): the light in the haze of the street lamps ahead, each fading in
    // and out, and of the shown headlights.
    hazeLights.clear();
    if (sunShadows && ambientOcclusion && scene && set.hazeLight && !players.empty()) {
        const oj::Mat34& eye = players.front()->camera.world();
        const auto found = scene->lampsWithin(oj::scene::hazeCentre(eye), oj::scene::kHazeLampsReach,
                                              oj::scene::kHazeLamps + 1);
        const auto weights = oj::scene::rankFade(oj::scene::TrackScene::distances(found), oj::scene::kHazeLamps,
                                                 oj::scene::kHazeLampsBand);
        for (size_t i = 0; i < weights.size(); ++i) {
            const float weight = weights[i] * oj::scene::distanceFade(found[i].distance, oj::scene::kHazeLampsFull,
                                                                      oj::scene::kHazeLampsReach);
            if (weight <= 0.0f) continue;
            const oj::Mat34& w = found[i].node->world();
            const oj::Vec4 lamp{w[3], w[7], w[11], 0.0f};
            hazeLights.push_back(oj::scene::lampCone(lamp, weight, oj::scene::hazeSteps(eye, lamp)));
        }
        for (size_t i = 0; i < lights.size() && i < oj::scene::kHazeBeams; ++i) {
            hazeLights.push_back(oj::scene::headlightBeam(oj::scene::headlightFrame(lights[i]), strength));
        }
    }
    // WORLD LIGHT HI and ULTRA: the cars' shown flames (the nitro's, the backfires') as lights by the set's share, in
    // the flames' own colour (the car's neon mixed into the warm orange: flameLightColour).
    flameLights.clear();
    if (ambientOcclusion && scene) {
        for (size_t i = 0; i < cars.size(); ++i) {
            const RaceSlot& s = *cars[i];
            const oj::Car& record = s.car.car != nullptr ? *s.car.car : s.record;
            const std::array<float, 3> colour = oj::scene::flameLightColour(record, oj::scene::kFlameColourShare);
            for (size_t f = 0; f < 2; ++f) {
                oj::render::ModelNode* node = s.flameNodes[f];
                if (!s.car.effects.flameShown[f] || node == nullptr || (node->flags & 4u) != 0) continue;
                const float level = std::clamp(s.car.effects.flameEmit[f], 0.4f, 1.0f);
                flameLights.push_back(oj::scene::flameLight(node->world(), colour, set.flames * level, seconds,
                                                            static_cast<uint32_t>(2 * i + f)));
            }
        }
    }
    headlightsSet = oj::scene::setHeadlights(device, lights, strength, headlightsSet, shadowed ? &texels : nullptr);
    // WORLD LIGHT HI and ULTRA: the cars' paint lit by the street lamps (TrackScene::setCarLamps; the user, 2026-10-29:
    // this is light, not reflections) and by the headlights (their row's own, HEADLIGHTS HI and ULTRA).
    if (scene) {
        oj::scene::setCarHeadlights(scene->engine().renderer().programs(), lights, strength,
                                    oj::carLampsLit(oj::graphicsConfig()));
    }
    // The world's clock stopped and started again with the race's pause (0x004263E0 and the resumes that clear it
    // inline: 0x00406AB0, 0x00424270, 0x00426B80, 0x00426C40, 0x00426CF0, 0x00426DF0, 0x0044ED90, 0x00455A90).
    if (scene) scene->clock().stop(paused, seconds);
    // With the fixed build's headlights (HI and ULTRA) the stock's projected beams (headlight_b) are not drawn: the
    // headlights light the road themselves, and the beams know no shadow (the user, 2026-10-28).
    std::vector<std::pair<oj::scene::Projector*, bool>> beams;
    if (headlights > 0.0f) {
        for (const std::unique_ptr<RaceSlot>& s : cars) {
            if (s->headBeam == nullptr) continue;
            beams.emplace_back(s->headBeam, s->headBeam->on);
            s->headBeam->on = false;
        }
    }
    for (std::unique_ptr<RacePlayer>& p : players) drawView(device, *p, seconds, dt);
    for (const auto& [beam, on] : beams) beam->on = on;
    // 0x005A9D20: with a message up the windows under it draw only their FRAME_ elements.
    // 0x00456BA0: the network's overlay, from the race front end's frame; off the network the racers' labels
    // (0x004BF320).
    device.profileMark("HUD and menus");
    drawNetOverlay();
    drawRacerLabels();
    frontEnd.draw(device, seconds, dt, !netFrameEnded);
}

}  // namespace app
