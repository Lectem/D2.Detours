
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
    if (S_OK == PathAllocCombine(patchFolder, lpLibFileName, PATHCCH_ALLOW_LONG_PATHS, &finalPatchPath))
        patchSucceeded = DetoursPatchModule(hModule, finalPatchPath);

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


