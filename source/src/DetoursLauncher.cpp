#include <Windows.h>
#include <detours.h>
#include <PathCch.h>
#include <DetoursPatch.h>

#define LOG_PREFIX "(D2.DetoursLauncher):"
#include "Log.h"

static void ResumeWaitAndCleanChildProcess(const PROCESS_INFORMATION& pi)
{
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

using ScopedLocalPtr = std::unique_ptr<void, decltype(&::LocalFree)>;

// Retrieve installation directory from the official registry keys
static std::wstring GetInstallDirectory()
{
#define DIABLO2_KEY "Software\\Blizzard Entertainment\\Diablo II"
    const HKEY allowedKeys[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
    for (HKEY baseKey : allowedKeys)
    {
        HKEY    openedKey = 0;
        LSTATUS statusCode = RegOpenKeyExA(baseKey, DIABLO2_KEY, 0, KEY_READ, &openedKey);
        if (statusCode == ERROR_SUCCESS) {
            BYTE       buffer[1024] = { 0 };
            DWORD valueSize = sizeof(buffer) - sizeof(wchar_t); // Values are not necessarily null terminated
            DWORD valueType;
            statusCode = RegQueryValueExW(openedKey, L"InstallPath", 0, &valueType, buffer, &valueSize);
            RegCloseKey(openedKey);
            if (statusCode == ERROR_SUCCESS && valueType == REG_SZ) {
                return { (const wchar_t*)buffer };
            }
        }
    }
    return {};
}

static std::wstring FindD2Executable()
{
    const size_t bufferNbChars = 1 << 10;
    wchar_t buffer[bufferNbChars];

    const wchar_t* Diablo2ExeNames[] = { L"D2SE.exe", L"Game.exe", L"Diablo II.exe" };
    DWORD stringLength = 0;

    // First look in current dir and system PATH
    for (const wchar_t* exeName : Diablo2ExeNames)
    {
        stringLength = SearchPathW(nullptr, exeName, nullptr, bufferNbChars, buffer, nullptr);
        if (stringLength != 0)
            break;
    }
    // If not found, try the game folder
    const std::wstring installDir = GetInstallDirectory();
    if (!installDir.empty() && stringLength == 0)
    {
        for (const wchar_t* exeName : Diablo2ExeNames)
        {
            stringLength = SearchPathW(installDir.c_str(), exeName, nullptr, bufferNbChars, buffer, nullptr);
            if (stringLength != 0)
                break;
        }
    }
    if (stringLength == 0)
        return {};
    else if (stringLength >= bufferNbChars)
    {
        LOG("Path buffer too small, move the file to a shorter path.\n");
        return {};
    }
    else return buffer;
}

static bool IsFile(const wchar_t* szPath)
{
    const DWORD dwAttrib = GetFileAttributesW(szPath);
    return dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring GetD2ExecutablePath(int argc, wchar_t* argv[], bool& overrideChildCurrentDir)
{
    if (argc >= 2)
    {
        if (!IsFile(argv[1]))
        {
            USER_ERRORW(L"The path {} does not point to a file", argv[1]);
            return {};
        }
        return argv[1];
    }
    else
    {
        const std::wstring d2Executable = FindD2Executable();
        if (d2Executable.empty())
        {
            USER_ERRORW(L"D2 executable not found, please provide the executable name as 1st parameter of the command line.");
        }
        overrideChildCurrentDir = true;
        return d2Executable;
    }
}

ScopedLocalPtr GetDirectory(const std::wstring path)
{
    const size_t bufferSizeInBytes = sizeof(wchar_t) * (path.length() + 1);
    void* directoryPath = memcpy(LocalAlloc(LMEM_FIXED, bufferSizeInBytes), path.data(), bufferSizeInBytes);
    PathCchRemoveFileSpec(static_cast<wchar_t*>(directoryPath), bufferSizeInBytes);
    return { directoryPath, ::LocalFree };
}

int wmain(int argc, wchar_t* argv[])
{
    bool overrideChildCurrentDir = false;
    const std::wstring d2ExecPath = GetD2ExecutablePath(argc, argv, overrideChildCurrentDir);
    const ScopedLocalPtr directoryPathPtr = GetDirectory(d2ExecPath);
    if (d2ExecPath.empty())
    {
        return 1;
    }

    const wchar_t* appName = d2ExecPath.c_str();
    wchar_t* commandLine = nullptr;

    const size_t maxPathLen = 2048;
    wchar_t currentModuleFilePath[maxPathLen] = { 0 };
    GetModuleFileNameW(NULL, currentModuleFilePath, maxPathLen);
    PathCchRemoveFileSpec(currentModuleFilePath, maxPathLen);

    
    const wchar_t* detoursDllName = L"D2.Detours.dll";
    wchar_t* finalPatchPath = nullptr;
    if (S_OK != PathAllocCombine(currentModuleFilePath, detoursDllName, PATHCCH_ALLOW_LONG_PATHS, &finalPatchPath))
    {
        return 1;
    }
    ScopedLocalPtr finalPatchPathScoped{ (void*)finalPatchPath, ::LocalFree };

    size_t dllPathAnsiSize = 1 << 15;
    ScopedLocalPtr dllPathAnsi{ LocalAlloc(LMEM_FIXED, dllPathAnsiSize), ::LocalFree };
    if (0 == WideCharToMultiByte(CP_ACP, 0, finalPatchPath, -1, (char*)dllPathAnsi.get(), dllPathAnsiSize, 0, 0))
    {
        USER_ERROR("Paths to detours dll has unsupported characters or is too long.");
        return 1;
    }

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    const DWORD dwFlags = CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED;
    if (DetourCreateProcessWithDllExW(appName, commandLine,
        NULL, NULL, 
        TRUE, dwFlags, 
        NULL, overrideChildCurrentDir ? static_cast<const wchar_t*>(directoryPathPtr.get()) : nullptr,
        &si, &pi, (char*)dllPathAnsi.get(), NULL
        ))
    {
        ResumeWaitAndCleanChildProcess(pi);
        return 0;
    }
    else if (GetLastError() == ERROR_ELEVATION_REQUIRED)
    {
        USER_ERRORW(L"{} needs to be run as administrator.", appName);
    }
    else
    {
        USER_ERROR("Failed with error {}\n", GetLastError());
    }
    return GetLastError();
}

