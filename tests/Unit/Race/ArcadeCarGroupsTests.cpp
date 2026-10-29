#include "Framework/Test.h"
#include "Core/Data/ExeTables.h"
#include "Core/Race/Arcade.h"
#include <string>

OJ_UNIT_TEST(ArcadeCarGroups, ExpandedRosterRetainsLastCarsAndWraps) {
    auto tables = oj::ExeTables::stock();
    for (int id = 53; id < 64; ++id) tables.carModels.push_back("EXTRA_" + std::to_string(id));
    std::string script;
    for (int group = 0; group < 8; ++group) script += "Level\n";
    for (size_t id = 1; id < tables.carModels.size(); ++id) script += "Car " + tables.carModels[id] + "\n";
    oj::ArcadeCarGroups groups;
    groups.load(script, tables);
    groups.group = 7;
    for (int id = 1; id < 64; ++id) {
        oj::test::check(groups.current() == id, "Every car remains selectable in the final group");
        groups.next();
    }
    oj::test::check(groups.index == 0 && groups.current() == 1, "Next wraps after the last added car");
    groups.previous();
    oj::test::check(groups.index == 62 && groups.current() == 63, "Previous wraps to the last added car");
}

OJ_UNIT_TEST(ArcadeCarGroups, StockWidthAndNavigationRemainUnchanged) {
    const auto tables = oj::ExeTables::stock();
    oj::ArcadeCarGroups groups;
    groups.load("Level\nCar celica\nCar fto\n", tables);
    oj::test::check(groups.ids.front().size() == 60, "Stock keeps its sixty-slot random selection range");
    groups.group = 0;
    oj::test::check(groups.current() == tables.carModelId("celica"), "Stock first car");
    groups.previous();
    oj::test::check(groups.current() == tables.carModelId("fto"), "Stock previous wraps");
    groups.next();
    oj::test::check(groups.current() == tables.carModelId("celica"), "Stock next wraps");
}
