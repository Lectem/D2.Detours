# D2.Detours

A small project to help hooking Diablo2 .dlls

## Building

This project is using [Detours](https://github.com/microsoft/Detours) to patch functions.

It is also using [fmt](https://github.com/fmtlib/fmt) (as a git submodule, clone this repository recursively or use `git submodule update --init` after clone).

Start by building `D2.Detours.dll` and `D2.DetoursLauncher` using CMake.

```sh
# Configure the CMake project
cmake -A Win32 -B build
# Build the release config
cmake --build build --config Release
# Install
cmake --install build --config Release --prefix YOUR_INSTALL_FOLDER
```

## Usage

Then use `D2.DetoursLauncher` to inject the detours dll into the Diablo II process of your choice.
For example if you are using D2SE mod manager:

```sh
D2.DetoursLauncher.exe D2SE.exe
```

Note that it will spawn D2SE.exe as a subprocess, so you might be interested in the following Visual Studio extension [Microsoft Child Process Debugging Power Tool](https://marketplace.visualstudio.com/items?itemName=vsdbgplat.MicrosoftChildProcessDebuggingPowerTool). Then go to `Debug > Other debug targets > Child process debugging settings`, enable & save.

## Requirements :

- CMake (buildsystem)
- Visual Studio (compiler)
