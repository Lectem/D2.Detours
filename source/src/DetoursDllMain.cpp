#include <Windows.h>
#include <detours.h>
#include <fmt/format.h>
#include <shlwapi.h>
#include "DetoursHelpers.h"

#include "D2CMP.detours.h"
#include "D2Client.detours.h"
#include "D2Common.detours.h"

#define LOG_PREFIX "(D2.Detours.dll):"
#include "Log.h"

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
    // Needed is someone tries to inject from a 64bit process
    if (DetourIsHelperProcess()) { return TRUE; }

    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DetourRestoreAfterWith();

        LOG(" Starting.\n");

        LOG(" Already loaded DLLs:\n");
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
                USER_ERROR(" Failed to attach LoadLibrary*\n");
                return FALSE;
            }
            const LONG error = DetourTransactionCommit();
            if (error == NO_ERROR)
            {
                LOG(" Successfully applied detours to LoadLibrary.\n");

                DetoursRegisterDllPatch(L"D2CMP.dll", patchD2CMP, nullptr);
                DetoursRegisterDllPatch(L"D2Client.dll", patchD2Client, nullptr);
                DetoursRegisterDllPatch(L"D2Common.dll", patchD2Common, nullptr);

                DetoursApplyPatches();
            }
            else
            {
                USER_ERROR(" Error while patching with detours: {}\n", error);
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