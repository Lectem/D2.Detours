#include "DetoursHelpers.h"
#include "DetoursPatch.h"

#include <Windows.h>
#include <detours.h>
#include <fmt/format.h>
#include <shlwapi.h>
#include <vector>

#define LOG_PREFIX "(DetoursHelpers):"
#include "Log.h"

// Uncomment for extensive tracing of LoadLibrary and patching
// #define TRACE_FOR_DEBUG

#ifdef TRACE_FOR_DEBUG
#define TRACE LOG
#define TRACEW LOGW
#else
#define TRACE
#define TRACEW
#endif

struct DllPatch
{
    std::wstring            libraryName;
    std::wstring            patchLibraryPath;
    DetoursDllPatchFunction patchFunction  = nullptr;
    void*                   userContext    = nullptr;
    // We need to prevent double patching to avoid infinite recursions
    bool                    alreadyPatched = false;
    // TODO : detach mechanism ?
};

static std::vector<DllPatch> dllPatches;

static void PatchDLL(LPCWSTR lpLibFileName, HMODULE hModule)
{
    wchar_t fileName[MAX_PATH];
    if (lstrcpynW(fileName, lpLibFileName, _countof(fileName)))
    {
        PathStripPathW(fileName);
        for (DllPatch& patch : dllPatches)
        {
            if (!patch.alreadyPatched && 0 == _wcsicmp(fileName, patch.libraryName.c_str()))
            {
                // Do this to avoid recursion, as GetProcAdress can call LoadLibrary
                patch.alreadyPatched = true;

                if (!patch.patchFunction(fileName, patch.patchLibraryPath.c_str(), patch.userContext, hModule))
                    LOGW(L"Failed to patch {}\n", patch.libraryName);
            }
        }
    }
}


const HMODULE GetDetoursDllModule()
{
    HMODULE hModule = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCTSTR)GetDetoursDllModule, &hModule);
    return hModule;
}
const HMODULE gDetoursDllModule = GetDetoursDllModule();

void DetoursRegisterDllPatch(const wchar_t* dllName, const wchar_t* patchFolder, DetoursDllPatchFunction patchFunction,
                             void* userContext)
{
    // We need to make sure we load the patch .dll, not the one we want to patch.
    const std::wstring fullDllPath = fmt::format(L"{}\\{}", patchFolder, dllName);
    // To support patching another .dll without specifying a naming convention, we use resource (.rc) files.
    // This is because we do not have a way to load a .dll without ensuring we don't load .dlls that should not be loaded yet.
    // For example we need to wait for D2Common.dll to be loaded before loading a .dll that patches D2Game and may have D2Common in its import table.
    // 
    // So we actually need to know the .dll we want to patch so that we don't load the patch too early...
    // We can only rely on either another file (which is not very practical), reading data from the .dll itself, or embedding this in the patch file name.
    // Most of those solutions are not very practical, and since it is possible to load a .dll without loading its dependencies 
    // nor calling its DllMain (LOAD_LIBRARY_AS_DATAFILE) but still be able to read its resources, that's what we use...
    // We then unload the patch .dll if it wasn't already loaded, so that the next LoadLibrary actually loads it correctly.
    // 
    // You will need to create a .rc file with the following content:
    // NameOfModuleToPatch 256 { L"TheDLLIWantToPatch.dll\0" }
    if (HMODULE patchDLL = TrueLoadLibraryExW(fullDllPath.c_str(), NULL, LOAD_LIBRARY_AS_DATAFILE))
    {
		// Ignore if self.
        if (gDetoursDllModule == patchDLL)
        {
            FreeLibrary(patchDLL);
            return;
		}

        HRSRC NameOfModulesToPatch = FindResourceA(patchDLL, TEXT("NameOfModuleToPatch"), resourceType); // Backward compat

        if (NameOfModulesToPatch)
        {
            HGLOBAL res = nullptr;
            const wchar_t* dllToPatch = nullptr;
            if ((res = LoadResource(patchDLL, NameOfModuleToPatch)) && (dllToPatch = (const wchar_t*)LockResource(res)))
            {
                LOGW(L"{} will be used to patch {}\n", dllName, dllToPatch);
                dllPatches.push_back(DllPatch{dllToPatch, fullDllPath, patchFunction, userContext});
                FreeLibrary(patchDLL);
                return;
            }
        }
        FreeLibrary(patchDLL);
    }
    dllPatches.push_back(DllPatch{dllName, fullDllPath, patchFunction, userContext});
}

void DetoursApplyPatches()
{
    HMODULE hCurrentModule = nullptr;
    wchar_t moduleFileName[MAX_PATH];
    while (nullptr != (hCurrentModule = DetourEnumerateModules(hCurrentModule)))
    {
        WCHAR       szName[MAX_PATH] = {0};
        const DWORD nRetSize         = GetModuleFileNameW(hCurrentModule, szName, MAX_PATH);
        if (nRetSize == 0 || nRetSize == MAX_PATH)
        {
            TRACEW(L"Error occured while getting module {} name\n", (void*)hCurrentModule);
            continue;
        }
        PatchDLL(szName,hCurrentModule);
    }
}


template<class CallLoadLibrary>
HMODULE LoadLibraryPatcher(LPCWSTR lpLibFileName, const CallLoadLibrary& callLoadLibrary)
{
    const HMODULE hModule = callLoadLibrary();
    // We are forced to check all dlls for patching as the loader does not call LoadLibrary
    // and we can't trigger LoadLibrary from its notifications
    // This could easily be optimized at a later point in time if needed.
    if(hModule)
        DetoursApplyPatches();
    return hModule;
}

template<class CallLoadLibrary>
HMODULE LoadLibraryPatcher(LPCSTR lpLibFileName, const CallLoadLibrary& callLoadLibrary)
{
    wchar_t libFileNameW[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, lpLibFileName, -1, libFileNameW, _countof(libFileNameW));
    return LoadLibraryPatcher(libFileNameW, callLoadLibrary);
}

HMODULE(WINAPI* TrueLoadLibraryW)(LPCWSTR lpLibFileName)                                = LoadLibraryW;
HMODULE(WINAPI* TrueLoadLibraryA)(LPCSTR lpLibFileName)                                 = LoadLibraryA;
HMODULE(WINAPI* TrueLoadLibraryExA)(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)  = LoadLibraryExA;
HMODULE(WINAPI* TrueLoadLibraryExW)(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags) = LoadLibraryExW;

_Ret_maybenull_ HMODULE WINAPI DetouredLoadLibraryA(_In_ LPCSTR lpLibFileName)
{
    TRACE("LoadLibraryA({})\n", lpLibFileName);
    return LoadLibraryPatcher(lpLibFileName, [=]() { return TrueLoadLibraryA(lpLibFileName); });
}

_Ret_maybenull_ HMODULE WINAPI DetouredLoadLibraryW(_In_ LPCWSTR lpLibFileName)
{
    TRACEW(L"LoadLibraryW({})\n", (wchar_t*)lpLibFileName);
    return LoadLibraryPatcher(lpLibFileName, [=]() { return TrueLoadLibraryW(lpLibFileName); });
}

_Ret_maybenull_ HMODULE WINAPI DetouredLoadLibraryExA(_In_ LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    TRACE("LoadLibraryExA({},{},{})\n", lpLibFileName, (void*)hFile, dwFlags);
    return LoadLibraryPatcher(lpLibFileName, [=]() { return TrueLoadLibraryExA(lpLibFileName, hFile, dwFlags); });
}

_Ret_maybenull_ HMODULE WINAPI DetouredLoadLibraryExW(_In_ LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    TRACEW(L"LoadLibraryExW({},{},{})\n", (wchar_t*)lpLibFileName, (void*)hFile, dwFlags);
    return LoadLibraryPatcher(lpLibFileName, [=]() { return TrueLoadLibraryExW(lpLibFileName, hFile, dwFlags); });
}

bool DetoursAttachLoadLibraryFunctions()
{
    bool success = true;
    success      = success && NO_ERROR == DetourAttach(&(PVOID&)TrueLoadLibraryA, DetouredLoadLibraryA);
    success      = success && NO_ERROR == DetourAttach(&(PVOID&)TrueLoadLibraryW, DetouredLoadLibraryW);
    success      = success && NO_ERROR == DetourAttach(&(PVOID&)TrueLoadLibraryExA, DetouredLoadLibraryExA);
    success      = success && NO_ERROR == DetourAttach(&(PVOID&)TrueLoadLibraryExW, DetouredLoadLibraryExW);
    return true;

    // Kernel32 uses stubs which call kernelbase for the real implementation
    // This means that we may be missing some calls, so hook KernelBase directly
    if (HMODULE kernelbase = LoadLibraryA("KernelBase.dll"))
    {
        (PVOID&)TrueLoadLibraryA = GetProcAddress(kernelbase, "LoadLibraryA");
        (PVOID&)TrueLoadLibraryW = GetProcAddress(kernelbase, "LoadLibraryW");
        (PVOID&)TrueLoadLibraryExA = GetProcAddress(kernelbase, "LoadLibraryExA");
        (PVOID&)TrueLoadLibraryExW = GetProcAddress(kernelbase, "LoadLibraryExW");


        bool success = true;
        success = success && NO_ERROR == DetourAttach(&(PVOID&)TrueLoadLibraryA, DetouredLoadLibraryA);
        success = success && NO_ERROR == DetourAttach(&(PVOID&)TrueLoadLibraryW, DetouredLoadLibraryW);
        success = success && NO_ERROR == DetourAttach(&(PVOID&)TrueLoadLibraryExA, DetouredLoadLibraryExA);
        success = success && NO_ERROR == DetourAttach(&(PVOID&)TrueLoadLibraryExW, DetouredLoadLibraryExW);

        return success;
    }
    USER_ERROR("Couldn't load kernel32.dll !!");
    return false;
}
bool DetoursDetachLoadLibraryFunctions()
{
    bool success = true;
    success      = success && NO_ERROR == DetourDetach(&(PVOID&)TrueLoadLibraryA, DetouredLoadLibraryA);
    success      = success && NO_ERROR == DetourDetach(&(PVOID&)TrueLoadLibraryW, DetouredLoadLibraryW);
    success      = success && NO_ERROR == DetourDetach(&(PVOID&)TrueLoadLibraryExA, DetouredLoadLibraryExA);
    success      = success && NO_ERROR == DetourDetach(&(PVOID&)TrueLoadLibraryExW, DetouredLoadLibraryExW);
    return true;
}
