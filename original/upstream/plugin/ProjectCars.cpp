#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Debug.h>
#include <fstream>
#include <sstream>
#include "../src/Vehicle.h"
#include "NativeRuntime.h"
#include "OilProduction.h"

namespace {
cars::Catalog g_catalog;
int g_moduleAnchor = 0;
bool g_started = false;
bool loadCatalog(std::string& error) {
    HMODULE module = 0;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&g_moduleAnchor), &module)) {
        error = "Cannot locate plugin module"; return false;
    }
    wchar_t path[32768];
    DWORD length = GetModuleFileNameW(module, path, 32768);
    if (!length || length >= 32768) { error = "Cannot resolve plugin directory"; return false; }
    std::wstring file(path, length);
    size_t slash = file.find_last_of(L"\\/");
    if (slash == std::wstring::npos) { error = "Plugin directory missing"; return false; }
    file = file.substr(0, slash + 1) + L"vehicles.catalog";
    std::ifstream input(file.c_str()); // VC10 supports wide Windows file paths.
    if (!input) { error = "vehicles.catalog missing beside ProjectCars.dll"; return false; }
    return g_catalog.load(input, error);
}
}

static int startCars() {
    if (g_started) return 1;
    g_started = true;
    DebugLog("Project Cars 0.8.19 vehicle speed tuning; build " __DATE__ " " __TIME__);
    try {
        std::string error;
        if (!loadCatalog(error)) { ErrorLog("Project Cars disabled: " + error); return 1; }
        std::ostringstream message;
        message << "Project Cars: validated " << g_catalog.frames.size() << " frame definitions and "
                << g_catalog.parts.size() << " parts.";
        DebugLog(message.str());
        for (std::map<std::string, std::string>::const_iterator b = g_catalog.buildingBindings.begin(); b != g_catalog.buildingBindings.end(); ++b) {
            const cars::FrameDef* frame = g_catalog.frameForBuilding(b->first);
            if (frame) DebugLog("Project Cars: configured building StringID [" + b->first + "] -> frame [" + frame->id + "]");
        }
        if (!cars::installNativeRuntime(g_catalog)) {ErrorLog("Project Cars: native main-loop hook failed");return 1;}
        if (!cars::installOilProduction()) {ErrorLog("Project Cars: oil extraction hooks failed");return 1;}
        return 0;
    } catch (const std::exception& e) {
        ErrorLog(std::string("Project Cars initialization failed: ") + e.what());
    } catch (...) {
        ErrorLog("Project Cars initialization failed");
    }
    return 1;
}
__declspec(dllexport) void startPlugin() {startCars();}
