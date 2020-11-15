
#include <Windows.h>
#include <PathCch.h>
#include <detours.h>
#include <DetoursPatch.h>

#define LOG_PREFIX "(D2Common.detours):"
#include "Log.h"
#include "D2Common.detours.h"

bool patchD2Common(void*, HMODULE hModule)
{
    LOGW(L"Patching D2Common.dll\n");

    if (DetourTransactionBegin() != NO_ERROR)
        return false;

    DetourUpdateThread(GetCurrentThread());
    
    const size_t maxEnvPathLen = 32'767; // From the win32 docs
    wchar_t* envPath = (wchar_t*)malloc(maxEnvPathLen * sizeof(*envPath));
    const wchar_t* patchFolder = (0 != GetEnvironmentVariableW(L"DIABLO2_PATCH", envPath, maxEnvPathLen)) ? envPath : LR"(.\patch\)";
    const wchar_t* patchDllName = LR"(D2Common.dll)";

    wchar_t* finalPatchPath = nullptr;
    bool patchSucceeded = false;
    if (S_OK == PathAllocCombine(patchFolder, patchDllName, PATHCCH_ALLOW_LONG_PATHS, &finalPatchPath))
        patchSucceeded = DetoursPatchModule(hModule, finalPatchPath);

    LocalFree(finalPatchPath);
    free(envPath);


    if (NO_ERROR != DetourTransactionCommit())
        return false;

    return patchSucceeded;
}
