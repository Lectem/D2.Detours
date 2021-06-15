
#include <DetoursHelpers.h>
#include <Windows.h>
#include <detours.h>

#define LOG_PREFIX "(D2CMP.detours):"
#include "Log.h"

struct PL2File;
static PL2File* __stdcall DetouredCreateD2Palette(BYTE* pPal[256])
{
    return GetHookOrdinalInfo<10000>(DetouredCreateD2Palette)
        .realFunction(pPal);
}

BYTE __stdcall DetouredD2GetNearestPaletteIndex(BYTE* pPalette, int nPaletteSize, int nRed, int nGreen, int nBlue)
{
    return GetHookOrdinalInfo<10004>(DetouredD2GetNearestPaletteIndex)
        .realFunction(pPalette, nPaletteSize, nRed, nGreen, nBlue);
}

BYTE __stdcall DetouredD2GetFarthestPaletteIndex(BYTE* pPalette, int nPaletteSize, int nRed, int nGreen, int nBlue)
{
    return GetHookOrdinalInfo<10005>(DetouredD2GetFarthestPaletteIndex)
        .realFunction(pPalette, nPaletteSize, nRed, nGreen, nBlue);
}

struct TileHeader;
static int __stdcall DetouredD2GetTileFlagsType(TileHeader* hTile)
{
    int flag = GetHookOrdinalInfo<10079>(DetouredD2GetTileFlagsType).realFunction(hTile);
    return flag;
}

static DllOrdinalHookTypeless dllOrdinalHooks[]{
    GetHookOrdinalInfo<10000>(DetouredCreateD2Palette),
    GetHookOrdinalInfo<10004>(DetouredD2GetNearestPaletteIndex),
    GetHookOrdinalInfo<10005>(DetouredD2GetFarthestPaletteIndex),
    GetHookOrdinalInfo<10079>(DetouredD2GetTileFlagsType),
};

bool patchD2CMP(LPCWSTR, void*, HMODULE hModule)
{
    LOG("Patching D2CMP.dll\n");
    if (NO_ERROR != DetourTransactionBegin())
    {
        LOG("Failed to start transaction for D2CMP.dll\n");
        return false;
    }
    DetourUpdateThread(GetCurrentThread());

    for (auto& dllOrdinalHook : dllOrdinalHooks)
    {
        dllOrdinalHook.realFunction = GetProcAddress(hModule, (LPCSTR)dllOrdinalHook.ordinal);
        assert(dllOrdinalHook.realFunction);
        if (NO_ERROR != DetourAttach(&dllOrdinalHook.realFunction, dllOrdinalHook.hookFunction))
        {
            LOGW(L"Failed to patch ordinal {} with {}\n", dllOrdinalHook.ordinal, dllOrdinalHook.hookFunction);
            exit(-1);
        }
    }

    return NO_ERROR == DetourTransactionCommit();
}
