#include <Windows.h>
#include <detours.h>
#include <DetoursHelpers.h>
#include "DetoursPatch.h"

#define LOG_PREFIX "(D2detours.patch):"
#include "Log.h"
#include <unordered_map>

bool getPatchInformationFunctions(PatchInformationFunctions& functions, HMODULE hModulePatch)
{
    functions.GetBaseOrdinal = (GetIntegerFunctionType)GetProcAddress(hModulePatch, "GetBaseOrdinal");
    functions.GetLastOrdinal = (GetIntegerFunctionType)GetProcAddress(hModulePatch, "GetLastOrdinal");
    functions.GetPatchAction = (GetPatchActionType)GetProcAddress(hModulePatch, "GetPatchAction");

    functions.GetExtraPatchActionsCount = (GetIntegerFunctionType)GetProcAddress(hModulePatch, "GetExtraPatchActionsCount");
    functions.GetExtraPatchAction = (GetExtraPatchActionType)GetProcAddress(hModulePatch, "GetExtraPatchAction");

    // Extra patch actions are optional
    return functions.GetBaseOrdinal && functions.GetLastOrdinal && functions.GetPatchAction;
}

struct PatchHistory {

    // Note: because we use unordered_map, this means that we can not patch allocation functions this way.
    std::unordered_map<void*, int> patchedOriginalDllAddresses;
    std::unordered_map<void*, int> patchedPatchDllAddresses;

    bool CheckIfAlreadyPatched(int ordinal, void* originalOrdinalAddress, void* patchOrdinalAddress) {
#define ALREADY_PATCHED_MSG(dll) L"Ordinal {} " #dll L" address ({}) was already patched (for ordinal {}), skipping. This can lead to unwanted behavior (multiple ordinals with same function).\n"
        {
            auto inserted = patchedOriginalDllAddresses.insert({ originalOrdinalAddress, ordinal });
            if (!inserted.second)
            {
                LOGW(ALREADY_PATCHED_MSG(original), ordinal, originalOrdinalAddress, inserted.first->second);
                return true;
            }
        }
        {
            auto inserted = patchedPatchDllAddresses.insert({ patchOrdinalAddress, ordinal });
            if (!inserted.second)
            {
                LOGW(ALREADY_PATCHED_MSG(patch), ordinal, patchOrdinalAddress, inserted.first->second);
                return true;
            }
        }
        return false;
    };

};

// Returns false if patching failed
bool ApplyPatchAction(PatchHistory& patchHistory, PVOID originalOrdinalAddress, PVOID patchOrdinalAddress, PatchAction patchAction, int ordinal, PVOID* realPatchedFunction = nullptr)
{
    if (patchAction == PatchAction::Ignore)
    {
        LOGW(L"Ordinal {} ignored.\n", ordinal);
        return true;
    }
    LOGW(L"Ordinal {} (origAddr {} {} patchAddr {}) \n",
        ordinal,
        originalOrdinalAddress,
        patchAction == PatchAction::FunctionReplaceOriginalByPatch || patchAction == PatchAction::PointerReplaceOriginalByPatch ? L"<==" : L"==>",
        patchOrdinalAddress);

    if (originalOrdinalAddress && patchOrdinalAddress)
    {
        // We check if we didn't already patch the functions one way or another, as it could cause unwanted behaviour, or worse, infinite recursion.
        // However, this method of patching is not safe when the target dll function has ordinals that are not unique.
        // For example, with D2Common 1.10f,  10089_DUNGEON_GetInitSeedFromAct and 10985_SKILLS_GetFlags point to the same address.
        // This means you can not patch them with different functions, for example if you want to do some logging.
        // This is also an issue if you want to put a breakpoint, as you might trigger it even though it is not the ordinal you expected.
        // The best way to fix this would be to hook GetProcAddress to return the patched function directly.
        if (patchHistory.CheckIfAlreadyPatched(ordinal, originalOrdinalAddress, patchOrdinalAddress))
            return true;

        LONG err = NO_ERROR;
        switch (patchAction)
        {
        case PatchAction::FunctionReplaceOriginalByPatch:
            if (realPatchedFunction)
                *realPatchedFunction = originalOrdinalAddress;
            err = DetourAttach(realPatchedFunction  ? realPatchedFunction  : &originalOrdinalAddress, patchOrdinalAddress);
            break;
        case PatchAction::FunctionReplacePatchByOriginal:
            if (realPatchedFunction)
                *realPatchedFunction = patchOrdinalAddress;
            err = DetourAttach(realPatchedFunction ? realPatchedFunction : &patchOrdinalAddress, originalOrdinalAddress);
            break;
        case PatchAction::PointerReplaceOriginalByPatch:
            *(void**)originalOrdinalAddress = *(void**)patchOrdinalAddress;
            break;
        case PatchAction::PointerReplacePatchByOriginal:
            *(void**)patchOrdinalAddress = *(void**)originalOrdinalAddress;
            break;
        case PatchAction::Ignore: // Should not reach here since checked before
            break;
        }
        if (err != NO_ERROR)
        {
            LOGW(L"Failed to patch ordinal {} ({}) with {}\n", ordinal, originalOrdinalAddress, patchOrdinalAddress);
            return false;
        }
    }
    else {
        LOGW(L"Ordinal skipped due to missing function\n");
    }
    return true;
}


bool DetoursPatchModule(HMODULE hOriginalModule, HMODULE hPatchModule, std::vector<PVOID>& ordinalDetouredAddresses)
{
    PatchInformationFunctions patch;
    if (!getPatchInformationFunctions(patch, hPatchModule))
    {
        LOGW(L"Failed to load patch info.\n");
        return false;
    }

    PatchHistory patchHistory;

    ordinalDetouredAddresses.resize(patch.GetLastOrdinal() - patch.GetBaseOrdinal() + 1);
    for (int ordinal = patch.GetBaseOrdinal(); ordinal <= patch.GetLastOrdinal(); ordinal++)
    {
        const PatchAction patchAction = patch.GetPatchAction(ordinal);

        PVOID originalOrdinalAddress = GetProcAddress(hOriginalModule, (LPCSTR)ordinal);
        PVOID patchOrdinalAddress    = GetProcAddress(hPatchModule, (LPCSTR)ordinal);
        if (!ApplyPatchAction(patchHistory, originalOrdinalAddress, patchOrdinalAddress, patchAction, ordinal,
                              &ordinalDetouredAddresses[ordinal - patch.GetBaseOrdinal()]))
        {
            LOGW(L"Stop patching...\n");
            return false;
        }
    }

    const int nbExtraPatchActions = patch.GetExtraPatchActionsCount ? patch.GetExtraPatchActionsCount() : 0;
    for (int extraPatchActionIndex = 0; extraPatchActionIndex < nbExtraPatchActions; extraPatchActionIndex++)
    {
        ExtraPatchAction* extraPatchAction = patch.GetExtraPatchAction(extraPatchActionIndex);
        PVOID originalOrdinalAddress       = PVOID(uintptr_t(hOriginalModule) + extraPatchAction->originalDllOffset);
        // If an adress if given, then the user wants us to store the real function address at the provided pointer
        // value.
        void** realPatchedFunctionStorage = extraPatchAction->detouredPatchedFunction == nullptr
                                                ? &extraPatchAction->detouredPatchedFunction
                                                : (void**)extraPatchAction->detouredPatchedFunction;
        if (!ApplyPatchAction(patchHistory, originalOrdinalAddress, extraPatchAction->patchData,
                              extraPatchAction->action, -1, realPatchedFunctionStorage))
        {
            LOGW(L"Stop patching...\n");
            return false;
        }
    }
    return true;
}
