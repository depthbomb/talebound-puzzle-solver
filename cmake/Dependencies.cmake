# Find the existing vcpkg installation for both presets and CLion's default profile.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    find_program(TALEBOUND_VCPKG_EXECUTABLE NAMES vcpkg HINTS "$ENV{VCPKG_ROOT}")
    if(TALEBOUND_VCPKG_EXECUTABLE)
        get_filename_component(TALEBOUND_VCPKG_ROOT "${TALEBOUND_VCPKG_EXECUTABLE}" DIRECTORY)
        set(CMAKE_TOOLCHAIN_FILE "${TALEBOUND_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE FILEPATH "Installed vcpkg toolchain")
    endif()
endif()

set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "Installed wxWidgets triplet")
set(VCPKG_MANIFEST_MODE OFF CACHE BOOL "Use existing dependencies")
set(VCPKG_MANIFEST_INSTALL OFF CACHE BOOL "Do not install dependencies during configuration")
