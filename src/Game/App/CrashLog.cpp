// The crashes written down: to OpenJuice.log with the stack, and as the stock game writes crash.txt.

#include "Game/App/CrashLog.h"

#include "Core/Base/Log.h"
#include "Game/Race/RaceCamera.h"

#include <dbghelp.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace app {

namespace {

// 0x0059AF00's sibling 0x004FB590: a line printed (vsprintf into the game's buffer [0x0079EDF8]) and written to a file.
void writeCrashLine(HANDLE file, const char* format, ...) {
    static char buffer[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    DWORD written = 0;
    if (file != INVALID_HANDLE_VALUE) WriteFile(file, buffer, static_cast<DWORD>(std::strlen(buffer)), &written, nullptr);
}

}  // namespace

// A crash written to the log: the exception, then the frames of its stack by the program's symbols.
LONG WINAPI logCrash(EXCEPTION_POINTERS* e) {
    oj::log::error("crash: exception {:#x} at {}", e->ExceptionRecord->ExceptionCode,
                   static_cast<const void*>(e->ExceptionRecord->ExceptionAddress));
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    SymInitialize(process, nullptr, TRUE);
    CONTEXT context = *e->ContextRecord;
    STACKFRAME64 frame{};
#ifdef _WIN64
    const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context.Rip;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrStack.Offset = context.Rsp;
#else
    const DWORD machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context.Eip;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrStack.Offset = context.Esp;
#endif
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;
    for (int i = 0; i < 32; ++i) {
        if (!StackWalk64(machine, process, thread, &frame, &context, nullptr, SymFunctionTableAccess64,
                         SymGetModuleBase64, nullptr) ||
            frame.AddrPC.Offset == 0) {
            break;
        }
        alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256]{};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;
        DWORD64 offset = 0;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD column = 0;
        const bool named = SymFromAddr(process, frame.AddrPC.Offset, &offset, symbol) != FALSE;
        const bool lined = SymGetLineFromAddr64(process, frame.AddrPC.Offset, &column, &line) != FALSE;
        oj::log::error("crash:   {} +{:#x} {}:{}", named ? symbol->Name : "?", offset,
                       lined ? line.FileName : "", lined ? line.LineNumber : 0);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// The unhandled exceptions' filter of the game's main function (0x005A34D0, not a Ghidra function; set by 0x005A3740):
// crash.txt in the game's folder (0x004F9BE0: the folder less one \ and one / at its end; its file "%s\%s" opened
// to write, 0x004FD540 mode 1: CreateFileA with GENERIC_WRITE, no sharing, CREATE_ALWAYS, normal and sequential, a
// missing folder offered to be made, "Create directory?"), each line written as it is printed: the exception's code,
// flags, address, count of parameters and its first two, the registers and WinMain's address. The search goes on (0).
// x64: the pointers and the registers of the same names (Rdi .. Rsp, Rip for Eip) as their low 32 bits, as %x prints.
// The stock record first, then OpenJuice's own in OpenJuice.log (logCrash, the port's diagnostics).
LONG WINAPI writeCrashTxt(EXCEPTION_POINTERS* e) {
    if (e == nullptr) return EXCEPTION_CONTINUE_SEARCH;
    std::string folder = g_gameFolder;
    if (!folder.empty() && folder.back() == '\\') folder.pop_back();
    if (!folder.empty() && folder.back() == '/') folder.pop_back();
    const std::string path = folder.empty() ? std::string("crash.txt") : folder + "\\crash.txt";
    HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PATH_NOT_FOUND) {
        std::string directory = path;
        const size_t slash = directory.find_last_of("/\\");
        if (slash != std::string::npos) directory.resize(slash);
        if (MessageBoxA(nullptr, directory.c_str(), "Create directory?", MB_YESNO) == IDYES) {
            CreateDirectoryA(directory.c_str(), nullptr);
            file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        }
    }
    const EXCEPTION_RECORD& r = *e->ExceptionRecord;
    const CONTEXT& c = *e->ContextRecord;
    auto low = [](uint64_t v) { return static_cast<uint32_t>(v); };
    writeCrashLine(file, "ExceptionCode = 0x%x\r\n", static_cast<uint32_t>(r.ExceptionCode));
    writeCrashLine(file, "ExceptionFlags = %d\r\n", static_cast<int32_t>(r.ExceptionFlags));
    writeCrashLine(file, "ExceptionAddress = 0x%x\r\n", low(reinterpret_cast<uintptr_t>(r.ExceptionAddress)));
    writeCrashLine(file, "ExceptionNumParameters = %d\r\n", static_cast<int32_t>(r.NumberParameters));
    writeCrashLine(file, "ExceptionInformation[0] = 0x%x\r\n", low(r.ExceptionInformation[0]));
    writeCrashLine(file, "ExceptionInformation[1] = 0x%x\r\n", low(r.ExceptionInformation[1]));
#ifdef _WIN64
    writeCrashLine(file, "Edi = 0x%x\r\n", low(c.Rdi));
    writeCrashLine(file, "Esi = 0x%x\r\n", low(c.Rsi));
    writeCrashLine(file, "Ebx = 0x%x\r\n", low(c.Rbx));
    writeCrashLine(file, "Edx = 0x%x\r\n", low(c.Rdx));
    writeCrashLine(file, "Ecx = 0x%x\r\n", low(c.Rcx));
    writeCrashLine(file, "Eax = 0x%x\r\n", low(c.Rax));
    writeCrashLine(file, "Ebp = 0x%x\r\n", low(c.Rbp));
    writeCrashLine(file, "Eip = 0x%x\r\n", low(c.Rip));
#else
    writeCrashLine(file, "Edi = 0x%x\r\n", low(c.Edi));
    writeCrashLine(file, "Esi = 0x%x\r\n", low(c.Esi));
    writeCrashLine(file, "Ebx = 0x%x\r\n", low(c.Ebx));
    writeCrashLine(file, "Edx = 0x%x\r\n", low(c.Edx));
    writeCrashLine(file, "Ecx = 0x%x\r\n", low(c.Ecx));
    writeCrashLine(file, "Eax = 0x%x\r\n", low(c.Eax));
    writeCrashLine(file, "Ebp = 0x%x\r\n", low(c.Ebp));
    writeCrashLine(file, "Eip = 0x%x\r\n", low(c.Eip));
#endif
    writeCrashLine(file, "SegCs = 0x%x\r\n", static_cast<uint32_t>(c.SegCs));
    writeCrashLine(file, "EFlags = 0x%x\r\n", static_cast<uint32_t>(c.EFlags));
#ifdef _WIN64
    writeCrashLine(file, "Esp = 0x%x\r\n", low(c.Rsp));
#else
    writeCrashLine(file, "Esp = 0x%x\r\n", low(c.Esp));
#endif
    writeCrashLine(file, "SegSs = 0x%x\r\n", static_cast<uint32_t>(c.SegSs));
    writeCrashLine(file, "WinMain = 0x%x\r\n", low(reinterpret_cast<uintptr_t>(&WinMain)));
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    logCrash(e);
    return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace app
