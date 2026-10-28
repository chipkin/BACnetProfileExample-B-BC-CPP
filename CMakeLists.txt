# =============================================================================
# CMake build for the BACnet B-BC (Building Controller) C++ example.
#
# This example links the CAS BACnet Stack through the CASBACnetStack::Adapter CMake
# target (submodules/cas-bacnet-stack/adapters/cpp) and calls the stack's C API
# (BACnetStack_*, via CASBACnetStackAdapter.h) directly. It builds and ships in the
# adapter's default SOURCE mode: the stack's source/*.cpp are compiled straight into
# the executable, so there is no library or DLL to build, ship, or install first and
# the build is the same two commands on every platform. (The adapter also offers
# STATIC and DLL modes - see its CMakeLists.txt - but they need a prebuilt library
# and this example does not use them.)
#
# ONE executable, four data links: BACnet/IP (default), BACnet/IPv6, MS/TP and
# BACnet/SC, chosen at run time with --datalink. The stack's default (full) build
# already compiles all four. BACnet/IP, BACnet/IPv6 and MS/TP need nothing but the
# operating system. BACnet/SC also needs libwebsockets, OpenSSL and zlib (the
# WebSocket/TLS connection to the hub, and the certificate tools); they are found
# through vcpkg (VCPKG_ROOT) or the system. Without them the example still builds,
# and --datalink sc says how to rebuild with them. -D BBC_SC=OFF leaves them out on
# purpose.
#
# Build:
#   cmake -B build -S .
#   cmake --build build --config Release --parallel
# Run (from the example folder):
#   ./build/BACnetExampleBBC [--datalink ipv4|ipv6|mstp|sc]          (Linux/macOS)
#   .\build\Release\BACnetExampleBBC.exe [--datalink ipv4|ipv6|mstp|sc]   (Windows)
# =============================================================================

cmake_minimum_required(VERSION 3.15)

# --- BACnet/SC (optional) --------------------------------------------------------
# ON (the default) means "build BACnet/SC if its libraries can be found". It is
# not an error when they cannot: the example is then built without it.
option(BBC_SC "Build the BACnet/SC data link (needs libwebsockets, OpenSSL and zlib)" ON)

# vcpkg toolchain auto-detection - only when BACnet/SC is wanted, and only if the
# caller did not choose a toolchain. Must happen BEFORE project(). With VCPKG_ROOT
# set (Visual Studio bundles a vcpkg), vcpkg.json's libraries are installed into
# the build folder at configure time. A machine without vcpkg skips all of this.
if(BBC_SC AND NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT}
   AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
    # On Windows, the static triplet: libwebsockets/OpenSSL linked into the one
    # executable, against the static C runtime (see CMAKE_MSVC_RUNTIME_LIBRARY below).
    if(WIN32 AND NOT DEFINED VCPKG_TARGET_TRIPLET)
        set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "Vcpkg target triplet")
    endif()
endif()

project(BACnetProfileExample_CPP_B_BC CXX C)


# Default to a Release build.
#
# Without this, a single-config generator (Unix Makefiles / Ninja - the default
# on Linux and macOS) leaves CMAKE_BUILD_TYPE EMPTY, and the documented
# `cmake --build build --config Release` is SILENTLY IGNORED there: --config
# only means anything to a multi-config generator like Visual Studio. The result
# is that the whole stack (~600 files) compiles at -O0 and the customer never
# finds out. Windows is unaffected, which is exactly why this survived so long.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Locate the pieces this project ships -----------------------------------
# The shared helper is vendored into this repository at ./common.
if(NOT DEFINED EXAMPLES_COMMON_DIR)
    set(EXAMPLES_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/common")
endif()

# The CAS BACnet Stack is a git submodule of this repository, at
# ./submodules/cas-bacnet-stack. Run `git submodule update --init --recursive`
# after cloning. (Override with -D CAS_STACK_DIR=... to use a stack elsewhere.)
if(NOT DEFINED CAS_STACK_DIR)
    set(CAS_STACK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/submodules/cas-bacnet-stack")
endif()
if(NOT EXISTS "${CAS_STACK_DIR}/adapters/cpp/CMakeLists.txt")
    message(FATAL_ERROR
        "CAS BACnet Stack adapter not found under: ${CAS_STACK_DIR}\n"
        "The stack is a git submodule. After cloning this repository, run:\n"
        "    git submodule update --init --recursive\n"
        "Or pass -D CAS_STACK_DIR=/path/to/cas-bacnet-stack to use a stack elsewhere.")
endif()

# --- BACnet/SC libraries ---------------------------------------------------------
set(BBC_SC_FOUND OFF)
if(BBC_SC)
    find_package(libwebsockets CONFIG QUIET)
    find_package(OpenSSL QUIET)
    find_package(ZLIB QUIET)
    find_package(Threads QUIET)
    if(libwebsockets_FOUND AND OpenSSL_FOUND AND ZLIB_FOUND AND Threads_FOUND)
        set(BBC_SC_FOUND ON)
    else()
        message(STATUS "B-BC: BACnet/SC is NOT built - libwebsockets, OpenSSL or zlib was not found "
                       "(set VCPKG_ROOT, or install them). The other data links are built.")
    endif()
endif()
if(BBC_SC_FOUND)
    message(STATUS "B-BC: BACnet/SC is built (libwebsockets, OpenSSL, zlib found)")
endif()

# --- MSVC runtime library (must match across the whole link) ----------------
# vcpkg's x64-windows-static triplet builds libwebsockets/OpenSSL against the
# static CRT (/MT). When this build links them, compile the stack and this example
# the same way - set BEFORE add_subdirectory(adapter) so the stack's own SOURCE-mode
# compile picks it up - or the link fails with LNK2038 MD-vs-MT mismatches.
if(MSVC AND BBC_SC_FOUND AND VCPKG_TARGET_TRIPLET MATCHES "-static$")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Compile each target's files in parallel with MSVC too (/MP): the stack is ~600
# files. Before add_subdirectory() so the stack's target gets it.
if(MSVC)
    add_compile_options(/MP)
endif()

# --- The CAS BACnet Stack, via its CMake adapter target ---------------------
# CAS_BACNET_STACK_LINK is the adapter's own cache variable (SOURCE/STATIC/DLL,
# default SOURCE, which is what this example builds with). To override it, set it
# BEFORE add_subdirectory() so the adapter target picks it up. See
# submodules/cas-bacnet-stack/adapters/cpp/CMakeLists.txt for what each mode does.
# The stack is built with its default options (source/CIBuildSettingsDefaults.h):
# every object type and every data link - BACnet/IP, BACnet/IPv6, MS/TP, BACnet/SC.
add_subdirectory("${CAS_STACK_DIR}/adapters/cpp" cas-bacnet-stack-adapter)

# --- The shared example helper ----------------------------------------------
set(COMMON_SOURCES
    "${EXAMPLES_COMMON_DIR}/CASExampleHelper.cpp"
    "${EXAMPLES_COMMON_DIR}/SimpleUDP.cpp"      # BACnet/IP (--datalink ipv4)
    "${EXAMPLES_COMMON_DIR}/SimpleUDP6.cpp"     # BACnet/IPv6 (--datalink ipv6)
    "${EXAMPLES_COMMON_DIR}/SimpleSerial.cpp")  # MS/TP (--datalink mstp)

# --- This example's own sources ---------------------------------------------
set(EXAMPLE_SOURCES
    main.cpp
    command_line.cpp                 # --datalink and every other option, --help
    ipv6/LocalIPv6Address.cpp        # the host's IPv6 address for the IPv6 Network Port
    mstp/MstpDataLink.cpp)           # the stack's MS/TP byte callbacks on a serial port

# BACnet/SC: the hub connection (libwebsockets client) and the certificate files
# (CARI request/response, the certificate File objects) - see sc/README.md.
set(SC_SOURCES
    sc/ScNode.cpp
    sc/ScHubConnector.cpp
    sc/KeyPassword.cpp
    sc/cari.cpp
    sc/cert_layout.cpp
    sc/cert_tool.cpp
    sc/cert_store.cpp)

# --- The executable ---------------------------------------------------------
if(BBC_SC_FOUND)
    add_executable(BACnetExampleBBC ${EXAMPLE_SOURCES} ${COMMON_SOURCES} ${SC_SOURCES})
    target_compile_definitions(BACnetExampleBBC PRIVATE BBC_WITH_SC)
    # The libwebsockets package names its target "websockets" (static) or
    # "websockets_shared" (some Linux distributions ship only that one).
    if(TARGET websockets)
        set(BBC_WEBSOCKETS_TARGET websockets)
    else()
        set(BBC_WEBSOCKETS_TARGET websockets_shared)
    endif()
    target_link_libraries(BACnetExampleBBC PRIVATE
        ${BBC_WEBSOCKETS_TARGET} OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB Threads::Threads)
else()
    add_executable(BACnetExampleBBC ${EXAMPLE_SOURCES} ${COMMON_SOURCES})
endif()

target_include_directories(BACnetExampleBBC PRIVATE
    "${EXAMPLES_COMMON_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(BACnetExampleBBC PRIVATE CASBACnetStack::Adapter)

# --- Warnings on OUR sources only -------------------------------------------
# Apply strict warnings to the example + helper sources, but NOT to the bundled
# third-party stack (~600 files, built inside the CASBACnetStack::Adapter target)
# which would be noisy and isn't ours to fix.
if(MSVC)
    set(EXAMPLE_WARNINGS "/W4")
else()
    set(EXAMPLE_WARNINGS "-Wall;-Wextra")
endif()
set_source_files_properties(${EXAMPLE_SOURCES} ${COMMON_SOURCES} ${SC_SOURCES}
    PROPERTIES COMPILE_OPTIONS "${EXAMPLE_WARNINGS}")

# --- Platform libraries -----------------------------------------------------
# ws2_32  = Winsock (UDP); iphlpapi = GetAdaptersInfo / GetAdaptersAddresses (the
# local IPv4 and IPv6 addresses); winmm = timeBeginPeriod (1 ms sleeps for MS/TP).
# dbghelp is needed by libuv, which a static libwebsockets pulls in (BACnet/SC).
if(WIN32)
    target_link_libraries(BACnetExampleBBC PRIVATE ws2_32 iphlpapi winmm)
    if(BBC_SC_FOUND)
        target_link_libraries(BACnetExampleBBC PRIVATE dbghelp)
    endif()
endif()
