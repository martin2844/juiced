#pragma once
#include "windows.h"
inline DWORD timeGetTime() {
    return GetTickCount();
}
