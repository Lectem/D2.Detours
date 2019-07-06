
#include <Windows.h>
#include <detours.h>

#define LOG_PREFIX "(D2CMP.detours):"
#include "Log.h"

struct TileHeader;
typedef int(__stdcall* D2GetTileFlagsType)(TileHeader* hTile);
D2GetTileFlagsType TrueD2GetTileFlags;

int __stdcall DetouredD2GetTileFlagsType(TileHeader* hTile) 
{
	int flag = TrueD2GetTileFlags(hTile);
    LOG("D2GetTileFlagsType->{}\n", flag);
    return flag;
}

bool patchD2CMP(void* , HMODULE hModule)
{
    LOGW(L"Patching D2CMP.dll\n");
    TrueD2GetTileFlags = (D2GetTileFlagsType)GetProcAddress(hModule, (LPCSTR)10079);
    assert(TrueD2GetTileFlags);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)TrueD2GetTileFlags, DetouredD2GetTileFlagsType);
    return NO_ERROR == DetourTransactionCommit();
}
