project(D2.detours)

set(D2_detours_SOURCES
    src/main.cpp
    src/DetoursHelpers.cpp
    src/DetoursPatch.cpp
    # .def file for the dll exports
    src/D2.detours.def
    # The patches
    src/D2CMP.detours.cpp
    src/D2Client.detours.cpp
    src/D2Common.detours.cpp
)

set(D2_detours_HEADERS
    include/Log.h
    include/DetoursHelpers.h
    include/DetoursPatch.h
    include/D2CMP.detours.h
    include/D2Client.detours.h
    include/D2Common.detours.h
)

add_library(D2.detours 
    SHARED # We want to create a detours dll
    ${D2_detours_SOURCES} ${D2_detours_HEADERS}
)
target_include_directories(D2.detours PRIVATE include)

target_link_libraries(D2.detours
    PRIVATE
        fmt::fmt
        Shlwapi.lib
        Detours::Detours
)

target_compile_definitions(D2.detours
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




