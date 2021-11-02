cmake_minimum_required(VERSION 3.20)

project(Kedge VERSION 0.0.2)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

option(build_shared "help string describing build_shared" OFF)

if(NOT build_shared)
	message(STATUS "use static build")
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
	set(BUILD_SHARED_LIBS OFF)

	set(Boost_USE_STATIC_LIBS        ON)
	set(Boost_USE_STATIC_RUNTIME     ON)

# include_directories( ${BOOST_ROOT}/include )
	set(BOOST_ROOT "/opt/boost")
	set(LT_ROOT "/opt/lt12")

	include_directories( ${LT_ROOT}/include )
	link_directories( ${LT_ROOT}/lib )

	add_library(ZLIB::ZLIB STATIC IMPORTED)
	add_library(torrent-rasterbar STATIC IMPORTED)
else()
	message(STATUS "use shared build")
endif()

set(Boost_USE_MULTITHREADED      ON)

set(REQUIRED_BOOST_LIBRARIES
    system
    # filesystem
    program_options
    # date_time
    json
    # iostreams
    # timer
    thread
)

find_package(Boost 1.76 REQUIRED COMPONENTS ${REQUIRED_BOOST_LIBRARIES})
message(STATUS "Boost include dir: " ${Boost_INCLUDE_DIR})
message(STATUS "Boost libraries: " ${Boost_LIBRARIES})

configure_file(src/config.hpp.in ../src/config.hpp)
include_directories(${PROJECT_BINARY_BIN})

# include
include_directories( "include" "src" ${Boost_INCLUDE_DIRS}, "deps" "deps/plog/include")

find_package(ZLIB REQUIRED)
message(STATUS "ZLIB libraries: " ${ZLIB_LIBRARIES})

set(sources
	handlers.cpp
	http_session.cpp
	http_websocket.cpp
	listener.cpp
	main.cpp
	sheath.cpp
	util.cpp)

list(TRANSFORM sources PREPEND "src/")

add_executable(kedge ${sources})

if(NOT build_shared)
	set_property(TARGET torrent-rasterbar PROPERTY IMPORTED_LOCATION ${LT_ROOT}/lib/libtorrent-rasterbar.a)

	if(TARGET ZLIB::ZLIB)
		set_property(TARGET ZLIB::ZLIB PROPERTY IMPORTED_LOCATION ${ZLIB_LIBRARIES})
	endif()
else()
	find_package(LibtorrentRasterbar REQUIRED)
endif()
target_link_libraries(kedge PUBLIC ${Boost_LIBRARIES} torrent-rasterbar)

find_package(OpenSSL REQUIRED)
if(TARGET OpenSSL::SSL)
	target_link_libraries(kedge PUBLIC OpenSSL::SSL)
	target_compile_definitions(kedge PUBLIC TORRENT_USE_OPENSSL)
endif()

if(TARGET OpenSSL::Crypto)
	target_link_libraries(kedge PUBLIC OpenSSL::Crypto)
	target_compile_definitions(kedge PUBLIC TORRENT_USE_LIBCRYPTO)
endif()

# target_link_libraries(kedge PUBLIC ZLIB::ZLIB)
target_link_libraries(kedge PUBLIC ${ZLIB_LIBRARIES})

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

add_definitions(-DBOOST_BEAST_USE_STD_STRING_VIEW)

# add_definitions(-DTORRENT_DISABLE_ALERT_MSG)
