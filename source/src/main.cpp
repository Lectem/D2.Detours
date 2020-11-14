#include <Windows.h>
#include <detours.h>
#include <fmt/format.h>
#include <shlwapi.h>
#include "DetoursHelpers.h"

#include "D2CMP.detours.h"
#include "D2Client.detours.h"
#include "D2Common.detours.h"

#define LOG_PREFIX "(D2.detours.dll):"
#include "Log.h"

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
    // Needed is someone tries to inject from a 64bit process
    if (DetourIsHelperProcess()) { return TRUE; }

    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DetourRestoreAfterWith();

        LOG(" Starting.\n");

        LOG(" DLLs:\n");
        for (HMODULE hModule = NULL; (hModule = DetourEnumerateModules(hModule)) != NULL;)
        {
            CHAR szName[MAX_PATH] = {0};
            GetModuleFileNameA(hModule, szName, sizeof(szName) - 1);
            LOG("  {}: {}\n", (void*)hModule, szName);
        }

        if (NO_ERROR == DetourTransactionBegin())
        {
            DetourUpdateThread(GetCurrentThread());
            if (!DetoursAttachLoadLibraryFunctions())
            {
                LOG(" Failed to attach LoadLibrary*\n");
                return FALSE;
            }

            DetoursRegisterDllPatch(L"D2CMP.dll", patchD2CMP, nullptr);
            DetoursRegisterDllPatch(L"D2Client.dll", patchD2Client, nullptr);
            DetoursRegisterDllPatch(L"D2Common.dll", patchD2Common, nullptr);

            if (LONG error = DetourTransactionCommit() == NO_ERROR)
                LOG(" Detoured SleepEx().\n");
            else
            {
                LOG(" Error detouring SleepEx(): {}\n", error);
                return FALSE;
            }
        }
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        if (NO_ERROR == DetourTransactionBegin())
        {
            DetourUpdateThread(GetCurrentThread());
            if (!DetoursDetachLoadLibraryFunctions()) LOG(" Failed to detach LoadLibrary*\n");
            LONG error = DetourTransactionCommit();
        }
        LOG(" Exiting D2 detours\n");
    }
    return TRUE;
}