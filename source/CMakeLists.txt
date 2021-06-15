project(D2.Detours)

set(D2_detours_SOURCES
    src/DetoursDllMain.cpp
    src/DetoursHelpers.cpp
    src/DetoursPatch.cpp
    src/DetoursAutoPatchDirectory.cpp
    # .def file for the dll exports
    src/D2.Detours.def
    # The patches
    src/D2CMP.detours.cpp
)

set(D2_detours_HEADERS
    include/Log.h
    include/DetoursHelpers.h
    include/DetoursPatch.h
    include/D2CMP.detours.h
)

add_library(D2.Detours 
    SHARED # We want to create a detours dll
    ${D2_detours_SOURCES} ${D2_detours_HEADERS}
)
target_include_directories(D2.Detours PUBLIC include)

target_link_libraries(D2.Detours
    PRIVATE
        fmt::fmt
        Detours::Detours
        # Required for some of the Windows libs we use
        Shlwapi.lib
        PathCch.lib
)

target_compile_definitions(D2.Detours
    PRIVATE
        # This define is used to differentiate between the detours DLL and other header consumers such as patches
        -DDETOURS_PATCH_PRIVATE
        # Targetting windows 7 for now
        -DNTDDI_VERSION=NTDDI_WIN7
        -D_WIN32_WINNT=_WIN32_WINNT_WIN7 
        # For faster build
        -DWIN32_LEAN_AND_MEAN
        -DNOCOMM
        # To remove annoying stuff
        -DNOMINMAX
)

add_executable(D2.DetoursLauncher src/DetoursLauncher.cpp)

target_include_directories(D2.DetoursLauncher PRIVATE include)

target_link_libraries(D2.DetoursLauncher
    PRIVATE
        fmt::fmt
        Detours::Detours
        PathCch.lib
)

add_dependencies(D2.DetoursLauncher D2.Detours)
