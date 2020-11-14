
#include <Windows.h>
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
    
    const wchar_t* patchDllName = LR"(.\patch\D2Common.dll)";
    
    const bool patchSucceeded = DetoursPatchModule(hModule, patchDllName);
    
    if (NO_ERROR != DetourTransactionCommit())
        return false;

    return patchSucceeded;
}
