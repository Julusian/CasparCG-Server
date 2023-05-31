set(VCPKG_TARGET_ARCHITECTURE x64)

# This custom triplet lets us compile some dependencies as static and some as dynamic.
# Importantly, this lets us keep direct dependencies dynamic, but child dependencies static

set(VCPKG_CRT_LINKAGE dynamic)

# Allow reusing cache across different versions of the compiler. This will be fine as long as the same or newer compiler is used
set(VCPKG_DISABLE_COMPILER_TRACKING ON)
set(VCPKG_PLATFORM_TOOLSET v142)

if(${PORT} MATCHES "tbb|sfml|openal|glew|Freeimage|ffmpeg")
    # dynamic link specific dependencies
    set(VCPKG_LIBRARY_LINKAGE dynamic)
else()
    # static link by default
    set(VCPKG_LIBRARY_LINKAGE static)
endif()
