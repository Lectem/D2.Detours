#pragma once

#include <cstdint>

enum class PatchAction
{
    FunctionReplaceOriginalByPatch,
    FunctionReplacePatchByOriginal,
    PointerReplaceOriginalByPatch,
    PointerReplacePatchByOriginal,
    Ignore
};

struct ExtraPatchAction
{
    size_t      originalDllOffset;
    void*       patchData;
    PatchAction action;

	// You may select another location to store the real function pointer. This may make it easier to reference it from your hooks.
    void* detouredPatchedFunctionPointerStorageAddress = &detouredPatchedFunctionPointer;
	// This will always be filled, but for convenience `detouredPatchedFunctionPointerAddress` may point to your own variable instead of detouredPatchedFunctionPointer.
    void* detouredPatchedFunctionPointer = nullptr;
};

extern "C"
{
	// Old API, where one had to export GetBaseOrdinal+GetLastOrdinal+GetPatchAction (and optionally GetExtraPatchActionsCount+GetExtraPatchAction), or expose all through GetPatchInformationFunctions
	// Please use DllPreLoadHook now, as it gives you more freedom and should now be the preferred API.
	// For compatibility and ease of use, and/or if you need multi-dll support, you may use GetPatchInformationFunctions
    typedef int(__cdecl* GetIntegerFunctionType)();
    typedef PatchAction(__cdecl* GetPatchActionType)(int ordinal);
    typedef ExtraPatchAction*(__cdecl* GetExtraPatchActionType)(int extraPatchActionIndex);

    struct PatchInformationFunctions
    {
        // Must be exposed by the patch dll and return the 1st ordinal to patch
        GetIntegerFunctionType GetBaseOrdinal = nullptr;
        // Must be exposed by the patch dll and return the last ordinal to patch
        GetIntegerFunctionType GetLastOrdinal = nullptr;
        // Must be exposed by the patch dll and return the action to take for a given ordinal
        GetPatchActionType GetPatchAction = nullptr;

        // Must be exposed by the patch dll if extra patches are needed
        GetIntegerFunctionType GetExtraPatchActionsCount = nullptr;
        // Must be exposed by the patch dll and return the action to take for a given extra action index
        // Note that the returned pointer must be valid until the end of the patching, and not be reused between
        // GetExtraPatchAction calls since we will fill the detouredPatchedFunction field.
        GetExtraPatchActionType GetExtraPatchAction = nullptr;
    };
    typedef PatchInformationFunctions(__cdecl* GetPatchInformationFunctionsType)(const wchar_t* dllName);

    enum PatchActionReturn
    {
        PatchAction_Success,
        PatchAction_AlreadyPatched,				// Nothing was done because the function was already replaced
        PatchAction_CircularPatching,			// Nothing was done because the function had been replaced in the other direction before
        PatchAction_PatchFunctionWasPatched,	// Nothing was done because the source function had already been replaced before
        PatchAction_BadInput,					// Input parameters are invalid
        PatchAction_PatchFailed,				// Patching utility failed with an unknown reason. It might be due to a breakpoint being placed at the beginning of the target function, or the target function being patched by another system
    };

	// Convenience function for all kind of patching with sanity checks
	// Similar in spirit to the old API, except you are the one calling the patching functions.
	// 
	// originalDllOffset is the offset (function or address) relative to the base of the original dll. Note that it may be the source or destination of replacement depending on `patchAction`.
	// patchDllAddr is the address of the patch dll (function or address of pointer). Note that it may be the source or destination of replacement depending on `patchAction`.
	// patchAction is the action you want to do (see the enum)
	// realPatchedFunctionPointerStorageAddress is used to store the address of the unpatched function, it may be used to call the function that was replaced from the patch itself. Use nullptr if you do not need it.
    typedef PatchActionReturn(__cdecl* ApplyPatchActionType)(struct HookContext* context, uintptr_t originalDllOffset,
                                                             void* patchDllAddr, PatchAction patchAction,
                                                             void** realPatchedFunctionPointerStorageAddress);
    
	// Mostly the same as ApplyPatchAction for functions only, except that no sanity checks will be done at all.
	// Not recommended unless you know what you are doing.
	typedef PatchActionReturn(__cdecl* ReplaceAnyFunctionType)(HookContext* context, void* originalFunction,
                                                            void* patchFunction, void** realPatchedFunctionStorage);

    struct HookContext
    {
        void*                  pContextPrivateData;
        void*                  hOriginalModule; // The module you want to patch. May be safely casted to HMODULE
        void*                  hPatchModule;    // The module containing the patch. May be safely casted to HMODULE
        ApplyPatchActionType   ApplyPatchAction;
        ReplaceAnyFunctionType ReplaceAnyFunction;
    };


	// Expected to be exported as ordinal under the name `DllPreLoadHook`
	// 
	// Usage example:
	// 
	//	__declspec(dllexport) uint32_t __cdecl DllPreLoadHook(HookContext* ctx, const wchar_t* dllName)
	//	{
	//		for (auto& p : MyExtraPatchActions)
	//		{
	//			ctx->ApplyPatchAction(ctx, p.originalDllOffset, p.patchData, p.action, (void**)p.detouredPatchedFunctionPointerStorageAddress);
	//		}
	//		return 0; // success
	//	}
	// 
	// Will be called before any patching is done, right after the first `LoadLibrary` call for the original .dll.
	// Return 0 on success
	// hOriginalDllModule and hPatchDllModule may be safely casted to HMODULE
    typedef uint32_t(__cdecl* DllPreLoadHookType)(HookContext* ctx, const wchar_t* dllName);
}
#ifdef DETOURS_PATCH_PRIVATE
#include <Windows.h>
#include <vector>

/**
 * Patch hOriginalModule using a dll with a given path.
 * Ordinals patching is determined by the patch dll, and it must expose the functions in PatchInformationFunctions.
 */
bool DetoursPatchModule(LPCWSTR lpLibFileName, HMODULE hOriginalModule, HMODULE hPatchModule,
                        std::vector<PVOID>& ordinalDetouredAddresses);
#endif
