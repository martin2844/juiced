// Unit tests: Archive.

#include "Framework/Test.h"

#include "Core/Saves/Save.h"

#include <cstring>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// A changed entry gives an archive whose hash holds.
OJ_UNIT_TEST(SaveArchive, ChangedEntryBreaksHash) {
    oj::save::Archive made;
    made.entries["b.sav"] = {1, 2, 3};
    made.entries["A.sav"] = {4};
    std::vector<uint8_t> bytes = made.write();
    std::vector<std::byte> asFile(bytes.size());
    std::memcpy(asFile.data(), bytes.data(), bytes.size());
    oj::save::Archive again;
    check(again.read(asFile, nullptr) && again.entries == made.entries, "a new archive reads back");
    asFile[8] ^= std::byte{1};
    check(!again.read(asFile, nullptr), "a changed byte breaks the hash");
}
