// Integration tests: ModelFiles.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Base/GameFiles.h"
#include "Core/Formats/Model.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace {

using oj::test::check;
using oj::test::bytesOf;

}  // namespace

// Every model file of the game folder: .boh, .d8m (release layout), .pmd.
OJ_INTEGRATION_TEST(ModelFiles, EveryModel) {
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    size_t boh = 0, bohBad = 0, d8m = 0, d8mBad = 0, pmd = 0, pmdEmpty = 0;
    std::error_code ec;
    const std::filesystem::path gameRoot = game.root();
    for (auto it = std::filesystem::recursive_directory_iterator(gameRoot, ec);
         it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (ec || !it->is_regular_file()) continue;
        const std::filesystem::path& file = it->path();
        std::wstring ext = file.extension().wstring();
        for (wchar_t& c : ext) c = static_cast<wchar_t>(towlower(c));
        const std::wstring folder = file.parent_path().filename().wstring();
        if (ext != L".boh" && ext != L".d8m" && ext != L".pmd") continue;
        const auto content = oj::GameFiles(file.parent_path()).read(file.filename());
        if (!content) continue;
        const std::vector<uint8_t> data = bytesOf(*content);
        if (ext == L".boh") {
            ++boh;
            if (!oj::parseBoh(data)) ++bohBad;
        } else if (ext == L".d8m" && _wcsicmp(folder.c_str(), L"mesh") == 0 &&
                   file.parent_path().parent_path().filename() != L"tracks") {
            ++d8m;
            if (!oj::parseMesh(data)) ++d8mBad;
        } else if (ext == L".pmd") {
            ++pmd;
            const oj::Material m = oj::parseMaterial(
                std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
            if (m.className.empty()) ++pmdEmpty;
        }
    }
    std::printf("     models: %zu .boh (%zu not read), %zu .d8m of meshes (%zu not read), %zu .pmd (%zu "
                "without a class)\n",
                boh, bohBad, d8m, d8mBad, pmd, pmdEmpty);
    check(boh > 400 && bohBad == 0 && d8m > 7000 && d8mBad <= 119 && pmd > 10000 && pmdEmpty == 0,
          "model files: node trees, meshes and materials read");
}
