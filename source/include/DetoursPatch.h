#pragma once

enum class PatchAction {
	FunctionReplaceOriginalByPatch,
	FunctionReplacePatchByOriginal,
	PointerReplaceOriginalByPatch,
	PointerReplacePatchByOriginal,
	Ignore
};

extern "C" {
	typedef int (__cdecl *GetBaseOrdinalType)();
	typedef int (__cdecl *GetLastOrdinalType)();
	typedef PatchAction (__cdecl *GetPatchActionType)(int ordinal);
}

struct PatchInformationFunctions
{
	// Must be exposed by the patch dll and return the 1st ordinal to patch
	GetBaseOrdinalType GetBaseOrdinal   = nullptr;
	// Must be exposed by the patch dll and return the last ordinal to patch
	GetLastOrdinalType GetLastOrdinal   = nullptr;
	// Must be exposed by the patch dll and return the action to take for a given ordinal
	GetPatchActionType GetPatchAction   = nullptr;
};

#ifdef DETOURS_PATCH_PRIVATE
#include <Windows.h>

/**
 * Patch hOriginalModule using a dll with a given path.
 * Ordinals patching is determined by the patch dll, and it must expose the functions in PatchInformationFunctions.
 */
bool DetoursPatchModule(HMODULE hOriginalModule, const wchar_t* patchDllNamee);
#endif

