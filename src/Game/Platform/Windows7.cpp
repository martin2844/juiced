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

}  // namespace oj::win7
