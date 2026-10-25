#pragma once
// What the functions of Windows 8 the toolset's C++ library calls directly do on Windows 7, with the functions it
// has: the executable's import slots of CreateFile2, CopyFile2 and GetSystemTimePreciseAsFileTime (Win7Imports.cpp)
// hold these on every Windows.

#include <cstdint>

namespace oj::win7 {

// CREATEFILE2_EXTENDED_PARAMETERS (fileapi.h, declared for Windows 8 and later only).
struct CreateFile2Parameters {
    uint32_t size = sizeof(CreateFile2Parameters);
    uint32_t attributes = 0;
    uint32_t flags = 0;
    uint32_t securityQosFlags = 0;
    void* security = nullptr;  // LPSECURITY_ATTRIBUTES
    void* templateFile = nullptr;
};

// CreateFile2 as CreateFileW: the same name, access, sharing and disposition; the attributes, the flags and the quality
// of service flags in one word, the security attributes and the template as given (none without parameters). The
// handle, INVALID_HANDLE_VALUE with the last error set on a failure.
void* createFile2(const wchar_t* name, uint32_t access, uint32_t share, uint32_t disposition,
                  const CreateFile2Parameters* parameters);

// COPYFILE2_EXTENDED_PARAMETERS (winbase.h, declared for Windows 8 and later only).
struct CopyFile2Parameters {
    uint32_t size = sizeof(CopyFile2Parameters);
    uint32_t copyFlags = 0;
    int32_t* cancel = nullptr;         // BOOL*
    void* progressRoutine = nullptr;   // PCOPYFILE2_PROGRESS_ROUTINE
    void* callbackContext = nullptr;
};

// CopyFile2 as CopyFileExW: the copy flags CopyFileExW takes on Windows 7 (COPY_FILE_FAIL_IF_EXISTS, _RESTARTABLE,
// _OPEN_SOURCE_FOR_WRITE, _ALLOW_DECRYPTED_DESTINATION, _COPY_SYMLINK, _NO_BUFFERING) and the cancel flag; a progress
// routine, of another form than CopyFileExW's, is not called (the C++ library gives none). S_OK, else the error as an
// HRESULT (HRESULT_FROM_WIN32).
int32_t copyFile2(const wchar_t* existing, const wchar_t* copy, const CopyFile2Parameters* parameters);

// GetSystemTimePreciseAsFileTime as GetSystemTimeAsFileTime: the time of the system's last tick, in 100 ns since
// 1601 (a FILETIME). Only std::chrono::system_clock reads it, which the game does not (its log and saves take
// GetLocalTime, its waits the steady clock).
void systemTimePrecise(void* fileTime);

}  // namespace oj::win7
