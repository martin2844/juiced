#include "Core/Saves/Save.h"

#include "Core/Base/GameFiles.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cstring>
#include <format>

namespace oj::save {

namespace {

template <class T>
T get(const std::vector<std::byte>& data, size_t offset) {
    T value{};
    std::memcpy(&value, data.data() + offset, sizeof(T));
    return value;
}

template <class T>
void put(std::vector<uint8_t>& out, T value) {
    const auto* p = reinterpret_cast<const uint8_t*>(&value);
    out.insert(out.end(), p, p + sizeof(T));
}

constexpr size_t kNameSize = 128;

}  // namespace

uint64_t archiveHash(const uint8_t* data, size_t size, uint64_t state) {
    for (size_t i = 0; i < size; ++i) {
        state = (state << 1) | (state >> 63);
        state ^= data[i];
    }
    return state;
}

bool Archive::read(const std::vector<std::byte>& file, std::string* error) {
    entries.clear();
    auto fail = [&](std::string message) {
        if (error != nullptr) *error = std::move(message);
        return false;
    };
    if (file.size() <= 8) {
        return fail("8 bytes or less");
    }
    const auto count = get<uint32_t>(file, 0);
    const auto directory = get<uint32_t>(file, 4);
    const auto* bytes = reinterpret_cast<const uint8_t*>(file.data());
    const int64_t end = static_cast<int64_t>(file.size()) - 8;
    int64_t record = directory;
    size_t offset = 8;
    uint64_t computed = 0x5A5A5A5A5A5A5A5Aull;
    for (uint32_t i = 0; i < count; ++i) {
        if (record > end - static_cast<int64_t>(kNameSize + 4)) break;
        const char* name = reinterpret_cast<const char*>(bytes + record);
        const std::string key(name, strnlen(name, kNameSize));
        const auto size = get<uint32_t>(file, static_cast<size_t>(record) + kNameSize);
        record += kNameSize + 4;
        const size_t take = std::min<size_t>(size, file.size() - std::min(offset, file.size()));
        entries[key].assign(bytes + offset, bytes + offset + take);
        computed = archiveHash(bytes + offset, take, computed);
        offset += take;
    }
    if (record != end) {
        return fail(std::format("{} entries, directory at {}: it does not end at the hash of {} bytes", count, directory,
                                file.size()));
    }
    const uint64_t stored = get<uint64_t>(file, file.size() - 8);
    if (stored != computed) {
        return fail(std::format("hash {:016X}, the data gives {:016X}", stored, computed));
    }
    return true;
}

std::vector<uint8_t> Archive::write() const {
    std::vector<uint8_t> out;
    size_t total = 0;
    for (const auto& [name, data] : entries) {
        total += data.size();
    }
    put<uint32_t>(out, static_cast<uint32_t>(entries.size()));
    put<uint32_t>(out, static_cast<uint32_t>(8 + total));
    for (const auto& [name, data] : entries) {
        out.insert(out.end(), data.begin(), data.end());
    }
    const uint64_t hash = archiveHash(out.data() + 8, total);
    for (const auto& [name, data] : entries) {
        uint8_t field[kNameSize] = {};
        std::memcpy(field, name.data(), std::min(name.size(), kNameSize));
        out.insert(out.end(), field, field + kNameSize);
        put<uint32_t>(out, static_cast<uint32_t>(data.size()));
    }
    put<uint64_t>(out, hash);
    return out;
}

bool UserData::read(const std::vector<std::byte>& file) {
    if (file.size() < 4) {
        return false;
    }
    value = get<float>(file, 0);
    name.clear();
    for (size_t i = 4; i + 1 < file.size(); i += 2) {
        const auto c = static_cast<wchar_t>(get<uint16_t>(file, i));
        if (c == 0) {
            break;
        }
        name.push_back(c);
    }
    return true;
}

std::vector<uint8_t> UserData::write() const {
    std::vector<uint8_t> out;
    put<float>(out, value);
    for (wchar_t c : name) {
        put<uint16_t>(out, static_cast<uint16_t>(c));
    }
    put<uint16_t>(out, 0);
    return out;
}

bool RegCf1::read(const std::vector<std::byte>& file) {
    if (file.size() != 0x4C) {
        return false;
    }
    version = get<uint32_t>(file, 0);
    sum = get<uint32_t>(file, 4);
    for (size_t i = 0; i < 16; ++i) {
        values[i] = get<uint32_t>(file, 8 + i * 4);
    }
    pointer = get<uint32_t>(file, 0x48);
    return version == 0x10000 && sum == computeSum();
}

uint32_t RegCf1::computeSum() const {
    uint32_t s = 0;
    for (uint32_t v : values) {
        s += v;
    }
    return s;
}

std::vector<uint8_t> RegCf1::write() const {
    std::vector<uint8_t> out;
    put<uint32_t>(out, version);
    put<uint32_t>(out, computeSum());
    for (uint32_t v : values) {
        put<uint32_t>(out, v);
    }
    put<uint32_t>(out, pointer);
    return out;
}

std::filesystem::path root(const std::wstring& title) {
    wchar_t appData[MAX_PATH] = {};
    SHGetSpecialFolderPathW(nullptr, appData, CSIDL_APPDATA, TRUE);
    return std::filesystem::path(appData) / L"THQ" / title;
}

void makeFolders(const std::filesystem::path& folder) {
    std::error_code ignored;
    std::filesystem::create_directories(folder, ignored);
}

std::vector<Driver> listDrivers(const std::filesystem::path& saveFolder) {
    std::vector<Driver> drivers;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(saveFolder, ec)) {
        const std::wstring name = entry.path().filename().wstring();
        if (!entry.is_directory() || name.empty() || name[0] != L'P') {
            continue;
        }
        const GameFiles folder(entry.path());
        const auto userData = folder.read(L"USERDATA");
        Driver d;
        d.folder = entry.path();
        if (!userData || !d.userData.read(*userData)) {
            continue;
        }
        d.hasArchive = folder.exists(L"juicey.dat");
        drivers.push_back(std::move(d));
    }
    return drivers;
}

}  // namespace oj::save

namespace oj::save {

std::wstring driverFolderName(const std::wstring& name) {
    std::wstring folder = L"P" + name;
    std::replace(folder.begin(), folder.end(), L' ', L'_');
    return folder;
}

bool writeNewDriver(const std::filesystem::path& saveFolder, const std::wstring& name, const Archive& archive,
                    std::string* error) {
    const std::filesystem::path folder = saveFolder / driverFolderName(name);
    std::error_code ec;
    if (std::filesystem::exists(folder, ec)) {
        if (error) *error = "the folder of the driver is there already";
        return false;
    }
    if (!std::filesystem::create_directories(folder, ec)) {
        if (error) *error = "the folder of the driver cannot be made: " + ec.message();
        return false;
    }
    UserData userData;
    userData.value = 1.38f;
    userData.name = name;
    auto store = [&](const wchar_t* file, const std::vector<uint8_t>& bytes) {
        HANDLE h = CreateFileW((folder / file).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
                               nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;
        DWORD written = 0;
        const BOOL ok = WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
        CloseHandle(h);
        return ok && written == bytes.size();
    };
    if (!store(L"USERDATA", userData.write()) || !store(L"juicey.dat", archive.write())) {
        if (error) *error = "the files of the driver cannot be written";
        return false;
    }
    return true;
}

}  // namespace oj::save
