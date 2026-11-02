#include "Framework/Test.h"
#include "Core/Cars/CarDisplayName.h"

OJ_UNIT_TEST(CarDisplayName, AddedModelsDoNotReadPastRetailNames) {
    oj::TextTable texts;
    oj::CarDef car;
    car.id = 53;
    car.name = "FERRARI_458";
    car.displayName = "FERRARI 458 SPIDER";
    oj::test::check(oj::carDisplayName(texts, car) == L"FERRARI 458 SPIDER", "mod name comes from its catalogue, not unrelated text index 264");
    car.id = 54;
    car.displayName = "BMW 325i";
    oj::test::check(oj::carDisplayName(texts, car) == L"BMW 325i", "future added models also resolve their catalogue names");
    car.displayName.clear();
    oj::test::check(oj::carDisplayName(texts, car) == L"FERRARI_458", "missing display name falls back to model name");
    car.id = 52;
    oj::test::check(oj::carDisplayName(texts, car) == texts.at(oj::kTextModels + 52), "retail models retain localized names");
}
