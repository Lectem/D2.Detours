
#include <Windows.h>
#include <PathCch.h>
#include <detours.h>
#include <DetoursPatch.h>
#include <DetoursHelpers.h>
#include <shlwapi.h>

#define LOG_PREFIX "(D2Common.detours):"
#include "Log.h"

const size_t maxEnvPathLen = 32'767; // From the win32 docs
wchar_t envPathBuffer[maxEnvPathLen];

const wchar_t* patchFolder = (0 != GetEnvironmentVariableW(L"DIABLO2_PATCH", envPathBuffer, maxEnvPathLen)) ? envPathBuffer : LR"(.\patch\)";

bool patchDllWithEmbeddedPatches(LPCWSTR lpLibFileName, void*, HMODULE hModule)
{
    LOGW(L"Patching {}\n", lpLibFileName);

    if (DetourTransactionBegin() != NO_ERROR)
        return false;

    DetourUpdateThread(GetCurrentThread());

    wchar_t* finalPatchPath = nullptr;
    bool patchSucceeded = false;
    // We need to keep the addresses that are given to DetourAttach alive until the transaction finishes,
    // so we store them in a temporary vector
    std::vector<PVOID> keepAliveOrdinalDetoursAddresses;
    if (S_OK == PathAllocCombine(patchFolder, lpLibFileName, PATHCCH_ALLOW_LONG_PATHS, &finalPatchPath))
        patchSucceeded = DetoursPatchModule(hModule, finalPatchPath, keepAliveOrdinalDetoursAddresses);

    LocalFree(finalPatchPath);

    if (NO_ERROR != DetourTransactionCommit())
        return false;

    return patchSucceeded;
}

void D2DetoursRegisterPatchFolder()
{
    if (!PathFileExistsW(patchFolder))
    {
        USER_ERRORW(L"Could not find directory {}. Aborting.\n", patchFolder);
        exit(1);
    }

    // Set the patch folder as the DLL directory.
    // The objective is to allow for 3rd party DLLs to be placed in the same directory.
    // This is fine because per the documentation https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setdlldirectorya
    // the working directory still has priority over the DLL directory. This means the game .DLLs will still be loaded first.
    // If this ever becomes an issue for whatever reason, we should change the names of the patch .DLLs using a prefix, suffix, or another extension.
    SetDllDirectoryW(patchFolder);

    auto searchPath = fmt::format(L"{}\\*.dll", patchFolder);

    WIN32_FIND_DATAW findData;
    HANDLE searchHandle = FindFirstFileW(searchPath.c_str(), &findData);
    if (searchHandle == INVALID_HANDLE_VALUE)
    {
        LOGW(L"Warning: Could not find any dll in {}.\n", patchFolder);
        return;
    }
    do {
        LOGW(L"Registering patch for {}.\n", findData.cFileName);
        DetoursRegisterDllPatch(findData.cFileName, patchDllWithEmbeddedPatches, nullptr);
    } while (FindNextFileW(searchHandle, &findData));

    FindClose(searchHandle);



}


