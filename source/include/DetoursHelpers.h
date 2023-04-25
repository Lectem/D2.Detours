#pragma once

#include <Windows.h>

bool DetoursAttachLoadLibraryFunctions();
bool DetoursDetachLoadLibraryFunctions();

using DetoursDllPatchFunction = bool (*)(LPCWSTR lpLibFileName, LPCWSTR patchLibraryPath, void* userContext, HMODULE hModule);

/// Automatically patch an existing dll or once it is loaded
void DetoursRegisterDllPatch(const wchar_t* dllName, const wchar_t* patchFolder, DetoursDllPatchFunction patchFunction,
                             void* userContext);
void DetoursApplyPatches();

/// See GetHookOrdinalInfo
template<class FuncType>
struct DllOrdinalHookInfo
{
    int       ordinal;
    FuncType  hookFunction;
    FuncType& realFunction;
};

/// Helper so that you don't need to repeat functions prototypes and store the pointers yourself
/// Usage: Call GetHookOrdinalInfo<ordinal>(detourFunction) to receive the detour information
template<int ordinal, class T>
inline DllOrdinalHookInfo<T> GetHookOrdinalInfo(T func)
{
    static T realFunctionPtr = nullptr;
    return {ordinal, func, realFunctionPtr};
}

/// A typeless version of DllOrdinalHookInfo to be used with GetHookOrdinalInfo if you want to build a container of hooks
struct DllOrdinalHookTypeless
{
    template<class FuncType>
    DllOrdinalHookTypeless(const DllOrdinalHookInfo<FuncType>& info)
        : ordinal(info.ordinal), hookFunction((PVOID)info.hookFunction), realFunction((PVOID&)info.realFunction)
    {
    }
    int    ordinal;
    PVOID  hookFunction;
    PVOID& realFunction;
};

// Use this to avoid recurive calls.
extern HMODULE(WINAPI* TrueLoadLibraryW)(LPCWSTR lpLibFileName);
extern HMODULE(WINAPI* TrueLoadLibraryA)(LPCSTR lpLibFileName);
extern HMODULE(WINAPI* TrueLoadLibraryExA)(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
extern HMODULE(WINAPI* TrueLoadLibraryExW)(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);