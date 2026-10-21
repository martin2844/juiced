#include "Framework/Test.h"
#include "Core/Base/GameFiles.h"
#include <filesystem>
#include <fstream>
#include <chrono>

OJ_UNIT_TEST(GameFiles, OptionalModOverlay) {
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/ ("juiced-mod-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/"game/scripts");fs::create_directories(root/"mod/scripts");
    std::ofstream(root/"game/scripts/base.txt") << "retail";
    std::ofstream(root/"game/scripts/other.txt") << "fallback";
    std::ofstream(root/"mod/scripts/base.txt") << "mod";
    std::ofstream(root/"mod/scripts/local_e.txt") << "localized";
    oj::GameFiles files(root/"game");
    auto text=[&](const fs::path& p){auto b=files.read(p);return b?std::string(reinterpret_cast<const char*>(b->data()),b->size()):std::string();};
    oj::GameFiles::setOverrideRoot(root/"mod");
    oj::test::check(text("SCRIPTS\\BASE.TXT")=="mod", "case and Windows separators normalized for mod files");
    oj::test::check(text(root/"game/scripts/base.txt")=="mod", "absolute game path uses mod");
    oj::test::check(text("scripts/other.txt")=="fallback", "missing mod asset falls back to retail");
    oj::test::check(text(root/"game/scripts/other.txt")=="fallback", "absolute retail fallback");
    oj::GameFiles::setLanguageLetter('e');
    oj::test::check(files.exists("scripts/local.txt") && text("scripts/local.txt")=="localized", "localized mod lookup agrees with exists");
    oj::GameFiles::setLanguageLetter(0);
    oj::GameFiles::setOverrideRoot({});
    oj::test::check(text("scripts/base.txt")=="retail", "disabling overlay restores retail files");
    fs::remove_all(root);
}
