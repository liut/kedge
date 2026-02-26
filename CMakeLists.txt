cmake_minimum_required(VERSION 3.20)

project(Kedge VERSION 0.0.2)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

# Build mode override (optional): set to STATIC to force static build
# By default, auto-detects: static if available, otherwise shared
set(BUILD_MODE "" CACHE STRING "Build mode override: STATIC or SHARED (empty for auto-detect)")

# Dependency paths (can be overridden via -DBOOST_ROOT=/path/to/boost etc.)
set(BOOST_ROOT "" CACHE PATH "Boost installation root directory")
set(LT_ROOT "" CACHE PATH "libtorrent-rasterbar installation root directory")
set(ZLIB_ROOT "" CACHE PATH "ZLIB installation root directory")
set(OPENSSL_ROOT "" CACHE PATH "OpenSSL installation root directory")

# Use uppercase for CMake 3.16+ compatibility (CMP0144)
if(BOOST_ROOT)
    set(BOOSTROOT "${BOOST_ROOT}")
endif()

# Auto-detect: prefer static libraries if available
# If BUILD_MODE is set to STATIC, force static; if SHARED, force shared; otherwise auto-detect
if(BUILD_MODE STREQUAL "STATIC")
    message(STATUS "Build mode: FORCE STATIC")
    set(Boost_USE_STATIC_LIBS        ON)
    set(Boost_USE_STATIC_RUNTIME     ON)
elseif(BUILD_MODE STREQUAL "SHARED")
    message(STATUS "Build mode: FORCE SHARED")
    set(Boost_USE_STATIC_LIBS        OFF)
    set(Boost_USE_STATIC_RUNTIME     OFF)
else()
    message(STATUS "Build mode: AUTO (static preferred if available)")
    set(Boost_USE_STATIC_LIBS        ON)
    set(Boost_USE_STATIC_RUNTIME     ON)
endif()

set(Boost_USE_MULTITHREADED      ON)

# ============================================================================
# Find Boost (auto-detect static/shared)
# ============================================================================
set(REQUIRED_BOOST_LIBRARIES
    system
    program_options
    json
    thread
)

find_package(Boost 1.81 QUIET COMPONENTS ${REQUIRED_BOOST_LIBRARIES})

if(Boost_FOUND)
    message(STATUS "Boost found: ${Boost_INCLUDE_DIR}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
else()
    # Static not found, try shared
    message(STATUS "Static Boost not found, trying shared library")
    set(Boost_USE_STATIC_LIBS        OFF)
    set(Boost_USE_STATIC_RUNTIME     OFF)
    find_package(Boost 1.81 REQUIRED COMPONENTS ${REQUIRED_BOOST_LIBRARIES})
    message(STATUS "Boost found (shared): ${Boost_INCLUDE_DIR}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
endif()

# ============================================================================
# Find ZLIB (auto-detect static/shared)
# ============================================================================
find_package(ZLIB REQUIRED)
if(ZLIB_ROOT)
    message(STATUS "ZLIB root: ${ZLIB_ROOT}")
endif()
message(STATUS "ZLIB libraries: ${ZLIB_LIBRARIES}")

# ============================================================================
# Find OpenSSL (auto-detect static/shared)
# ============================================================================
find_package(OpenSSL REQUIRED)
message(STATUS "OpenSSL found: ${OPENSSL_FOUND}")

# ============================================================================
# Find libtorrent (auto-detect static/shared)
# ============================================================================
set(LIBTORRENT_FOUND FALSE)

if(LT_ROOT)
    # User specified custom path, search in provided location
    set(LT_SEARCH_PATHS ${LT_ROOT}/lib ${LT_ROOT}/lib64)
    foreach(path ${LT_SEARCH_PATHS})
        if(EXISTS ${path}/libtorrent-rasterbar.a)
            message(STATUS "Found static libtorrent: ${path}/libtorrent-rasterbar.a")
            add_library(torrent-rasterbar STATIC IMPORTED)
            set_property(TARGET torrent-rasterbar PROPERTY IMPORTED_LOCATION ${path}/libtorrent-rasterbar.a)
            if(EXISTS ${LT_ROOT}/include)
                set_property(TARGET torrent-rasterbar PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LT_ROOT}/include)
            endif()
            set(LIBTORRENT_FOUND TRUE)
            break()
        endif()
    endforeach()

    if(NOT LIBTORRENT_FOUND)
        # Fall back to find_package for shared library
        message(STATUS "Static libtorrent not found, trying shared library")
        find_package(LibtorrentRasterbar REQUIRED)
        set(LIBTORRENT_FOUND TRUE)
    endif()
else()
    # No custom path, try static first then shared
    find_package(LibtorrentRasterbar QUIET)
    if(LibtorrentRasterbar_FOUND)
        set(LIBTORRENT_FOUND TRUE)
    endif()
endif()

if(NOT LIBTORRENT_FOUND)
    message(FATAL_ERROR "libtorrent-rasterbar not found. Please install or specify LT_ROOT")
endif()

# ============================================================================
# Generate const.hpp and configure include directories
# ============================================================================
configure_file(src/const.hpp.in ../include/const.hpp)
include_directories(${PROJECT_BINARY_BIN})
include_directories("include" "src" ${Boost_INCLUDE_DIRS} "deps" "deps/plog/include")

# ============================================================================
# Build executable
# ============================================================================
file(GLOB_RECURSE sources RELATIVE ${CMAKE_SOURCE_DIR} "src/*.cpp")
add_executable(kedge ${sources})

# ============================================================================
# Link libraries (auto-detect based on availability)
# ============================================================================
target_link_libraries(kedge PUBLIC
    Boost::system
    Boost::program_options
    Boost::json
    Boost::thread
)

if(TARGET torrent-rasterbar)
    target_link_libraries(kedge PUBLIC torrent-rasterbar)
else()
    target_link_libraries(kedge PUBLIC LibtorrentRasterbar::torrent-rasterbar)
endif()

target_link_libraries(kedge PUBLIC ${ZLIB_LIBRARIES})

if(TARGET OpenSSL::SSL)
    target_link_libraries(kedge PUBLIC OpenSSL::SSL)
    target_compile_definitions(kedge PUBLIC TORRENT_USE_OPENSSL)
endif()

if(TARGET OpenSSL::Crypto)
    target_link_libraries(kedge PUBLIC OpenSSL::Crypto)
    target_compile_definitions(kedge PUBLIC TORRENT_USE_LIBCRYPTO)
endif()

include(CheckCXXSymbolExists)
check_cxx_symbol_exists(std::filesystem::path::preferred_separator filesystem cxx17fs)

set(LLVM_STATIC_LINK_CXX_STDLIB ON)
set(LLVM_ENABLE_LIBCXX ON)
if (APPLE)
	target_link_libraries(kedge PRIVATE "-framework CoreFoundation" "-framework SystemConfiguration" "-framework Security" )
else()
	# set(CMAKE_EXE_LINKER_FLAGS "-static")
	# include_directories("/usr/include/c++/8")
	target_link_libraries(kedge PRIVATE stdc++fs "-static-libgcc" "-static-libstdc++" "-ldl")
endif()

# target_compile_features(kedge PUBLIC cxx_std_17)

add_definitions(-DBOOST_BEAST_USE_STD_STRING_VIEW -DBOOST_ASIO_HAS_STD_INVOKE_RESULT)

# add_definitions(-DTORRENT_DISABLE_ALERT_MSG)
