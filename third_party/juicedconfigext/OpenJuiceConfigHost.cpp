// The fixed build's OpenJuiceConfig: JuicedConfigExt's settings program (external/JuicedConfigExt/src/Config) with
// OpenJuice as its host (its Core/Host.h): the patch's name and version, OpenJuice's executables, the controller's and
// the wheel's options in controller.ini, and the settings the fixed build's game reads so far: the display (the
// monitor's adapter, full screen or windowed, the resolution, the shaders, the textures' sharpness, the picture's shape,
// the threading), the sound (EAX always there: OpenAL Soft), the controller and its layout, the steering wheels (the
// wheel map, the dead zone, the rotation, the response, the force feedback) and MultiTurn. The effects, the
// anti-aliasing and the texture filter are the game's own graphics options' (their ADVANCED page), not here.

#include "Core/Host.h"

#include "Game/App/Version.h"

#include <LayoutFile.h>

#include <windows.h>

#include <vector>

#pragma comment(lib, "version.lib")

namespace Host
{
    namespace
    {
        bool IsFile(const std::wstring& path)
        {
            const DWORD attributes = GetFileAttributesW(path.c_str());
            return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
        }

        // Windows on an x64 processor (asked by a 32-bit program too); not on ARM64, whose Windows 10 runs no x64
        // programs.
        bool WindowsX64()
        {
            SYSTEM_INFO system{};
            GetNativeSystemInfo(&system);
            return system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;
        }

        // An executable whose version resource names OpenJuice (Juiced.exe when OpenJuice took its place).
        bool IsOpenJuice(const std::wstring& path)
        {
            DWORD handle = 0;
            const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
            if (size == 0)
            {
                return false;
            }
            std::vector<char> data(size);
            wchar_t* name = nullptr;
            UINT length = 0;
            return GetFileVersionInfoW(path.c_str(), 0, size, data.data()) &&
                   VerQueryValueW(data.data(), L"\\StringFileInfo\\040904b0\\ProductName", reinterpret_cast<void**>(&name),
                                  &length) &&
                   name && std::wstring(name) == L"OpenJuice";
        }
    }

    const Features& Supports()
    {
        static const Features features = [] {
            Features f;
            f.pluginFiles = false;
            f.borderless = true;
            f.frameRate = true;
            // The anti-aliasing and the texture filter are set in the game too (the same page): their values kept.
            f.msaa = false;
            f.smaa = false;
            f.textureFilter = false;
            f.lodBias = true;
            f.fpsCounter = true;
            f.aspectList = true;
            f.enhancedSound = false;
            f.directInputChoice = false;
            f.rumble = true;
            f.consoleSteering = true;
            // The effects are set in the game (its graphics options' ADVANCED page): no Effects page here.
            f.carPaint = false;
            f.worldLight = false;
            f.headlights = false;
            f.heatHaze = false;
            // EAX is set in the game (its audio options), the volumes there too; the hardware's mixing does nothing
            // through OpenAL Soft: no Audio page here (the user, 2026-10-29).
            f.audioPage = false;
            return f;
        }();
        return features;
    }

    bool OpenJuice()
    {
        return true;
    }

    const wchar_t* Name()
    {
        return L"OpenJuiceConfig";
    }

    const wchar_t* Version()
    {
        return L"" OJ_VERSION_TEXT;
    }

    const wchar_t* HeaderFirst()
    {
        return L"OPENJUICE";
    }

    const wchar_t* HeaderSecond()
    {
        return L"CONFIG";
    }

    // OpenJuice of the program's bitness, else the other; Juiced.exe when it is OpenJuice's.
    std::wstring GameProgram(const std::wstring& folder)
    {
        // The 64-bit game on Windows of x64 when it is there, else the 32-bit one.
        std::vector<const wchar_t*> order;
        if (WindowsX64())
        {
            order.push_back(L"OpenJuice64.exe");
        }
        order.push_back(L"OpenJuice.exe");
        for (const wchar_t* name : order)
        {
            const std::wstring path = folder + L"\\" + name;
            if (IsFile(path))
            {
                return path;
            }
        }
        const std::wstring renamed = folder + L"\\Juiced.exe";
        return IsFile(renamed) && IsOpenJuice(renamed) ? renamed : std::wstring();
    }

    std::wstring OptionsFile(const std::wstring&)
    {
        return LayoutFile::Path();
    }
}
