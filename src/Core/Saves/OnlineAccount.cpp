#include "Core/Saves/OnlineAccount.h"

#include "Core/Saves/SaveEntries.h"

#include <windows.h>
#ifdef _WIN32
#include <dpapi.h>
#endif

#include <cstring>

#ifdef _MSC_VER
#pragma comment(lib, "crypt32.lib")
#endif

namespace oj::save {

namespace {

constexpr uint32_t kVersion = 1;
constexpr wchar_t kDescription[] = L"OpenJuice online account";

void putU32(std::vector<uint8_t>& out, uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(value >> (8 * i)));
}

void putText(std::vector<uint8_t>& out, const std::wstring& text) {
    putU32(out, static_cast<uint32_t>(text.size()));
    for (wchar_t c : text) {
        out.push_back(static_cast<uint8_t>(c & 0xFF));
        out.push_back(static_cast<uint8_t>((c >> 8) & 0xFF));
    }
}

struct Reader {
    const uint8_t* at;
    const uint8_t* end;
    bool u32(uint32_t& value) {
        if (end - at < 4) return false;
        value = static_cast<uint32_t>(at[0]) | (static_cast<uint32_t>(at[1]) << 8) |
                (static_cast<uint32_t>(at[2]) << 16) | (static_cast<uint32_t>(at[3]) << 24);
        at += 4;
        return true;
    }
    bool text(std::wstring& value) {
        uint32_t size = 0;
        if (!u32(size) || size > 0x100 || static_cast<size_t>(end - at) < size * 2u) return false;
        value.clear();
        for (uint32_t i = 0; i < size; ++i, at += 2) {
            value.push_back(static_cast<wchar_t>(at[0] | (at[1] << 8)));
        }
        return true;
    }
};

}  // namespace

OnlineAccount OnlineAccount::of(const Settings& settings) {
    return {settings.wideText(Settings::kOnlineNick, 31), settings.wideText(Settings::kOnlineEmail, 51),
            settings.wideText(Settings::kOnlinePassword, 31)};
}

void OnlineAccount::into(Settings& settings) const {
    settings.setWideText(Settings::kOnlineNick, nick, 31);
    settings.setWideText(Settings::kOnlineEmail, email, 51);
    settings.setWideText(Settings::kOnlinePassword, password, 31);
}

std::vector<uint8_t> protectAccount(const OnlineAccount& account) {
#ifndef _WIN32
    // DPAPI blobs are Windows-user-bound. Never fall back to storing plaintext.
    return {};
#else
    std::vector<uint8_t> plain;
    putU32(plain, kVersion);
    putText(plain, account.nick);
    putText(plain, account.email);
    putText(plain, account.password);
    DATA_BLOB in{static_cast<DWORD>(plain.size()), plain.data()};
    DATA_BLOB out{};
    const BOOL done = CryptProtectData(&in, kDescription, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out);
    SecureZeroMemory(plain.data(), plain.size());
    if (!done) return {};
    std::vector<uint8_t> blob(out.pbData, out.pbData + out.cbData);
    LocalFree(out.pbData);
    return blob;
#endif
}

std::optional<OnlineAccount> unprotectAccount(const std::vector<uint8_t>& entry) {
#ifndef _WIN32
    return std::nullopt;
#else
    if (entry.empty()) return std::nullopt;
    DATA_BLOB in{static_cast<DWORD>(entry.size()), const_cast<BYTE*>(entry.data())};
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return std::nullopt;
    }
    Reader r{out.pbData, out.pbData + out.cbData};
    uint32_t version = 0;
    OnlineAccount account;
    const bool ok = r.u32(version) && version == kVersion && r.text(account.nick) && r.text(account.email) &&
                    r.text(account.password);
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    if (!ok) return std::nullopt;
    return account;
#endif
}

}  // namespace oj::save
