#pragma once

#include <Windows.h>

bool DetoursAttachLoadLibraryFunctions();
bool DetoursDetachLoadLibraryFunctions();

typedef bool (*DetoursDllPatchFunction)(void* userContext, HMODULE hModule);

/// Automatically patch an existing dll or once it is loaded
void DetoursRegisterDllPatch(const wchar_t* dllName, DetoursDllPatchFunction patchFunction, void* userContext);

