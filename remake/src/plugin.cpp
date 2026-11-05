// SPDX-License-Identifier: GPL-3.0-only
#include "catalog.h"
#include "lifecycle.h"

#include <Windows.h>
#include <Debug.h>

#include <cerrno>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace
{
    const char moduleMarker = 0;
    volatile LONG startupClaimed = 0;
    volatile LONG duplicateStartupReported = 0;

    std::runtime_error WindowsFailure(const char* operation, DWORD code)
    {
        std::ostringstream message;
        message << operation << " failed (Win32 error " << code << ')';
        return std::runtime_error(message.str());
    }

    std::wstring ModuleDirectory()
    {
        HMODULE module = NULL;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCWSTR>(&moduleMarker), &module))
            throw WindowsFailure("GetModuleHandleExW", GetLastError());

        std::vector<wchar_t> buffer(512);
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(module, &buffer[0],
                                                   static_cast<DWORD>(buffer.size()));
            if (length == 0)
                throw WindowsFailure("GetModuleFileNameW", GetLastError());
            if (length < buffer.size())
            {
                const std::wstring filename(&buffer[0], length);
                const size_t separator = filename.find_last_of(L"\\");
                if (separator == std::wstring::npos)
                    throw std::runtime_error("module filename has no directory");
                return filename.substr(0, separator + 1);
            }
            if (buffer.size() == 32768)
                throw std::runtime_error("module filename exceeds the Windows path limit");
            buffer.resize(buffer.size() * 2);
        }
    }

    std::string Utf8(const std::wstring& text)
    {
        const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, NULL, 0, NULL, NULL);
        if (size == 0)
            throw WindowsFailure("WideCharToMultiByte", GetLastError());
        std::vector<char> buffer(size);
        if (!WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &buffer[0], size, NULL, NULL))
            throw WindowsFailure("WideCharToMultiByte", GetLastError());
        return std::string(&buffer[0]);
    }

    void Bootstrap()
    {
        DebugLog("ProjectCarsRemake FOUNDATION (development only): no drivable gameplay yet; "
                 "no gameplay hooks or SDK initialization; lifecycle observation requires explicit opt-in.");
        const std::wstring directory = ModuleDirectory();
        const std::wstring path = directory + L"vehicles.catalog";
        const std::string source = Utf8(path);
        errno = 0;
        std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
        if (!input)
        {
            const int code = errno;
            std::ostringstream message;
            message << "cannot open module-local catalog '" << source << "' (CRT errno " << code << ')';
            throw std::runtime_error(message.str());
        }

        const projectcars::Catalog catalog = projectcars::ParseCatalog(input, source);
        if (catalog.parts.empty() || catalog.frames.empty())
            throw std::runtime_error(source + ": foundation bootstrap requires at least one part and frame");
        std::ostringstream message;
        message << "Foundation catalog validated: " << catalog.parts.size() << " parts, "
                << catalog.frames.size() << " frames, " << catalog.slots.size() << " slots, "
                << catalog.buildings.size() << " building mappings. Declarations only; "
                << "no FCS lookup, vehicle creation or gameplay enabled.";
        const std::string summary = message.str();
        DebugLog(summary.c_str());
        projectcars::StartNativeLifecycle(directory);
    }

    void ReportStartupFailure(const char* detail) throw()
    {
        char message[4096];
        _snprintf_s(message, sizeof(message), _TRUNCATE,
                    "ProjectCarsRemake FOUNDATION STARTUP FAILED: %s. No gameplay enabled.", detail);
        try
        {
            ErrorLog(message);
        }
        catch (...)
        {
            // The SDK logger can allocate; failure reporting must not escape the host boundary.
            OutputDebugStringA("ProjectCarsRemake: SDK ErrorLog failed; displaying startup failure.\n");
            OutputDebugStringA(message);
            MessageBoxA(NULL, message, "ProjectCarsRemake startup failed (SDK logging unavailable)",
                        MB_OK | MB_ICONERROR | MB_TASKMODAL);
        }
    }
}

__declspec(dllexport) void startPlugin()
{
    if (InterlockedCompareExchange(&startupClaimed, 1, 0) != 0)
    {
        if (InterlockedCompareExchange(&duplicateStartupReported, 1, 0) == 0)
            OutputDebugStringA("ProjectCarsRemake: duplicate startPlugin refused; "
                               "no lifecycle retry, hot reload or replacement.\n");
        return;
    }
    try
    {
        Bootstrap();
    }
    catch (const std::exception& error)
    {
        ReportStartupFailure(error.what());
    }
    catch (...)
    {
        ReportStartupFailure("unknown non-standard C++ exception");
    }
}
