#include <Windows.h>
#include <detours.h>
#include <DetoursHelpers.h>
#include "DetoursPatch.h"
#include <psapi.h>

#define LOG_PREFIX "(D2detours.patch):"
#include "Log.h"
#include <unordered_map>

bool getPatchInformationFunctions(LPCWSTR lpLibFileName, PatchInformationFunctions& functions, HMODULE hModulePatch)
{
    if (auto GetPatchInformationFunctions =
            (GetPatchInformationFunctionsType)GetProcAddress(hModulePatch, "GetPatchInformationFunctions"))
    {
        functions = GetPatchInformationFunctions(lpLibFileName);
    }
    else
    {
        functions.GetBaseOrdinal = (GetIntegerFunctionType)GetProcAddress(hModulePatch, "GetBaseOrdinal");
        functions.GetLastOrdinal = (GetIntegerFunctionType)GetProcAddress(hModulePatch, "GetLastOrdinal");
        functions.GetPatchAction = (GetPatchActionType)GetProcAddress(hModulePatch, "GetPatchAction");

        functions.GetExtraPatchActionsCount =
            (GetIntegerFunctionType)GetProcAddress(hModulePatch, "GetExtraPatchActionsCount");
        functions.GetExtraPatchAction = (GetExtraPatchActionType)GetProcAddress(hModulePatch, "GetExtraPatchAction");

        // Extra patch actions are optional
    }
    return functions.GetBaseOrdinal && functions.GetLastOrdinal && functions.GetPatchAction ||
           functions.GetExtraPatchActionsCount && functions.GetExtraPatchAction;
}

struct PatchHistory
{
    // Note: Since we use unordered_map, we can not patch allocation functions this way as they would allocate while
    // being registered into the map.
    std::unordered_map<void*, void*> patchedAddresses;

    PatchActionReturn PatchChecks(const wchar_t* logPrefix,void* addressBeingPatched, void* patchAddress)
    {
        // We check if we didn't already patch the functions one way or another, as it could cause unwanted behaviour,
        // or worse, infinite recursion. However, this method of patching is not safe when the target dll function has
        // ordinals that are not unique. For example, with D2Common 1.10f,  10089_DUNGEON_GetInitSeedFromAct and
        // 10985_SKILLS_GetFlags point to the same address. This means you can not patch them with different functions,
        // for example if you want to do some logging. This is also an issue if you want to put a breakpoint, as you
        // might trigger it even though it is not the ordinal you expected. The best way to fix this would be to hook
        // GetProcAddress to return the patched function directly.
        auto inserted = patchedAddresses.insert({addressBeingPatched, patchAddress});
        if (!inserted.second)
        {
            LOGW(L"{}Trying to patch address {} which was already patched by {}, skipping patch. This can lead to unwanted behavior "
                 L"(multiple functions could have been merged in original dll).\n",
                 logPrefix, addressBeingPatched, inserted.first->second);
            return PatchAction_AlreadyPatched;
        }
        auto it = patchedAddresses.find(patchAddress);
        if (it != patchedAddresses.end())
        {
            if (it->second == addressBeingPatched)
            {
                LOGW(L"{}Trying to patch address {} using {} which itself was already patched by {}, skipping patch. "
                     L"This would lead to circular dependencies and infinite recursion.\n",
                     logPrefix, addressBeingPatched, patchAddress, addressBeingPatched);
                return PatchAction_CircularPatching;
            }
			else
            {
                LOGW(L"{}Trying to patch address {} using {} which itself was already patched by {}, skipping patch. This could "
                     L"lead to circular dependencies, infinite recursion or other issues.\n",
                     logPrefix, addressBeingPatched, patchAddress, it->second);
                return PatchAction_PatchFunctionWasPatched;
			}
        }
        return PatchAction_Success;
    }
};



PatchActionReturn ApplyPatchAction(PatchHistory& patchHistory, PVOID originalAddress, PVOID patchAddress,
                                   PatchAction patchAction, PVOID* realPatchedFunctionPtr = nullptr)
{
    if (patchAction == PatchAction::Ignore) return PatchAction_Success;
    if (originalAddress == nullptr || patchAddress == nullptr) { return PatchAction_BadInput; }
    switch (patchAction)
    {
    case PatchAction::FunctionReplaceOriginalByPatch:
    case PatchAction::PointerReplaceOriginalByPatch:
    {
        PatchActionReturn r = patchHistory.PatchChecks(L"Original<==Patch:", originalAddress, patchAddress);
        if (r != PatchAction_Success) return r;
        break;
    }
    case PatchAction::FunctionReplacePatchByOriginal:
    case PatchAction::PointerReplacePatchByOriginal:
    {
        PatchActionReturn r = patchHistory.PatchChecks(L"Original==>Patch:", patchAddress, originalAddress);
        if (r != PatchAction_Success) return r;
        break;
    }
    }

    LONG err = NO_ERROR;
    switch (patchAction)
    {
    case PatchAction::FunctionReplaceOriginalByPatch:
        if (realPatchedFunctionPtr)
            *realPatchedFunctionPtr = originalAddress;
        else
            realPatchedFunctionPtr = &originalAddress;
        err = DetourAttach(realPatchedFunctionPtr, patchAddress);
        break;
    case PatchAction::FunctionReplacePatchByOriginal:
        if (realPatchedFunctionPtr)
            *realPatchedFunctionPtr = patchAddress;
        else
            realPatchedFunctionPtr = &patchAddress;
        err = DetourAttach(realPatchedFunctionPtr, originalAddress);
        break;
    case PatchAction::PointerReplaceOriginalByPatch: *(void**)originalAddress = *(void**)patchAddress; break;
    case PatchAction::PointerReplacePatchByOriginal: *(void**)patchAddress = *(void**)originalAddress; break;
    case PatchAction::Ignore: // Should not reach here since checked before
        break;
    }
    if (err != NO_ERROR)
    {
        LOGW(L"Failed to patch with error {}\n", (void*)err);
        return PatchAction_PatchFailed;
    }
    return PatchAction_Success;
}

PatchHistory gPatchHistory;
struct HookContextData {
    PatchHistory& patchHistory = ::gPatchHistory;
#ifndef NDEBUG
    bool hasModuleInfo = false;
    MODULEINFO originalModuleInfo;
    MODULEINFO patchModuleInfo;
#endif
};

static bool AddressIsInModule(void* address, MODULEINFO& moduleInfo)
{
    const uintptr_t comparableAddr = uintptr_t(address);
    return comparableAddr >= uintptr_t(moduleInfo.lpBaseOfDll) &&
           comparableAddr < (uintptr_t(moduleInfo.lpBaseOfDll) + moduleInfo.SizeOfImage);
}

bool DetoursPatchModule(LPCWSTR lpLibFileName, HMODULE hOriginalModule, HMODULE hPatchModule,
                        std::vector<PVOID>& ordinalDetouredAddresses)
{
    HookContextData ctxData{};
#ifndef NDEBUG
    ctxData.hasModuleInfo =
        GetModuleInformation(GetCurrentProcess(), hOriginalModule, &ctxData.originalModuleInfo, sizeof(MODULEINFO)) &&
        GetModuleInformation(GetCurrentProcess(), hPatchModule, &ctxData.patchModuleInfo, sizeof(MODULEINFO));
#endif
    HookContext ctx{};
    ctx.pContextPrivateData = &ctxData;
    ctx.hOriginalModule     = hOriginalModule;
    ctx.hPatchModule        = hPatchModule;

    auto DllPreLoadHook = (DllPreLoadHookType)GetProcAddress(hPatchModule, "DllPreLoadHook");
    if (DllPreLoadHook)
    {
        ctx.ApplyPatchAction = [](HookContext* context, uintptr_t originalDllOffset, void* patchAddr,
                                  PatchAction patchAction, void** realPatchedFunction)
        {
            HookContextData& ctxData      = *(HookContextData*)context->pContextPrivateData;
            void*            originalAddr = (void*)(uintptr_t(context->hOriginalModule) + originalDllOffset);
#ifndef NDEBUG
            // Check if parameters were inverted. (We do not check if address is in the expected module as it could be
            // allocated instead of being in the static part of the DLL)
            assert(!ctxData.hasModuleInfo || (!AddressIsInModule(originalAddr, ctxData.patchModuleInfo) &&
                                              !AddressIsInModule(patchAddr, ctxData.originalModuleInfo)));
#endif
            return ApplyPatchAction(ctxData.patchHistory, originalAddr, patchAddr, patchAction, realPatchedFunction);
        };
        ctx.ReplaceAnyFunction =
            [](HookContext* context, void* originalFunction, void* patchFunction, void** realPatchedFunctionStorage)
        {
            if (realPatchedFunctionStorage)
                *realPatchedFunctionStorage = originalFunction;
            else
                realPatchedFunctionStorage = &originalFunction;
            LONG err = DetourAttach(realPatchedFunctionStorage, patchFunction);

            if (err != NO_ERROR)
            {
                LOGW(L"Failed to patch with error {}\n", (void*)err);
                return PatchAction_PatchFailed;
            }
            return PatchAction_Success;
        };

        if (uint32_t err = DllPreLoadHook(&ctx, lpLibFileName))
        {
            LOGW(L"DllPreLoadHook returned{}.\n", err);
            // TODO: Handle error?
            return false;
        }
    }

    PatchInformationFunctions patch;
    if (!getPatchInformationFunctions(lpLibFileName, patch, hPatchModule))
    {
        if (DllPreLoadHook)
        {
            LOGW(L"Failed to load patch info.\n");
            return false;
        }
        else { return true; }
    }
    if (patch.GetLastOrdinal && patch.GetBaseOrdinal && patch.GetPatchAction)
    {
        ordinalDetouredAddresses.resize(patch.GetLastOrdinal() - patch.GetBaseOrdinal() + 1);
        for (int ordinal = patch.GetBaseOrdinal(); ordinal <= patch.GetLastOrdinal(); ordinal++)
        {
            const PatchAction patchAction = patch.GetPatchAction(ordinal);
            if (patchAction == PatchAction::Ignore)
            {
                LOGW(L"Ordinal {} ignored.\n", ordinal);
                continue;
            }

            PVOID originalOrdinalAddress = GetProcAddress(hOriginalModule, (LPCSTR)ordinal);
            PVOID patchOrdinalAddress    = GetProcAddress(hPatchModule, (LPCSTR)ordinal);

            LOGW(L"Patching oridnal {} (origAddr {} {} patchAddr {}) \n", ordinal, originalOrdinalAddress,
                 patchAction == PatchAction::FunctionReplaceOriginalByPatch ||
                         patchAction == PatchAction::PointerReplaceOriginalByPatch
                     ? L"<=="
                     : L"==>",
                 patchOrdinalAddress);
            switch (ApplyPatchAction(ctxData.patchHistory, originalOrdinalAddress, patchOrdinalAddress, patchAction,
                                     &ordinalDetouredAddresses[ordinal - patch.GetBaseOrdinal()]))
            {
            case PatchAction_BadInput: // FALLTHROUGH
            case PatchAction_PatchFailed: LOGW(L"Stop patching...\n"); return false;
            default: break;
            }
        }
    }

    const int nbExtraPatchActions = patch.GetExtraPatchActionsCount ? patch.GetExtraPatchActionsCount() : 0;
    for (int extraPatchActionIndex = 0; extraPatchActionIndex < nbExtraPatchActions; extraPatchActionIndex++)
    {
        ExtraPatchAction* extraPatchAction = patch.GetExtraPatchAction(extraPatchActionIndex);

        PVOID             originalAddress  = PVOID(uintptr_t(hOriginalModule) + extraPatchAction->originalDllOffset);
        // If an adress if given, then the user wants us to store the real function address at the provided pointer
        // value.
        void** realPatchedFunctionStorage =
            extraPatchAction->detouredPatchedFunctionPointerStorageAddress == nullptr
                ? &extraPatchAction->detouredPatchedFunctionPointer
                : (void**)extraPatchAction->detouredPatchedFunctionPointerStorageAddress;

        if (extraPatchAction->action == PatchAction::Ignore)
        {
            LOGW(L"Ignoring patch offset {} patchAddr {}) \n", extraPatchAction->originalDllOffset, extraPatchAction->patchData);
            continue;
        }
        LOGW(L"Patching origAddr {} {} patchAddr {}) \n", originalAddress,
             extraPatchAction->action == PatchAction::FunctionReplaceOriginalByPatch ||
                     extraPatchAction->action == PatchAction::PointerReplaceOriginalByPatch
                 ? L"<=="
                 : L"==>",
             extraPatchAction->patchData);
        switch (ApplyPatchAction(ctxData.patchHistory, originalAddress, extraPatchAction->patchData,
                                 extraPatchAction->action, realPatchedFunctionStorage))
        {
        case PatchAction_BadInput: // FALLTHROUGH
        case PatchAction_PatchFailed: LOGW(L"Stop patching...\n"); return false;
        default:
            // Always write the function pointer to the default location even if the user provided a custom location
            extraPatchAction->detouredPatchedFunctionPointer = *realPatchedFunctionStorage;
            break;
        }
    }
    return true;
}
