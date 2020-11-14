#include <Windows.h>
#include <detours.h>
#include <DetoursHelpers.h>
#include "DetoursPatch.h"

#define LOG_PREFIX "(D2detours.patch):"
#include "Log.h"


bool getPatchInformationFunctions(PatchInformationFunctions& functions, HMODULE hModulePatch)
{
    functions.GetBaseOrdinal = (GetBaseOrdinalType)GetProcAddress(hModulePatch, "GetBaseOrdinal");
    functions.GetLastOrdinal = (GetLastOrdinalType)GetProcAddress(hModulePatch, "GetLastOrdinal");
    functions.GetPatchAction = (GetPatchActionType)GetProcAddress(hModulePatch, "GetPatchAction");
    return functions.GetBaseOrdinal && functions.GetLastOrdinal && functions.GetPatchAction;
}

bool DetoursPatchModule(HMODULE hOriginalModule, const wchar_t* patchDllName)
{
    if (const HMODULE hModulePatch = TrueLoadLibraryW(patchDllName))
    {
        PatchInformationFunctions patch;
        if (!getPatchInformationFunctions(patch, hModulePatch))
        {
            LOGW(L"Failed to load {} patch info.\n", patchDllName);
            return false;
        }

        for (int ordinal = patch.GetBaseOrdinal(); ordinal <= patch.GetLastOrdinal(); ordinal++)
        {
            const PatchAction patchAction = patch.GetPatchAction(ordinal);
            PVOID originalOrdinalAddress = GetProcAddress(hOriginalModule, (LPCSTR)ordinal);
            PVOID patchOrdinalAddress = GetProcAddress(hModulePatch, (LPCSTR)ordinal);
            LOGW(L"Ordinal {} (origAddr {} {} patchAddr {}) \n",
                ordinal,
                originalOrdinalAddress,
                patchAction == PatchAction::FunctionReplaceOriginalByPatch || patchAction == PatchAction::PointerReplaceOriginalByPatch ? L"<==" : L"==>",
                patchOrdinalAddress);
            if (originalOrdinalAddress && patchOrdinalAddress)
            {
                LONG err = NO_ERROR;
                switch (patchAction)
                {
                case PatchAction::FunctionReplaceOriginalByPatch:
                    err = DetourAttach(&originalOrdinalAddress, patchOrdinalAddress);
                    break;
                case PatchAction::FunctionReplacePatchByOriginal:
                    err = DetourAttach(&patchOrdinalAddress, originalOrdinalAddress);
                    break;
                case PatchAction::PointerReplaceOriginalByPatch:
                    *(void**)originalOrdinalAddress = *(void**)patchOrdinalAddress;
                    break;
                case PatchAction::PointerReplacePatchByOriginal:
                    *(void**)patchOrdinalAddress = *(void**)originalOrdinalAddress;
                    break;
                case PatchAction::Ignore:
                    break;
                }
                if (err != NO_ERROR)
                {
                    LOGW(L"Failed to patch ordinal {} ({}) with {}\n", ordinal, originalOrdinalAddress, patchOrdinalAddress);
                    LOGW(L"Stop patching...\n");
                    return false;
                }
            }
            else {
                LOGW(L"Ordinal skipped due to missing function\n");
                return false;
            }
        }
    }
    else
    {
        LOGW(L"Failed to load {}\n", patchDllName);
        return false;
    }
    return true;
}
