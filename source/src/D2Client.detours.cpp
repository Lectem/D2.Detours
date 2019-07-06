
#include <Windows.h>
#include <detours.h>

#define LOG_PREFIX "(D2Client.detours):"
#include "Log.h"

bool patchD2Client(void*, HMODULE hModule)
{
    LOGW(L"Patching D2Client.dll\n");
    return true;
}