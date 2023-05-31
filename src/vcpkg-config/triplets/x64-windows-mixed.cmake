set(VCPKG_TARGET_ARCHITECTURE x64)

# This custom triplet lets us compile some dependencies as static and some as dynamic.
# Importantly, this lets us keep some things (such as ffmpeg) dynamic, but all of its dependencies are static

set(VCPKG_CRT_LINKAGE dynamic)

if(${PORT} MATCHES "libopenmpt|tesseract")
    # this wants to be static, but there is a bug in the build process https://github.com/microsoft/vcpkg/issues/30030
    set(VCPKG_LIBRARY_LINKAGE dynamic)
elseif(${PORT} MATCHES "tbb|sfml|openal|glew|Freeimage|ffmpeg")
    # dynamic link specific dependencies
    set(VCPKG_LIBRARY_LINKAGE dynamic)
else()
    # static link by default
    set(VCPKG_LIBRARY_LINKAGE static)
endif()
