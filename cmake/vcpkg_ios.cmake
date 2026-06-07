if (NOT DEFINED ENV{VCPKG_ROOT})
    message(FATAL_ERROR "
    iOS Compilation failed, vcpkg is required.
    Please install vcpkg and set the environment variable VCPKG_ROOT
    For example:
    export VCPKG_ROOT=/path/to/vcpkg
    ")
endif()

#[[
    The following packages will be installed:
    "boost-filesystem",
    "boost-icl",
    "boost-program-options",
    "boost-system",
    "boost-variant",
    "curl",
    "openssl",
    "zlib"

    #oh and you also apparently need pkg-config to build openssl, zlib, maybe curl
]]

set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME iOS)
#i don't think we need to set an extra variable for iphonesimulator? pretty sure xcode handles simulator stuff
set(VCPKG_TARGET_TRIPLET arm64-ios) 
set(CMAKE_SYSTEM_PROCESSOR "arm") #needed for capstone to not freak out

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
message("vcpkg_ios.cmake: CMAKE_TOOLCHAIN_FILE was set to ${CMAKE_TOOLCHAIN_FILE}")

execute_process(
    COMMAND vcpkg install --triplet ${VCPKG_TARGET_TRIPLET}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
