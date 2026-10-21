#include "Core/Saves/SaveDevice.h"

#include "Core/Saves/Save.h"

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <format>

namespace oj::save {

const std::array<FlagKind, 24> kFlagKinds = {
    FlagKind::Message,  FlagKind::State,   FlagKind::State,    FlagKind::State,     // Checking .. Unformatted
    FlagKind::Message,  FlagKind::Message, FlagKind::Question, FlagKind::Question,  // NoSpace .. Exists
    FlagKind::Message,  FlagKind::Message, FlagKind::Message,  FlagKind::Message,   // Loading .. Loaded
    FlagKind::Message,  FlagKind::Message, FlagKind::Message,  FlagKind::Message,   // Saved .. ReadError
    FlagKind::Message,  FlagKind::Message, FlagKind::Message,  FlagKind::Message,   // WriteError .. TooMany
    FlagKind::Question, FlagKind::Message, FlagKind::Message,  FlagKind::Message,   // DeleteAsk .. Deleting
};

namespace {

// The first characters of a folder of SAVE that make it a driver's ("P"), a replay's ("R") or the records' ("HS")
// (0x005D86C2 over 1, 1 and 2 characters).
bool startsWith(const std::wstring& name, const wchar_t* prefix) {
    const size_t n = std::wcslen(prefix);
    return name.size() >= n && _wcsnicmp(name.c_str(), prefix, n) == 0;
}

std::wstring withSlash(const std::filesystem::path& folder) {
    std::wstring s = folder.wstring();
    if (s.empty() || s.back() != L'\\') s += L'\\';
    return s;
}

// 0x00502490: USERDATA of a folder of SAVE, its find data (the last write) and its bytes.
bool readUserData(const std::wstring& folder, SaveSlot& slot) {
    const std::wstring path = folder + L"\\USERDATA";
    WIN32_FIND_DATAW find{};
    const HANDLE f = FindFirstFileW(path.c_str(), &find);
    if (f == INVALID_HANDLE_VALUE) return false;
    FindClose(f);
    const HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    std::vector<std::byte> bytes(find.nFileSizeLow);
    DWORD got = 0;
    ReadFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &got, nullptr);
    CloseHandle(h);
    UserData userData;
    userData.read(bytes);
    slot.name = userData.name;
    slot.value = userData.value;
    slot.written = (static_cast<uint64_t>(find.ftLastWriteTime.dwHighDateTime) << 32) |
                   find.ftLastWriteTime.dwLowDateTime;
    return true;
}

// 0x005026B0: a replay's USERDATA (0x00502490: its find data and bytes; 0x00502420 takes the fields).
bool readReplayData(const std::wstring& folder, ReplaySlot& slot) {
    const std::wstring path = folder + L"\\USERDATA";
    WIN32_FIND_DATAW find{};
    const HANDLE f = FindFirstFileW(path.c_str(), &find);
    if (f == INVALID_HANDLE_VALUE) return false;
    FindClose(f);
    const HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    std::vector<uint8_t> bytes(find.nFileSizeLow);
    DWORD got = 0;
    ReadFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &got, nullptr);
    CloseHandle(h);
    // 0x79 bytes are copied whatever the file's size (Stock problem: a shorter file's copy runs past its buffer;
    // zeros here).
    bytes.resize(std::max<size_t>(bytes.size(), 0x79), 0);
    const auto u32 = [&](size_t at) {
        uint32_t v = 0;
        std::memcpy(&v, bytes.data() + at, 4);
        return static_cast<int32_t>(v);
    };
    const auto text = [&](size_t at, size_t n) {
        const char* p = reinterpret_cast<const char*>(bytes.data() + at);
        return std::string(p, strnlen(p, n));
    };
    slot.cars = u32(0);
    slot.type = u32(4);
    slot.track = text(8, 32);
    slot.route = text(0x28, 32);
    slot.date = text(0x48, 17);
    slot.driver.clear();
    for (size_t i = 0; i < 16; ++i) {
        uint16_t c = 0;
        std::memcpy(&c, bytes.data() + 0x59 + i * 2, 2);
        if (c == 0) break;
        slot.driver.push_back(static_cast<wchar_t>(c));
    }
    slot.written = (static_cast<uint64_t>(find.ftLastWriteTime.dwHighDateTime) << 32) |
                   find.ftLastWriteTime.dwLowDateTime;
    return true;
}

// 0x00502B30 and 0x00502C50: SAVE made when it is not there, then the folder in it.
bool makeFolder(const std::filesystem::path& save, const std::wstring& folder, std::wstring& made) {
    std::wstring root = save.wstring();
    while (!root.empty() && root.back() == L'\\') root.pop_back();
    WIN32_FIND_DATAW find{};
    HANDLE f = FindFirstFileW(root.c_str(), &find);
    if (f != INVALID_HANDLE_VALUE) {
        FindClose(f);
    } else if (!CreateDirectoryW(root.c_str(), nullptr)) {
        return false;
    }
    const std::wstring path = withSlash(save) + folder;
    f = FindFirstFileW(path.c_str(), &find);
    if (f == INVALID_HANDLE_VALUE) {
        if (!CreateDirectoryW(path.c_str(), nullptr)) return false;
    } else {
        FindClose(f);
    }
    made = path;
    return true;
}

// 0x005029F0: every file of the folder deleted, then the folder; a folder that is not there counts as deleted.
bool deleteFolder(const std::wstring& folder) {
    WIN32_FIND_DATAW find{};
    const HANDLE f = FindFirstFileW((folder + L"\\*").c_str(), &find);
    if (f == INVALID_HANDLE_VALUE) return true;
    do {
        if (find.cFileName[0] != L'.') DeleteFileW((folder + L"\\" + find.cFileName).c_str());
    } while (FindNextFileW(f, &find));
    FindClose(f);
    return RemoveDirectoryW(folder.c_str()) != 0;
}

}  // namespace

void SaveDevice::reset() {
    m_list.step = ListStep::First;
    m_read.step = ReadStep::Find;
    m_write.step = WriteStep::Source;
    m_state = DeviceState::Idle;
}

DeviceState SaveDevice::stepProfile(SaveRequest& request, SaveGameSide& game) {
    return step(request, Kind::Profile, game);
}

DeviceState SaveDevice::stepHighScores(SaveRequest& request, SaveGameSide& game) {
    return step(request, Kind::HighScores, game);
}

DeviceState SaveDevice::stepReplay(SaveRequest& request, SaveGameSide& game) {
    return step(request, Kind::Replay, game);
}

bool SaveDevice::setReplayObject(SaveReplays* replays) {
    if (replays == nullptr) return false;
    m_replayObject = replays;
    return true;
}

DeviceState SaveDevice::step(SaveRequest& request, Kind kind, SaveGameSide& game) {
    m_side.kind = kind;
    m_side.ready = 0;
    // 0x004FA190 / 0x004FA600 / 0x004FAA30
    if (m_side.buildSource != 0) {
        m_source = std::make_unique<SaveSource>();
        if (kind == Kind::HighScores) {
            game.buildHighScores(*m_source);
        } else if (kind == Kind::Replay) {
            SYSTEMTIME t{};
            GetLocalTime(&t);
            m_source->folder = std::format(L"R{:04}{:02}{:02}{:02}{:02}{:02}", t.wYear, t.wMonth, t.wDay, t.wHour,
                                           t.wMinute, t.wSecond);
            const int hour = t.wHour > 12 ? t.wHour - 12 : t.wHour;
            const std::string date = std::format("{}/{}/{:02} {:2}:{:02}", t.wMonth, t.wDay, t.wYear, hour, t.wMinute);
            if (m_replayObject != nullptr) m_replayObject->save(*m_source, date);
        } else {
            game.buildProfile(*m_source);
        }
        m_side.ready = 1;
    }
    if (m_side.emptySource != 0) {
        m_source = std::make_unique<SaveSource>();
        m_side.ready = 1;
    }
    const DeviceState state = managerStep(request, game);
    // 0x004FA2B0 / 0x004FA810 / 0x004FAAC0
    if (request.flag(SaveFlag::Loaded) != 0 && m_source) {
        bool ok = false;
        if (kind == Kind::HighScores) {
            ok = game.applyHighScores(*m_source);
        } else if (kind == Kind::Replay) {
            ok = m_replayObject != nullptr && m_replayObject->take(*m_source);
        } else if (request.slot >= 0 && static_cast<size_t>(request.slot) < m_slots.size()) {
            ok = game.applyProfile(m_slots[static_cast<size_t>(request.slot)], *m_source);
        }
        m_source.reset();
        if (!ok) {
            request.flag(SaveFlag::Corrupt) = 1;
            request.flag(SaveFlag::Loaded) = 0;
            reset();
        }
    }
    request.scale = 1.0f;
    return state;
}

DeviceState SaveDevice::managerStep(SaveRequest& request, SaveGameSide& game) {
    request.flags.fill(0);
    DeviceState next = DeviceState::Idle;
    switch (m_state) {
    case DeviceState::Idle: next = idle(request); break;
    case DeviceState::List: next = list(request); break;
    case DeviceState::Read: next = read(request, game); break;
    default: next = write(request, game); break;
    }
    m_state = next;
    request.next = 0;
    request.done = 0;
    request.remove = 0;
    request.request = Request::None;
    request.answer = -1;
    return m_state;
}

DeviceState SaveDevice::idle(SaveRequest& request) {
    request.idleMarks = {1, 1};
    if (request.request == Request::List)
        return m_side.kind == Kind::HighScores ? DeviceState::Read : DeviceState::List;
    if (request.request == Request::Write) return DeviceState::Write;
    return DeviceState::Idle;
}

std::wstring SaveDevice::slotFolder(const SaveRequest& request) const {
    if (m_side.kind == Kind::HighScores) return L"HS";
    if (m_side.kind == Kind::Replay) {
        if (request.slot >= 0 && static_cast<size_t>(request.slot) < m_replays.size()) {
            return m_replays[static_cast<size_t>(request.slot)].folder;
        }
        return std::wstring();
    }
    if (request.slot >= 0 && static_cast<size_t>(request.slot) < m_slots.size()) {
        return m_slots[static_cast<size_t>(request.slot)].folder;
    }
    return std::wstring();
}

DeviceState SaveDevice::list(SaveRequest& request) {
    ListState& s = m_list;
    switch (s.step) {
    case ListStep::First: {
        s.count = 0;
        s.none = 1;
        s.sorted = 0;
        s.slots.clear();
        s.replays.clear();
        // 0x005027B0: the first folder of SAVE not starting with '.'.
        s.names.clear();
        s.at = 0;
        WIN32_FIND_DATAW find{};
        const HANDLE f = FindFirstFileW((withSlash(m_save) + L"*").c_str(), &find);
        DWORD error = GetLastError();
        if (f != INVALID_HANDLE_VALUE) {
            do {
                s.names.push_back(find.cFileName);
            } while (FindNextFileW(f, &find));
            error = GetLastError();
            FindClose(f);
        }
        while (s.at < s.names.size() && s.names[s.at][0] == L'.') ++s.at;
        s.finding = s.at < s.names.size() ? 1 : 0;
        if (s.finding != 0) {
            s.current = s.names[s.at];
            s.step = ListStep::Next;
        } else {
            s.step = error == ERROR_FILE_NOT_FOUND ? ListStep::Next : ListStep::End;
        }
        return DeviceState::List;
    }
    case ListStep::Next: {
        if (s.finding != 0) {
            if (m_side.kind == Kind::Profile && startsWith(s.current, L"P")) {
                s.none = 0;
                SaveSlot slot;
                if (readUserData(withSlash(m_save) + s.current, slot)) {
                    slot.folder = s.current;
                    slot.index = s.count;
                    s.slots.push_back(slot);
                    ++s.count;
                }
            } else if (m_side.kind == Kind::Replay && startsWith(s.current, L"R")) {
                s.none = 0;
                ReplaySlot slot;
                if (readReplayData(withSlash(m_save) + s.current, slot)) {
                    slot.folder = s.current;
                    slot.index = s.count;
                    s.replays.push_back(slot);
                    ++s.count;
                }
            } else if (m_side.kind == Kind::HighScores && startsWith(s.current, L"HS")) {
                ++s.count;
            }
        }
        // 0x00502940: the next folder; at the end ERROR_NO_MORE_FILES ends the list.
        ++s.at;
        s.finding = s.at < s.names.size() ? 1 : 0;
        if (s.finding == 0) {
            s.step = ListStep::End;
            return DeviceState::List;
        }
        s.current = s.names[s.at];
        if (m_side.kind == Kind::Profile && s.count >= 11) s.step = ListStep::End;
        return DeviceState::List;
    }
    case ListStep::Delete: {
        if (request.answer == -1) {
            request.flag(SaveFlag::Asking) = 1;
            request.flag(SaveFlag::DeleteAsk) = 1;
            return DeviceState::List;
        }
        if (request.answer == 0) {
            s.step = ListStep::End;
            return DeviceState::List;
        }
        if (deleteFolder(withSlash(m_save) + slotFolder(request))) {
            request.flag(SaveFlag::Deleted) = 1;
        } else {
            request.flag(SaveFlag::DeleteError) = 1;
        }
        s.step = ListStep::First;
        return DeviceState::List;
    }
    case ListStep::End: {
        if (s.none != 0) {
            m_found = 0;
            request.flag(SaveFlag::NoData) = 1;
            request.flag(SaveFlag::End) = 1;
            if (request.next == 0) return DeviceState::List;
            s.step = ListStep::First;
            return DeviceState::Idle;
        }
        if (s.sorted == 0) {
            // 0x004FF290: std::sort, the latest USERDATA first.
            std::stable_sort(s.slots.begin(), s.slots.end(),
                             [](const SaveSlot& a, const SaveSlot& b) { return a.written > b.written; });
            for (size_t i = 0; i < s.slots.size(); ++i) s.slots[i].index = static_cast<uint32_t>(i);
            m_slots = s.slots;
            // 0x004FF300: the replays likewise.
            std::stable_sort(s.replays.begin(), s.replays.end(),
                             [](const ReplaySlot& a, const ReplaySlot& b) { return a.written > b.written; });
            for (size_t i = 0; i < s.replays.size(); ++i) s.replays[i].index = static_cast<uint32_t>(i);
            m_replays = s.replays;
            s.sorted = 1;
        }
        m_found = static_cast<int32_t>(s.count);
        request.flag(SaveFlag::Listed) = 1;
        request.flag(SaveFlag::End) = 1;
        request.flag(SaveFlag::ListEnd) = 1;
        if (request.done != 0) {
            s.step = ListStep::First;
            return DeviceState::Idle;
        }
        if (request.next != 0) {
            s.step = ListStep::First;
            return DeviceState::Read;
        }
        if (request.remove != 0) s.step = ListStep::Delete;
        return DeviceState::List;
    }
    default: return DeviceState::List;
    }
}

DeviceState SaveDevice::read(SaveRequest& request, SaveGameSide& game) {
    ReadState& s = m_read;
    switch (s.step) {
    case ReadStep::Find: {
        if (m_side.ready == 0) {
            m_side.emptySource = 1;
            return DeviceState::Read;
        }
        m_side.emptySource = 0;
        s.folder = slotFolder(request);
        s.ok = 0;
        s.failed = 0;
        s.files.clear();
        s.at = 0;
        WIN32_FIND_DATAW find{};
        const HANDLE f = FindFirstFileW((withSlash(m_save) + s.folder + L"\\*").c_str(), &find);
        if (f == INVALID_HANDLE_VALUE) {
            s.failed = m_side.kind == Kind::HighScores ? 0 : 1;
            s.step = ReadStep::End;
            return DeviceState::Read;
        }
        do {
            if (find.cFileName[0] != L'.') s.files.push_back(find.cFileName);
        } while (FindNextFileW(f, &find));
        FindClose(f);
        if (s.files.empty()) {
            s.failed = 1;
            s.step = ReadStep::End;
        } else {
            s.step = ReadStep::File;
        }
        return DeviceState::Read;
    }
    case ReadStep::File: {
        const std::wstring name = s.files[s.at];
        if (m_side.kind == Kind::Profile) game.readRegCf1();
        const std::wstring path = withSlash(m_save) + s.folder + L"\\" + name;
        const HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                     nullptr);
        bool read = false;
        if (h != INVALID_HANDLE_VALUE) {
            const DWORD size = GetFileSize(h, nullptr);
            if (size <= 0x100000) {
                std::vector<uint8_t> bytes(size);
                DWORD got = 0;
                ReadFile(h, bytes.data(), size, &got, nullptr);
                if (m_source) {
                    std::string key(name.size(), '\0');
                    for (size_t i = 0; i < name.size(); ++i) key[i] = static_cast<char>(name[i]);
                    std::vector<uint8_t>& file = m_source->files[key];
                    file.insert(file.end(), bytes.begin(), bytes.end());
                }
                read = true;
            }
            CloseHandle(h);
        }
        if (!read) {
            s.failed = 1;
            s.step = ReadStep::End;
        }
        ++s.at;
        if (s.at == s.files.size()) {
            s.ok = 1;
            s.step = ReadStep::End;
        }
        return DeviceState::Read;
    }
    default: {
        if (s.ok != 0) {
            request.flag(SaveFlag::Loaded) = 1;
        } else if (s.failed != 0) {
            request.flag(SaveFlag::Corrupt) = 1;
        } else {
            request.flag(SaveFlag::ReadError) = 1;
        }
        request.flag(SaveFlag::End) = 1;
        if (request.next == 0) return DeviceState::Read;
        s.step = ReadStep::Find;
        return DeviceState::Idle;
    }
    }
}

DeviceState SaveDevice::write(SaveRequest& request, SaveGameSide& game) {
    WriteState& s = m_write;
    switch (s.step) {
    case WriteStep::Source:
        s.ok = 0;
        if (m_side.ready == 0) {
            m_side.buildSource = 1;
            return DeviceState::Write;
        }
        m_side.buildSource = 0;
        s.step = WriteStep::Folder;
        return DeviceState::Write;
    case WriteStep::Folder: {
        s.folder =
            m_side.kind == Kind::HighScores ? std::wstring(L"HS") : (m_source ? m_source->folder : std::wstring());
        WIN32_FIND_DATAW find{};
        const HANDLE f = FindFirstFileW((withSlash(m_save) + s.folder).c_str(), &find);
        if (f != INVALID_HANDLE_VALUE) {
            FindClose(f);
            s.step = m_side.kind == Kind::HighScores ? WriteStep::Begin : WriteStep::Overwrite;
        } else {
            s.step = WriteStep::Begin;
        }
        return DeviceState::Write;
    }
    case WriteStep::Overwrite:
        if (request.answer == 0) {
            s.step = WriteStep::End;
        } else if (request.answer == 1) {
            s.step = WriteStep::Begin;
        } else {
            request.flag(SaveFlag::Exists) = 1;
            request.flag(SaveFlag::Asking) = 1;
        }
        return DeviceState::Write;
    case WriteStep::Begin:
        s.at = 0;
        s.step = WriteStep::MakeFolder;
        return DeviceState::Write;
    case WriteStep::MakeFolder: {
        request.flag(SaveFlag::Saving) = 1;
        std::wstring made;
        if (!makeFolder(m_save, s.folder, made)) {
            s.step = WriteStep::End;
            return DeviceState::Write;
        }
        s.folder = made;
        s.step = WriteStep::Files;
        return DeviceState::Write;
    }
    case WriteStep::Files: {
        const size_t count = m_source ? m_source->files.size() : 0;
        if (s.at >= count) {
            request.flag(SaveFlag::Saving) = 0;
            s.ok = 1;
            s.step = WriteStep::End;
            return DeviceState::Write;
        }
        auto it = m_source->files.begin();
        std::advance(it, static_cast<std::ptrdiff_t>(s.at));
        // 0x00502D20: reg.cf1, then the file made anew.
        game.writeRegCf1();
        const std::wstring name(it->first.begin(), it->first.end());
        const HANDLE h = CreateFileW((s.folder + L"\\" + name).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                     FILE_ATTRIBUTE_NORMAL, nullptr);
        BOOL written = FALSE;
        if (h != INVALID_HANDLE_VALUE) {
            DWORD done = 0;
            written = WriteFile(h, it->second.data(), static_cast<DWORD>(it->second.size()), &done, nullptr);
            CloseHandle(h);
        }
        if (!written) {
            s.step = WriteStep::End;
            return DeviceState::Write;
        }
        ++s.at;
        return DeviceState::Write;
    }
    default:
        m_source.reset();
        if (s.ok == 0) {
            s.step = WriteStep::Source;
            return DeviceState::Idle;
        }
        request.flag(SaveFlag::Saved) = 1;
        request.flag(SaveFlag::End) = 1;
        if (request.next == 0) return DeviceState::Write;
        s.step = WriteStep::Source;
        return DeviceState::Idle;
    }
}

}  // namespace oj::save
