// The paint windows: colourmenu, colourchoose, colourfocus and wheelcolourmenu.

#include "Game/Frontend/Garage/Customise/Paint.h"
#ifdef OJ_MACOS
#include "Mods/Creative.h"
#include "Mods/Runtime.h"
#include "Mods/Packages.h"
#include "Core/Base/Text.h"
#endif

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

namespace {

// A colour's red, green and blue put in another (its alpha kept).
uint32_t withRgb(uint32_t argb, uint32_t rgb) {
    return (argb & 0xFF000000u) | (rgb & 0x00FFFFFFu);
}

}  // namespace

// 0x0043D270: the base enter; the camera to the garage's place; without a wheel on the car the wheel item taken out
// (the fourth highlighted then the first); colourmenu's metallic and pearl colours and amounts kept; another car than
// +0x24 highlights the first item; the answer -1, no questions; the elements holding BUY_ hidden; +0x20 and +0x0C 1.
void ColourMenu::enter(GarageHost& host, ui::Screen& s) {
    Garage& garage = host.garage();
    host.baseEnter(s, "colourmenu");
    if (garage.view) garage.view->cameraTo(kCameraGarage);
    Car* car = garage.car(host);
    if (car == nullptr) return;
    if (s.find("Menu") >= 0 && car->mods[kWheel] == kNoMod) {
        s.removeMenuItem("Menu", "wheel");
        if (s.selectedMenuItem("Menu") == 3) s.selectMenuItem("Menu", 0);
    }
    garage.paintMetallic = car->metallic;
    garage.paintPearl = car->pearl;
    garage.paintMetallicAmount = car->metallicAmount;
    garage.paintPearlAmount = car->pearlAmount;
    if (m_car != car) {
        if (s.find("Menu") >= 0 && s.menuSize("Menu") != 0) s.selectMenuItem("Menu", 0);
        m_car = car;
    }
    m_answer = -1;
    m_askedBuy = 0;
    m_askedLose = 0;
    s.hideContaining("BUY_");
    m_showBuy = 1;
    m_first = 1;
}

// 0x0043D390: [0x0075F108] 2. Changes to pay (once, +0x20): the price theirs, the price and its box in, the BUY_
// elements in, BUY_TEST "BUY PAINT" (1930). The buying asked (+0x18): a "yes" pays them, notes them on the car and the
// profile, shows the money, clears the garage's texts, goes to custmenu and ends: the changes 0, a metallic paint sets
// the car's +0x50 1, a pearl its +0x54 1; else the question cleared. The losing asked (+0x1C): a "yes" puts the paint
// custmenu kept back on the car and goes to custmenu, the changes 0; else the question cleared. LEFT, RIGHT or the
// first frame: LOCKED hidden and by the highlighted item the price: the changes + 1000 for an uncharged base, + 3000
// for an uncharged metallic (locked under the car's level 0, [profile + 0x474 + model]), + 5000 for an uncharged pearl
// (locked under 1), 1000 for the wheels. SELECT: the money under the price "INSUFFICIENT CASH" (type 1) and NO; else
// CLICK and base colourchoose (paint 0), metal (paint 1) and pearl (paint 2) when not locked (else NO), wheel
// wheelcolourmenu. PGDN with changes: CLICK and "CONFIRM NEW CAR COLOR" (1786), "\n ", the price, " \n" and "ARE YOU
// SURE?" (type 3, on NO), the buying asked. BACK (CLICK): with changes "LOSE PAINT CHANGES" (1931), "\n" and "ARE YOU
// SURE?", the losing asked; else custmenu. The chassis lowered.
void ColourMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr || !data->career) return;
    Career& career = *data->career;
    if (garage.paintPending > 0 && m_showBuy != 0) {
        garage.price(host, garage.paintPending);
        garage.priceShown(host, true);
        s.showContaining("BUY_", true);
        m_showBuy = 0;
        if (s.find("BUY_TEST") >= 0) {
#ifdef OJ_MACOS
            s.setText("BUY_TEST", toWide(mods::runtime::text("paint_buy_label", {}, mods::packages::available() ? "" : "[0] ")) + texts.at(kTextBuyPaint));
#else
            s.setText("BUY_TEST", texts.at(kTextBuyPaint));
#endif
        }
    }
    if (m_askedBuy != 0) {
        if (m_answer == 1) {
            pay(career, garage.paintPending);
            const auto cost = static_cast<float>(garage.paintPending);
            car->spent = std::bit_cast<uint32_t>(std::bit_cast<float>(car->spent) + cost);
            CareerProfile& p = career.profile;
            const auto spent = static_cast<float>(static_cast<int32_t>(p.moneySpent));
            p.moneySpent = static_cast<uint32_t>(x87::chop(spent + cost));
            garage.cash(host);
            garage.clear(host);
            host.request("custmenu", SwitchMode::Swap);
            garage.paintPending = 0;
            if (garage.paintMode == PaintMode::Metallic) {
                car->metallicBought = 1;
            } else if (garage.paintMode == PaintMode::Pearl) {
                car->pearlBought = 1;
            }
            return;
        }
        m_askedBuy = 0;
    }
    if (m_askedLose != 0) {
        if (m_answer == 1) {
            car->base = garage.paintKept[0];
            car->metallic = garage.paintKept[1];
            car->pearl = garage.paintKept[2];
            garage.repaint(host);
            m_answer = -1;
            m_askedBuy = 0;
            garage.paintPending = 0;
            host.request("custmenu", SwitchMode::Swap);
        } else {
            m_askedLose = 0;
        }
    }
    const auto model = static_cast<size_t>(std::clamp(car->def != nullptr ? car->def->id : 0, 0, int(CareerProfile::kModelCapacity - 1)));
    const int level = career.profile.paintLevel(model);
    if (event == input::kLeft || event == input::kRight || m_first != 0) {
        m_first = 0;
        if (s.find("Menu") >= 0 && s.menuSize("Menu") != 0) {
            s.setHidden("Locked", true);
            const std::string command = s.menuItemCommand("Menu", s.selectedMenuItem("Menu"));
            if (_stricmp(command.c_str(), "base") == 0) {
                garage.price(host, garage.paintPending + (garage.paintCharged[0] == 0 ? 1000 : 0));
            } else if (_stricmp(command.c_str(), "metal") == 0) {
                garage.price(host, garage.paintPending + (garage.paintCharged[1] == 0 ? 3000 : 0));
                if (level < 0) s.setHidden("Locked", false);
            } else if (_stricmp(command.c_str(), "pearl") == 0) {
                garage.price(host, garage.paintPending + (garage.paintCharged[2] == 0 ? 5000 : 0));
                if (level < 1) s.setHidden("Locked", false);
            } else if (_stricmp(command.c_str(), "wheel") == 0) {
                garage.price(host, 1000);
            }
        }
    }
    if (event == input::kSelect) {
        if (career.profile.money < garage.texts.price) {
            host.message(MessageType::Continue, texts.at(kTextInsufficientCash), false, nullptr);
            host.sound("NO");
        } else if (s.find("Menu") >= 0 && s.menuSize("Menu") != 0) {
            host.sound("CLICK");
            const std::string command = s.menuItemCommand("Menu", s.selectedMenuItem("Menu"));
            if (_stricmp(command.c_str(), "base") == 0) {
                garage.paintMode = PaintMode::Base;
                host.request("colourchoose", SwitchMode::Swap);
            } else if (_stricmp(command.c_str(), "metal") == 0) {
                if (level >= 0) {
                    garage.paintMode = PaintMode::Metallic;
                    host.request("colourchoose", SwitchMode::Swap);
                } else {
                    host.sound("NO");
                }
            } else if (_stricmp(command.c_str(), "pearl") == 0) {
                if (level >= 1) {
                    garage.paintMode = PaintMode::Pearl;
                    host.request("colourchoose", SwitchMode::Swap);
                } else {
                    host.sound("NO");
                }
            } else if (_stricmp(command.c_str(), "wheel") == 0) {
                host.request("wheelcolourmenu", SwitchMode::Swap);
            }
        }
    } else if (event == input::kSelect2) {
        if (garage.paintPending != 0) {
            host.sound("CLICK");
            const std::wstring text = texts.at(kTextConfirmColour) + L"\n " +
                                      toWide(moneyText(garage.paintPending, host.language().index)) + L" \n" +
                                      texts.at(kTextAreYouSure);
            host.message(MessageType::YesNo, text, false, &m_answer, true);
            m_askedBuy = 1;
        }
    } else if (event == input::kBack) {
        host.sound("CLICK");
        if (garage.paintPending != 0) {
            host.message(MessageType::YesNo, texts.at(kTextBuyPaint + 1) + L"\n" + texts.at(kTextAreYouSure), false,
                         &m_answer, true);
            m_askedLose = 1;
        } else {
            m_answer = -1;
            m_askedBuy = 0;
            garage.paintPending = 0;
            host.request("custmenu", SwitchMode::Swap);
        }
    }
    garage.lowerChassis(host);
}

// 0x0043DB90: the base enter; the camera to the garage's place; the primer of the car's slots made the body's
// (0x0048C210, the car's folders pushed); HELP shown; SELCOLOUR looks for the paint's colour (base, metallic, pearl;
// for the decal (3) the pearl's: Stock problem, the decal's own colour is not looked for) and +0x10 keeps the paint
// (the decal's colour, the car's +0x4C, for 3); the price and its box come in; no question, the answer -1.
void ColourChoose::enter(GarageHost &host, ui::Screen &s) {
    Garage &garage = host.garage();
    host.baseEnter(s, "colourchoose");
    for (int i = 0; i < 2; ++i) {
        auto &p = m_chartPlaces[i];
        const int index = s.find(i == 0 ? "GRIDX" : "GRIDY");
        if (!m_chartPlacesSaved)
            s.placeOf(index, p[0], p[1], p[2], p[3]);
        else
            s.setPlace(index, p[0], p[1], p[2], p[3]);
    }
    m_chartPlacesSaved = true;
#ifdef OJ_MACOS
    if (garage.paintMode == PaintMode::Neon) {
        if (auto *car = garage.car(host)) {
            auto n = mods::neon(*car);
            n.rgb = car->neon & 0xffffff;
            n.effect %= mods::effectCount();
            m_original = car->neon;
            s.chartFind("SELCOLOUR", car->neon);
            mods::previewNeon(*car, n);
            mods::neonEditor(true);
            if (garage.view) {
                garage.view->showNeon(*car, host.viewLoading());
                garage.view->cameraTo({-180.f, 210.f, 580.f, 0.f});
                garage.view->zoomTo(1.f);
            }
        }
        s.setHidden("HELP", true);
        for (auto field : {std::pair{"GRIDX", .19f}, {"GRIDY", .66f}}) {
            const int index = s.find(field.first);
            float x, y, w, h;
            s.placeOf(index, x, y, w, h);
            s.setPlace(index, field.second, y, field.second < .5f ? .46f : .32f, h);
        }
        garage.priceShown(host, true);
        garage.price(host, 1000);
        return;
    }
#endif
    if (garage.view)
        garage.view->cameraTo(kCameraGarage);
    if (garage.view && garage.view->car() != nullptr)
        garage.view->car()->primerToBody();
    s.show("HELP", false);
    Car *car = garage.car(host);
    if (car != nullptr && s.find("SELCOLOUR") >= 0) {
        switch (garage.paintMode) {
        case PaintMode::Metallic:
            s.chartFind("SELCOLOUR", car->metallic);
            m_original = car->metallic;
            break;
        case PaintMode::Pearl:
            s.chartFind("SELCOLOUR", car->pearl);
            m_original = car->pearl;
            break;
        case PaintMode::SponsorDecals:
            s.chartFind("SELCOLOUR", car->pearl);
            m_original = car->sponsorColour;
            break;
        default:
            s.chartFind("SELCOLOUR", car->base);
            m_original = car->base;
            break;
        }
    }
    garage.priceShown(host, true);
    m_asked = 0;
    m_answer = -1;
}

// 0x0043DD60: [0x0075F108] 2; the garage's view's zoom goal 1. After the question a "yes" clears the garage's texts
// and goes to colourmenu; else it is cleared. ([0x0075ED10], which nothing sets, would go to colourmenu.) The chassis
// lowered. With the chart not looking: its colour goes to the paint (base: the base and the pearl, and the first base
// change since custmenu's paint also clears the metallic; metallic or pearl: theirs, the first change flag cleared;
// the decal: the car's +0x4C, its model's parts again (0x0048B440, 0x0048B690)); GRIDX "X<column>",
// GRIDY "Y<row>". BACK: the paint back (base: the base +0x10, the metallic and pearl colourmenu's; metallic or pearl:
// colourmenu's) and the amounts colourmenu's, CLICK and colourmenu; the decal: its colour back, CLICK and
// sponsordecalmenu. SELECT (CLICK): the decal CLICK and stylingmenu; metallic or pearl with the money over the price
// HELP hidden and colourfocus; else the question answered: the base charged 1000 when not yet. Stock problem: a
// metallic or pearl colour chosen without the money over the price is taken as a base paint, its own price unpaid.
// The paint on the model (0x0048BF90) after every frame but the decal's and the ends.
void ColourChoose::update(GarageHost &host, ui::Screen &s, uint32_t event) {
    GameData *data = host.gameData();
    Garage &garage = host.garage();
    Car *car = garage.car(host);
    if (car == nullptr || data == nullptr || !data->career)
        return;
#ifdef OJ_MACOS
    if (garage.paintMode == PaintMode::Neon) {
        if (auto shortcut = mods::takeNeonEvent())
            event = shortcut;
        if (garage.view)
            garage.view->zoomTo(1.f);
        auto n = mods::neon(*car);
        if (s.find("SELCOLOUR") >= 0 && !s.chartFinding("SELCOLOUR"))
            n.rgb = s.chartColour("SELCOLOUR") & 0xffffff;
        if (event == input::kSelect2)
            n.effect = (n.effect + 1) % mods::effectCount();
        if (event == input::kSelect3)
            n.speed = n.speed % 5 + 1;
        car->neon = 0xff000000 | n.rgb;
        mods::previewNeon(*car, n);
        if (garage.view)
            garage.view->colourNeon(mods::neonColour(*car, mods::seconds()));
        s.setText("GRIDX", toWide("Q: " + mods::effectName(n.effect)));
        s.setText("GRIDY", toWide("E: SPEED " + std::to_string(n.speed)));
        if (event == input::kBack) {
            car->neon = m_original;
            mods::clearNeonPreview();
            mods::neonEditor(false);
            host.request("", SwitchMode::Pop);
            host.sound("CLICK");
        } else if (event == input::kSelect) {
            mods::neonEditor(false);
            host.request("buymod", SwitchMode::Swap);
            host.sound("CLICK");
        }
        return;
    }
#endif
    if (garage.view)
        garage.view->zoomTo(1.0f);
    if (m_asked != 0) {
        if (m_answer == 1) {
            garage.clear(host);
            host.request("colourmenu", SwitchMode::Swap);
            return;
        }
        m_answer = -1;
        m_asked = 0;
    }
    if (garage.paintBack != 0) {
        host.request("colourmenu", SwitchMode::Swap);
        garage.paintBack = 0;
        return;
    }
    garage.lowerChassis(host);
    if (s.find("SELCOLOUR") >= 0 && !s.chartFinding("SELCOLOUR")) {
        const uint32_t colour = s.chartColour("SELCOLOUR");
        switch (garage.paintMode) {
        case PaintMode::Metallic:
            car->metallic = withRgb(car->metallic, colour);
            garage.paintReset = 0;
            break;
        case PaintMode::Pearl:
            car->pearl = withRgb(car->pearl, colour);
            garage.paintReset = 0;
            break;
        case PaintMode::SponsorDecals:
            car->sponsorColour = colour;
            garage.refreshParts(host);
            break;
        default:
            car->base = withRgb(car->base, colour);
            if (garage.paintReset != 0) car->metallic = withRgb(car->metallic, 0);
            car->pearl = withRgb(car->pearl, colour);
            break;
        }
        if (s.find("GRIDX") >= 0) s.setText("GRIDX", toWide(std::format("X{}", s.chartColumn("SELCOLOUR"))));
        if (s.find("GRIDY") >= 0) s.setText("GRIDY", toWide(std::format("Y{}", s.chartRow("SELCOLOUR"))));
    }
    if (event == input::kBack) {
        switch (garage.paintMode) {
        case PaintMode::Metallic:
            car->metallic = garage.paintMetallic;
            car->metallicAmount = garage.paintMetallicAmount;
            break;
        case PaintMode::Pearl:
            car->pearl = garage.paintPearl;
            car->pearlAmount = garage.paintPearlAmount;
            break;
        case PaintMode::SponsorDecals:
            car->sponsorColour = m_original;
            garage.refreshParts(host);
            host.sound("CLICK");
            host.request("sponsordecalmenu", SwitchMode::Swap);
            return;
        default:
            car->base = m_original;
            car->metallic = garage.paintMetallic;
            car->pearl = garage.paintPearl;
            break;
        }
        car->metallicAmount = garage.paintMetallicAmount;
        car->pearlAmount = garage.paintPearlAmount;
        host.sound("CLICK");
        host.request("colourmenu", SwitchMode::Swap);
    } else if (event == input::kSelect) {
        host.sound("CLICK");
        if (garage.paintMode == PaintMode::SponsorDecals) {
            garage.refreshParts(host);
            host.sound("CLICK");
            host.request("stylingmenu", SwitchMode::Swap);
            return;
        }
        if (garage.paintMode != PaintMode::Base && data->career->profile.money > garage.texts.price) {
            for (int i = 0; i < s.size(); ++i) {
                if (_stricmp(s.nameOf(i).c_str(), "HELP") == 0) s.setHidden(i, true);
            }
            host.request("colourfocus", SwitchMode::Swap);
        } else {
            m_asked = 1;
            if (garage.paintCharged[0] == 0) garage.paintPending += 1000;
            garage.paintCharged[0] = 1;
            m_answer = 1;
        }
    }
    garage.repaint(host);
}

// 0x0043E2D0: the base enter; the camera to the garage's place; the primer of the car's slots made the body's
// (0x0048C210); FOCUSSLIDER at the metallic amount (paint 1) or the pearl amount x 5; the price and its box come in;
// the answer -1, no question.
void ColourFocus::enter(GarageHost& host, ui::Screen& s) {
    Garage& garage = host.garage();
    host.baseEnter(s, "colourfocus");
    if (garage.view) garage.view->cameraTo(kCameraGarage);
    if (garage.view && garage.view->car() != nullptr) garage.view->car()->primerToBody();
    if (Car* car = garage.car(host); car != nullptr && s.find("FOCUSSLIDER") >= 0) {
        s.setSliderValue("FOCUSSLIDER", garage.paintMode == PaintMode::Metallic
                                            ? Car::asFloat(car->metallicAmount)
                                            : Car::asFloat(car->pearlAmount) * 5.0f);
    }
    garage.priceShown(host, true);
    m_answer = -1;
    m_asked = 0;
}

// 0x0043E410: [0x0075F108] 2; GRIDX "Z<the slider x 100, cut>". After the question a "yes" clears the garage's texts
// and goes to colourmenu; else it is cleared. The chassis lowered; the metallic amount the slider's (paint 1), else
// the pearl amount the slider's x 0.2; the paint on the model. SELECT: the metallic charged 3000 or the pearl 5000
// when not yet, CLICK, the question answered. BACK (CLICK): colourchoose.
void ColourFocus::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr) return;
    if (s.find("GRIDX") >= 0) {
        s.setText("GRIDX", toWide(std::format("Z{}", x87::chop(s.sliderValue("FOCUSSLIDER") * 100.0f))));
    }
    if (m_asked != 0) {
        if (m_answer == 1) {
            garage.clear(host);
            host.request("colourmenu", SwitchMode::Swap);
            return;
        }
        m_answer = -1;
        m_asked = 0;
    }
    garage.lowerChassis(host);
    if (s.find("FOCUSSLIDER") >= 0) {
        if (garage.paintMode == PaintMode::Metallic) {
            car->metallicAmount = Car::bitsOf(s.sliderValue("FOCUSSLIDER"));
        } else {
            car->pearlAmount = Car::bitsOf(s.sliderValue("FOCUSSLIDER") * 0.2f);
        }
        garage.repaint(host);
    }
    if (event == input::kSelect) {
        if (garage.paintMode == PaintMode::Metallic) {
            if (garage.paintCharged[1] == 0) garage.paintPending += 3000;
            garage.paintCharged[1] = 1;
        } else if (garage.paintMode == PaintMode::Pearl) {
            if (garage.paintCharged[2] == 0) garage.paintPending += 5000;
            garage.paintCharged[2] = 1;
        }
        host.sound("CLICK");
        m_asked = 1;
        m_answer = 1;
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("colourchoose", SwitchMode::Swap);
    }
}

// 0x0043F3F0: the base enter; the price and its box come in; the price 500; COLOURMENU emptied and an item for each
// wheel colour of the CarDefs ([WheelColour]): ui_wheel_off and _on in the colour, its place; the car's own colour (its
// +0x40 set) is +0x10 (2 without), kept as the wheels' (0x0048C5D0) and highlighted (the first past the list).
void WheelColourMenu::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    host.baseEnter(s, "wheelcolourmenu");
    garage.priceShown(host, true);
    garage.price(host, 500);
    m_own = 2;
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr || s.find("COLOURMENU") < 0) return;
    s.clearMenu("COLOURMENU");
    size_t own = 0;
    const std::vector<uint32_t>& wheels = data->defs.wheels;
    for (size_t i = 0; i < wheels.size(); ++i) {
        s.addMenuItem("COLOURMENU", L"", static_cast<float>(i), "ui_wheel_off", "ui_wheel_on");
        s.setMenuItemColour("COLOURMENU", i, wheels[i]);
        if (car->wheelsColoured != 0 && car->wheelColour == wheels[i]) {
            m_own = static_cast<int32_t>(i);
            if (garage.view && garage.view->car() != nullptr) garage.view->car()->keepWheelColours(*car);
            own = i;
        }
    }
    if (s.menuSize("COLOURMENU") != 0) s.selectMenuItem("COLOURMENU", own > s.menuSize("COLOURMENU") - 1 ? 0 : own);
}

// 0x0043F600: [0x0075F108] 2. A colour bought: colourmenu (the model's colours kept, 0x0048C5D0). The highlighted
// colour on the car (+0x3C, +0x40 1) and its wheels (0x0048C440). SELECT (CLICK): the car's own (+0x10) "You already
// have this mod" (type 1), another buymod. BACK (CLICK): colourmenu, the wheels' kept colours back (0x0048C530) and the
// colour +0x10 back on the car (0x0048C4F0). Stock problem: a car without a wheel colour of the list gets the third one
// (+0x10 2).
void WheelColourMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr || s.find("COLOURMENU") < 0 || s.menuSize("COLOURMENU") == 0) return;
    const std::vector<uint32_t>& wheels = data->defs.wheels;
    const auto colourOf = [&](int i) {
        return i >= 0 && static_cast<size_t>(i) < wheels.size() ? wheels[static_cast<size_t>(i)] : car->wheelColour;
    };
    if (garage.texts.bought != 0) {
        host.request("colourmenu", SwitchMode::Swap);
        if (garage.view && garage.view->car() != nullptr) garage.view->car()->keepWheelColours(*car);
    }
    const size_t item = s.selectedMenuItem("COLOURMENU");
    car->wheelColour = colourOf(x87::chop(s.menuItemValue("COLOURMENU", item)));
    car->wheelsColoured = 1;
    if (garage.view && garage.view->car() != nullptr) garage.view->car()->colourWheels(*car);
    if (event == input::kSelect) {
        host.sound("CLICK");
        if (static_cast<int32_t>(item) == m_own) {
            host.message(MessageType::Continue, texts.at(kTextHaveMod), false, nullptr);
        } else {
            host.request("buymod", SwitchMode::Push);
        }
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("colourmenu", SwitchMode::Swap);
        if (garage.view && garage.view->car() != nullptr) garage.view->car()->restoreWheelColours();
        car->wheelColour = colourOf(m_own);
        car->wheelsColoured = 1;
        if (garage.view && garage.view->car() != nullptr) garage.view->car()->colourWheels(*car);
    }
}

}  // namespace oj::frontend
