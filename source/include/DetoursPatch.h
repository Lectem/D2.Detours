#pragma once

enum class PatchAction {
	FunctionReplaceOriginalByPatch,
	FunctionReplacePatchByOriginal,
	PointerReplaceOriginalByPatch,
	PointerReplacePatchByOriginal,
	Ignore
};

struct ExtraPatchAction {
	size_t originalDllOffset;
	void* patchData;
	PatchAction action;
	void* detouredPatchedFunction; // Filled with new address of the original function. You can use it to call the old function from your patch.
};

extern "C" {
	typedef int(__cdecl* GetIntegerFunctionType)();
	typedef PatchAction (__cdecl *GetPatchActionType)(int ordinal);
	typedef ExtraPatchAction* (__cdecl* GetExtraPatchActionType)(int extraPatchActionIndex);
}

struct PatchInformationFunctions
{
	// Must be exposed by the patch dll and return the 1st ordinal to patch
	GetIntegerFunctionType GetBaseOrdinal   = nullptr;
	// Must be exposed by the patch dll and return the last ordinal to patch
	GetIntegerFunctionType GetLastOrdinal   = nullptr;
	// Must be exposed by the patch dll and return the action to take for a given ordinal
	GetPatchActionType GetPatchAction   = nullptr;

	// Must be exposed by the patch dll if extra patches are needed
	GetIntegerFunctionType GetExtraPatchActionsCount = nullptr;
	// Must be exposed by the patch dll and return the action to take for a given extra action index
	// Note that the returned pointer must be valid until the end of the patching, and not be reused between GetExtraPatchAction calls since we will fill the detouredPatchedFunction field.
	GetExtraPatchActionType GetExtraPatchAction = nullptr;
};

#ifdef DETOURS_PATCH_PRIVATE
#include <Windows.h>
#include <vector>

/**
 * Patch hOriginalModule using a dll with a given path.
 * Ordinals patching is determined by the patch dll, and it must expose the functions in PatchInformationFunctions.
 */
bool DetoursPatchModule(HMODULE hOriginalModule, const wchar_t* patchDllNamee, std::vector<PVOID>& ordinalDetouredAddresses);
#endif

