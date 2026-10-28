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
# Build:
#   cmake -B build -S .
#   cmake --build build --config Release --parallel
# Run (from the example folder):
#   ./build/BACnetExampleBBC          (Linux/macOS)
#   .\build\Release\BACnetExampleBBC.exe   (Windows)
# =============================================================================

cmake_minimum_required(VERSION 3.15)
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

# --- BACnet/IPv6 --------------------------------------------------------------
# cmake -B build-ipv6 -S . -DBBC_IPV6=ON builds the same device as a BACnet/IPv6 (Annex U) node instead of a BACnet/IP one:
# IPv6 is then the ONLY data link the stack is built with (ipv6/cas_bacnet_stack_options_ipv6.h), the socket is
# common/SimpleUDP6.cpp, and the executable is named BACnetExampleBBCIPv6. See ipv6/README.md.
option(BBC_IPV6 "Build the BACnet/IPv6 (Annex U) device instead of the BACnet/IP one" OFF)

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

# --- The CAS BACnet Stack, via its CMake adapter target ---------------------
# CAS_BACNET_STACK_LINK is the adapter's own cache variable (SOURCE/STATIC/DLL,
# default SOURCE, which is what this example builds with). To override it, set it
# BEFORE add_subdirectory() so the adapter target picks it up. See
# submodules/cas-bacnet-stack/adapters/cpp/CMakeLists.txt for what each mode does.
add_subdirectory("${CAS_STACK_DIR}/adapters/cpp" cas-bacnet-stack-adapter)

# --- The stack's options, for the IPv6 build ----------------------------------
# The stack picks its option set from source/CIBuildSettingsDefaults.h unless told otherwise, and that is the FULL target with every
# data link. The IPv6 build forces its own header in front of every stack source (DONT_BUILD_ALL_OPTIONS says the defaults do not apply).
if(BBC_IPV6)
    set(BBC_STACK_OPTIONS_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/ipv6/cas_bacnet_stack_options_ipv6.h")
    target_compile_definitions(cas_bacnet_stack_adapter PUBLIC DONT_BUILD_ALL_OPTIONS)
    if(MSVC)
        target_compile_options(cas_bacnet_stack_adapter PUBLIC "/FI${BBC_STACK_OPTIONS_HEADER}")
    else()
        target_compile_options(cas_bacnet_stack_adapter PUBLIC "-include" "${BBC_STACK_OPTIONS_HEADER}")
    endif()
endif()

# --- The shared example helper ----------------------------------------------
set(COMMON_SOURCES
    "${EXAMPLES_COMMON_DIR}/CASExampleHelper.cpp"
    "${EXAMPLES_COMMON_DIR}/SimpleUDP.cpp")   # CASExampleHelper's own BACnet/IP socket, which this example does not open
if(BBC_IPV6)
    list(APPEND COMMON_SOURCES "${EXAMPLES_COMMON_DIR}/SimpleUDP6.cpp")
endif()

# --- The executable ---------------------------------------------------------
if(BBC_IPV6)
    set(BBC_TARGET_NAME BACnetExampleBBCIPv6)
else()
    set(BBC_TARGET_NAME BACnetExampleBBC)
endif()
add_executable(${BBC_TARGET_NAME}
    main.cpp
    ${COMMON_SOURCES})
if(BBC_IPV6)
    target_compile_definitions(${BBC_TARGET_NAME} PRIVATE BBC_IPV6)
endif()

target_include_directories(${BBC_TARGET_NAME} PRIVATE
    "${EXAMPLES_COMMON_DIR}")

target_link_libraries(${BBC_TARGET_NAME} PRIVATE CASBACnetStack::Adapter)

# --- Warnings on OUR sources only -------------------------------------------
# Apply strict warnings to the example + helper sources, but NOT to the bundled
# third-party stack (~600 files, built inside the CASBACnetStack::Adapter target)
# which would be noisy and isn't ours to fix.
if(MSVC)
    set(EXAMPLE_WARNINGS "/W4")
else()
    set(EXAMPLE_WARNINGS "-Wall;-Wextra")
endif()
set_source_files_properties(main.cpp ${COMMON_SOURCES}
    PROPERTIES COMPILE_OPTIONS "${EXAMPLE_WARNINGS}")

# --- Platform sockets -------------------------------------------------------
# ws2_32  = Winsock (UDP); iphlpapi = GetAdaptersInfo (local broadcast address).
if(WIN32)
    target_link_libraries(${BBC_TARGET_NAME} PRIVATE ws2_32 iphlpapi)
endif()
