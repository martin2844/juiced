#pragma once
// What the garage's windows share: the car shown, its scene, the price, the money and the class shown, the parts put
// on the car's model and the customising windows' globals.

#include "Core/Cars/Car.h"
#include "Core/Cars/CarDefs.h"
#include "Game/Cars/CarView.h"
#include "Game/Frontend/WindowHost.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace oj::frontend {

// What colourchoose paints (the garage's paint mode): the base colour, the metallic or the pearl colour, the sponsor
// decals' colour (from sponsordecalmenu).
enum class PaintMode : int32_t { Base = 0, Metallic = 1, Pearl = 2, SponsorDecals = 3, Neon = 4 };

// The garage's texts ([0x00767D58]): the price shown and the elements of the price, the money and the price's box,
// the class, its meter and the rating; the car shown is a prototype ([0x0075ED0C]).
struct GarageTexts {
    int32_t price = 0;
    std::string priceText, cashText, boxText;
    std::string classText, classMeter, bhpText;
    bool prototype = false;
    float newRating = 0.0f;  // +0x18 ([0x00767D70]): the rating with the part to buy (0 none shown)
    float refund = 0.0f;     // +0x1C ([0x00767D74]): the money back for the part taken off
    int32_t bought = 0;      // +0x2C ([0x00767D84]): a part bought
};

class Garage {
public:
    // The car of the dealer's list shown (+0x27D8 at +0x27DC): the showroom in mode 1, else the garage.
    Car* car(WindowHost& host);
    // 0x00447090: the price kept and shown in the base window (thousands marked); the bought flag 0.
    void price(WindowHost& host, int32_t price);
    // 0x00447050: the price 0 and its text emptied; the rating, the bought flag and the refund 0.
    void clear(WindowHost& host);
    // 0x00447100: the money shown.
    void cash(WindowHost& host);
    // 0x004473B0: the price and its box come in or go out.
    void priceShown(WindowHost& host, bool shown);
    // 0x00447170: the car's class and rating shown, a prototype's change noted.
    void carInfo(WindowHost& host);
    // 0x004B5580: the deal of the dealer's mode once: a car bought or sold.
    bool deal(WindowHost& host);
    // The car's paint, tint and parts on the model again.
    void repaint(WindowHost& host);
    void retint(WindowHost& host);
    void refreshParts(WindowHost& host);
    // A part of the car and its mesh on the garage's model: the exhaust (0x0048BE30), the wheels (0x0048ABC0), a body
    // part (0x0048BC30) and the decal.
    void fitExhaust(WindowHost& host, Car& car, int part);
    void fitWheels(WindowHost& host, Car& car, int part);
    void fitBodyPart(Car& car, int category, int part);
    void fitDecal(Car& car, int part);
    // The rating of the car with a part of a kind (0x0048E8A0), then its own part back and rated again.
    float rateWithPart(WindowHost& host, Car& car, size_t partKind, uint8_t part);
    // The customising windows' frame: the chassis lowered by MAXDROPHEIGHT x the car's +0x58.
    void lowerChassis(WindowHost& host);
    // 0x00440320, the leave of exhaustmenu, gearboxmenu and the styling windows.
    void leavePriced(WindowHost& host, ui::Screen& s, const std::string& state);

    GarageTexts texts;                         // [0x00767D58]
    std::unique_ptr<scene::GarageView> view;   // the scene of carbuy and carsel (the car's model object [0x0075ED00])

    // The customising windows' globals.
    int32_t kind = 0;            // [0x00715C2C]: the kind of catalogue part
    int32_t brand = -1;          // [0x00715C30]: its supplier
    int32_t catalogueLevel = 0;  // the catalogue's +0x100: the level its suppliers and parts are listed for
    int32_t partShown = 0;       // [0x0075ED08]: the place of the part shown in the parts' list
    std::vector<int> brands;     // the catalogue's +0xF0: the suppliers as performmod's enter left them
    std::vector<const CarPart*> filtered;  // the catalogue's +0x104 (0x004990F0) as the last draw left it
    int32_t styleCategory = 0;   // [0x00715C28]: the body part category of modmenu
    // The paint windows': the paint as custmenu found it (0x00767D90 .. 0x00767D98: base, metallic, pearl), the
    // paints charged so far (0x00728B00 base, 0x00728AFC metallic, 0x00728AF8 pearl), the first base change also
    // clearing the metallic colour ([0x00715C24]), the paint chosen ([0x00728B0C]: 0 base, 1 metallic, 2 pearl, 3 the
    // decal), the price of the changes ([0x0075ECFC]), colourmenu's metallic and pearl colours and amounts
    // (0x00767D88, 0x00767D8C, 0x00728B08, 0x00728B04) and a flag nothing sets ([0x0075ED10]).
    std::array<uint32_t, 3> paintKept{};
    std::array<int32_t, 3> paintCharged{};
    int32_t paintReset = 0;
    PaintMode paintMode = PaintMode::Base;
    int32_t paintPending = 0;
    uint32_t paintMetallic = 0;
    uint32_t paintPearl = 0;
    uint32_t paintMetallicAmount = 0;
    uint32_t paintPearlAmount = 0;
    int32_t paintBack = 0;
};

}  // namespace oj::frontend
