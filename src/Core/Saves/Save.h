#pragma once
// The saves of the stock game (docs/spec/03-saves.md): %APPDATA%\THQ\Juiced\SAVE\P<driver>\juicey.dat and USERDATA,
// SAVE\HS\USERDATA, and reg.cf1 next to SAVE.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace oj::save {

// juicey.dat: named entries.
//   u32 entryCount, u32 directoryOffset (= 8 + the entries' sizes)
//   the entries' bytes, one after another in the order of the directory
//   directory: per entry char name[128] (zero padded), u32 size; entries in name order
//   u64 hash of the entries' bytes (all bytes from offset 8 to the directory)
// The save's files container packs its files but USERDATA ([0x00778A00]) into it (+0x1C, 0x00501700) and unpacks
// them (+0x20, 0x00501500); here the entries are its files.
class Archive {
public:
    // Byte order of the names, as the game's map keeps them.
    std::map<std::string, std::vector<uint8_t>> entries;

    // 0x00501500: the count and the directory's offset (a file of 8 bytes or less has none); each entry's name and
    // size from the directory (it stops when the next record would run into the hash's 8 bytes), its bytes the next
    // ones after the last entry's (as many as are left), hashed; the directory must end where the hash starts and the
    // hash must be the entries'. False with *error otherwise (the entries read before it stay).
    bool read(const std::vector<std::byte>& file, std::string* error);
    // 0x00501700: the count and offset, the entries' bytes, the directory (a name over 128 characters runs past its
    // field in the stock game: Stock problem; cut here) and the hash.
    std::vector<uint8_t> write() const;
};

// The hash of the archive (0x00503030, vtable 0x006F70A4): starting at 0x5A5A5A5A5A5A5A5A, per byte the 64-bit value
// is rotated left by 1 and the byte is XORed into its low bits.
uint64_t archiveHash(const uint8_t* data, size_t size, uint64_t state = 0x5A5A5A5A5A5A5A5Aull);

// USERDATA of a driver folder: f32 (1.38 in every save seen), the driver's name in UTF-16 ending with 0.
struct UserData {
    float value = 0.0f;
    std::wstring name;

    bool read(const std::vector<std::byte>& file);
    std::vector<uint8_t> write() const;
};

// reg.cf1: 0x4C bytes. u32 version 0x10000, u32 the sum of the 16 dwords after it, 16 dwords, u32 a pointer of the
// game's memory written along (not read).
struct RegCf1 {
    uint32_t version = 0x10000;
    uint32_t sum = 0;
    uint32_t values[16] = {};
    uint32_t pointer = 0;

    bool read(const std::vector<std::byte>& file);
    uint32_t computeSum() const;
    std::vector<uint8_t> write() const;  // with the sum computed
};

// %APPDATA%\THQ\<title>
std::filesystem::path root(const std::wstring& title = L"Juiced");

// 0x005A12E0: each folder down to the path made (CreateDirectory at each '\', then the path itself); one there already
// or one that cannot be made is passed over. The save device's file object (0x00502E60, made at the game's start)
// makes the root's SAVE so, reg.cf1's writer (0x00481630) the root.
void makeFolders(const std::filesystem::path& folder);

struct Driver {
    std::filesystem::path folder;  // SAVE\P<name>
    UserData userData;
    bool hasArchive = false;
};

// The driver folders of SAVE: those starting with "P" holding USERDATA.
std::vector<Driver> listDrivers(const std::filesystem::path& saveFolder);

// The folder name of a driver (0x0040AA20): "P" and the name with spaces as '_'.
std::wstring driverFolderName(const std::wstring& name);

// A new driver's folder SAVE\P<name> with USERDATA (1.38) and juicey.dat of the entries. A folder that is there
// already is left as it is and the driver is not written (*error says so).
bool writeNewDriver(const std::filesystem::path& saveFolder, const std::wstring& name, const Archive& archive,
                    std::string* error);

}  // namespace oj::save
