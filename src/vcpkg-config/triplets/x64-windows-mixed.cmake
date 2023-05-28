set(VCPKG_TARGET_ARCHITECTURE x64)

# This custom triplet lets us compile some dependencies as static and some as dynamic.
# Importantly, this lets us keep some things (such as ffmpeg) dynamic, but all of its dependencies are static

set(VCPKG_CRT_LINKAGE dynamic)

if(${PORT} MATCHES "boost-|lua-lsqlite3")
    # set(VCPKG_CRT_LINKAGE static)
    set(VCPKG_LIBRARY_LINKAGE static)
else()
    # set(VCPKG_CRT_LINKAGE dynamic)
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()
