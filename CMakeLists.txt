cmake_minimum_required(VERSION 3.8.2)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Do not build in-source. Please remove CMakeCache.txt and the CMakeFiles/ directory. Then build out-of-source.")
endif()

# Put the project early since modules might need to detect the compiler
project("D2.Detours" LANGUAGES C CXX VERSION 1.0.0)

if(NOT ${CMAKE_SIZEOF_VOID_P} EQUAL 4)
    message(FATAL_ERROR "Diablo2 is 32bits only. Invoke CMake with '-A Win32'")
endif()

# Standard CMake modules

include(CMakeDependentOption)# This is a really useful scripts that creates options that depends on other options. It can even be used with generator expressions !

# Custom modules and scripts

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(Warnings)

# External dependencies
add_subdirectory(external EXCLUDE_FROM_ALL)

# It is always easier to navigate in an IDE when projects are organized in folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(source)


###############
## Packaging ##
###############

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(GNUInstallDirs)
    install(TARGETS D2.Detours D2.DetoursLauncher)
    install(FILES README.md TYPE DOC)
    install(FILES LICENSE  TYPE DOC RENAME LICENSE.md)

    install(DIRECTORY external/Detours/bin.X86/ TYPE BIN)
    install(FILES external/Detours/LICENSE.md TYPE DOC RENAME Detours.LICENSE.md)

    set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_LIST_DIR}/README.md)
    set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_LIST_DIR}/LICENSE)
    include(CPack)
endif()

