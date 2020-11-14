# D2.Detours

A small project to help hooking Diablo2 .dlls

## Usage

This project is using [Detours](https://github.com/microsoft/Detours) to patch functions.

Start by building D2.detours.dll using CMake.

```
cmake -A Win32 -B build
cmake --build build
```

Then use the pre-built `external/Detours/bin.X86/with-dll.exe` executable to inject the detours dll.
For example if you are using D2SE mod manager:

```
with-dll.exe -d:D2.detours.dll D2SE.exe
```

Note that it will spawn D2SE.exe as a subprocess, so you might be interested in the following Visual Studio extension [Microsoft Child Process Debugging Power Tool](https://marketplace.visualstudio.com/items?itemName=vsdbgplat.MicrosoftChildProcessDebuggingPowerTool).

In case where it fails to start with the message `withdll.exe: DetourCreateProcessWithDllEx failed: 740`, try running it with admin privileges.

## Requirements :

- CMake (buildsystem)
- Visual Studio (compiler)
