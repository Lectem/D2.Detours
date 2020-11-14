#include "DetoursHelpers.h"

#include <Windows.h>
#include <detours.h>
#include <fmt/format.h>
#include <shlwapi.h>
#include <vector>

#define LOG_PREFIX "(DetoursHelpers):"
#include "Log.h"

struct DllPatch
{
    const wchar_t*          libraryName    = nullptr;
    DetoursDllPatchFunction patchFunction  = nullptr;
    void*                   userContext    = nullptr;
    // We need to prevent double patching to avoid infinite recursions
    bool                    alreadyPatched = false;
    // TODO : detach mechanism ?
};

static std::vector<DllPatch> dllPatches;

void DetoursRegisterDllPatch(const wchar_t* dllName, DetoursDllPatchFunction patchFunction, void* userContext)
{
    dllPatches.push_back(DllPatch{dllName, patchFunction, userContext});
    HMODULE hCurrentModule = nullptr;

    wchar_t moduleFileName[MAX_PATH];
    while (nullptr != (hCurrentModule = DetourEnumerateModules(hCurrentModule)))
    {
        WCHAR       szName[MAX_PATH] = {0};
        const DWORD nRetSize         = GetModuleFileNameW(hCurrentModule, szName, MAX_PATH);
        if (nRetSize == 0 || nRetSize == MAX_PATH)
        {
            LOGW(L"Error occured while getting module {} name", (void*)hCurrentModule);
            continue;
        }
        PathStripPathW(moduleFileName);
        if (0 != _wcsicmp(moduleFileName, dllName)) continue;
        if (!patchFunction(userContext, hCurrentModule)) LOGW(L"Failed to patch {}", szName);
    }
}

template<class CallLoadLibrary>
HMODULE LoadLibraryPatcher(LPCWSTR lpLibFileName, const CallLoadLibrary& callLoadLibrary)
{

    LOGW(L"LoadLibraryPatcher({}) called!\n", lpLibFileName);
    HMODULE hModule = callLoadLibrary();
    if (hModule)
    {
        wchar_t fileName[MAX_PATH];
        if (lstrcpynW(fileName, lpLibFileName, _countof(fileName)))
        {
            PathStripPathW(fileName);
            for (DllPatch& patch : dllPatches)
            {
                if (!patch.alreadyPatched && 0 == _wcsicmp(fileName, patch.libraryName))
                {
                    if (!patch.patchFunction(patch.userContext, hModule))
                        LOGW(L"Failed to patch {}", patch.libraryName); 
                    patch.alreadyPatched = true;
                }
            }
        }
    }
    return hModule;
}

template<class CallLoadLibrary>
HMODULE LoadLibraryPatcher(LPCSTR lpLibFileName, const CallLoadLibrary& callLoadLibrary)
{
    wchar_t libFileNameW[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, lpLibFileName, -1, libFileNameW, _countof(libFileNameW));
    return LoadLibraryPatcher(libFileNameW, callLoadLibrary);
}

static HMODULE(WINAPI* TrueLoadLibraryA)(LPCSTR lpLibFileName)                                 = LoadLibraryA;
static HMODULE(WINAPI* TrueLoadLibraryW)(LPCWSTR lpLibFileName)                                = LoadLibraryW;
static HMODULE(WINAPI* TrueLoadLibraryExA)(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)  = LoadLibraryExA;
static HMODULE(WINAPI* TrueLoadLibraryExW)(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags) = LoadLibraryExW;

_Ret_maybenull_ HMODULE WINAPI DetouredLoadLibraryA(_In_ LPCSTR lpLibFileName)
{
    LOG("LoadLibraryA({})\n", lpLibFileName);
    return LoadLibraryPatcher(lpLibFileName, [=]() { return TrueLoadLibraryA(lpLibFileName); });
}

_Ret_maybenull_ HMODULE WINAPI DetouredLoadLibraryW(_In_ LPCWSTR lpLibFileName)
{
    LOGW(L"LoadLibraryW({})\n", (wchar_t*)lpLibFileName);
    return LoadLibraryPatcher(lpLibFileName, [=]() { return TrueLoadLibraryW(lpLibFileName); });
}

_Ret_maybenull_ HMODULE WINAPI DetouredLoadLibraryExA(_In_ LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    LOG("LoadLibraryExA({},{},{})\n", lpLibFileName, (void*)hFile, dwFlags);
    return LoadLibraryPatcher(lpLibFileName, [=]() { return TrueLoadLibraryExA(lpLibFileName, hFile, dwFlags); });
}

_Ret_maybenull_ HMODULE WINAPI DetouredLoadLibraryExW(_In_ LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    LOGW(L"LoadLibraryExW({},{},{})\n", (wchar_t*)lpLibFileName, (void*)hFile, dwFlags);
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
