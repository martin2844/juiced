#include "Game/Platform/Windows7.h"

#include <windows.h>

namespace oj::win7 {

void* createFile2(const wchar_t* name, uint32_t access, uint32_t share, uint32_t disposition,
                  const CreateFile2Parameters* parameters) {
    if (parameters == nullptr) {
        return CreateFileW(name, access, share, nullptr, disposition, 0, nullptr);
    }
    return CreateFileW(name, access, share, static_cast<LPSECURITY_ATTRIBUTES>(parameters->security), disposition,
                       parameters->attributes | parameters->flags | parameters->securityQosFlags,
                       static_cast<HANDLE>(parameters->templateFile));
}

int32_t copyFile2(const wchar_t* existing, const wchar_t* copy, const CopyFile2Parameters* parameters) {
    constexpr DWORD kWindows7Flags = COPY_FILE_FAIL_IF_EXISTS | COPY_FILE_RESTARTABLE |
                                     COPY_FILE_OPEN_SOURCE_FOR_WRITE | COPY_FILE_ALLOW_DECRYPTED_DESTINATION |
                                     COPY_FILE_COPY_SYMLINK | COPY_FILE_NO_BUFFERING;
    const DWORD flags = parameters != nullptr ? parameters->copyFlags & kWindows7Flags : 0;
    BOOL* cancel = parameters != nullptr ? reinterpret_cast<BOOL*>(parameters->cancel) : nullptr;
    if (CopyFileExW(existing, copy, nullptr, nullptr, cancel, flags)) {
        return S_OK;
    }
    return HRESULT_FROM_WIN32(GetLastError());
}

void systemTimePrecise(void* fileTime) {
    GetSystemTimeAsFileTime(static_cast<FILETIME*>(fileTime));
}

namespace {

SRWLOCK g_waitLock = SRWLOCK_INIT;
CONDITION_VARIABLE g_woken = CONDITION_VARIABLE_INIT;

bool sameBytes(volatile void* address, const void* compare, size_t size) {
    switch (size) {
    case 1: return *static_cast<volatile uint8_t*>(address) == *static_cast<const uint8_t*>(compare);
    case 2: return *static_cast<volatile uint16_t*>(address) == *static_cast<const uint16_t*>(compare);
    case 4: return *static_cast<volatile uint32_t*>(address) == *static_cast<const uint32_t*>(compare);
    case 8: return *static_cast<volatile uint64_t*>(address) == *static_cast<const uint64_t*>(compare);
    default: return false;
    }
}

}  // namespace

bool waitOnAddress(volatile void* address, const void* compare, size_t size, uint32_t milliseconds) {
    const ULONGLONG start = GetTickCount64();
    AcquireSRWLockExclusive(&g_waitLock);
    bool changed = true;
    while (sameBytes(address, compare, size)) {
        DWORD left = INFINITE;
        if (milliseconds != INFINITE) {
            const ULONGLONG spent = GetTickCount64() - start;
            if (spent >= milliseconds) {
                changed = false;
                break;
            }
            left = static_cast<DWORD>(milliseconds - spent);
        }
        if (!SleepConditionVariableSRW(&g_woken, &g_waitLock, left, 0) && GetLastError() == ERROR_TIMEOUT) {
            changed = !sameBytes(address, compare, size);
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_waitLock);
    if (!changed) SetLastError(ERROR_TIMEOUT);
    return changed;
}

void wakeByAddressAll(void*) {
    // The lock taken and let go first: a waiter between its look at the bytes and its sleep is woken too.
    AcquireSRWLockExclusive(&g_waitLock);
    ReleaseSRWLockExclusive(&g_waitLock);
    WakeAllConditionVariable(&g_woken);
}

}  // namespace oj::win7
