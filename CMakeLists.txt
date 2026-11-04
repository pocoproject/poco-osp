cmake_minimum_required(VERSION 3.26)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" POCO_OSP_VERSION LIMIT_COUNT 1)

# C because the pulled POCO builds its bundled C libraries in the same build.
project(PocoOSP VERSION ${POCO_OSP_VERSION} LANGUAGES C CXX)

# The OSP code uses POCO 2.0 APIs, such as Net::ProxyConfig and RemotingNG.
set(POCO_OSP_POCO_MIN_VERSION 2.0.0)
# release resolution only picks POCO releases with these major versions, so a future major cannot change what a
# fresh configure builds.
set(POCO_OSP_POCO_SUPPORTED_MAJORS 2)

# Also in a superbuild, whose POCO may be built with an older standard.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(cxx_std_20 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
	# CMake lists C++20 also for compilers whose standard library lacks parts of it, such as contains().
	include(CheckCXXSourceCompiles)
	check_cxx_source_compiles("#include <map>\nint main() { return std::map<int, int>().contains(0) ? 1 : 0; }"
		POCO_OSP_HAVE_CXX20)
endif()
if(NOT POCO_OSP_HAVE_CXX20)
	message(FATAL_ERROR "POCO OSP requires a C++20 compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} "
		"does not support C++20.")
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(PocoOSPOptions)

# POCO's default build type, set here so that it applies to the OSP sources too: POCO sets it in its own
# directory scope, which a superproject's other directories do not see.
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

include(GNUInstallDirs)
# The installed tools find the OSP and POCO libraries in the library directory of the installation. The setting
# applies only to the targets created after it: in a standalone build also to POCO's, which is added below. A
# superproject owns the install RPATH of the POCO libraries it adds and sets CMAKE_INSTALL_RPATH before adding POCO.
if(NOT DEFINED CMAKE_INSTALL_RPATH)
	if(APPLE)
		set(CMAKE_INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
	else()
		set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
	endif()
endif()

# A superproject that added POCO already provides it.
if(TARGET Poco::Foundation)
	get_target_property(_poco_osp_imported Poco::Foundation IMPORTED)
	if(_poco_osp_imported)
		message(FATAL_ERROR "Poco::Foundation is imported from an installed POCO, which OSP is not built against: "
			"a superproject adds the POCO sources (add_subdirectory or FetchContent) before poco-osp, or "
			"poco-osp is configured on its own and pulls POCO (POCO_OSP_POCO_REPOSITORY).")
	endif()
	unset(_poco_osp_imported)
	set(POCO_OSP_SUPERBUILD ON)
else()
	set(POCO_OSP_SUPERBUILD OFF)
	include(PocoOSPFetchPoco)
endif()

# POCO applies these settings in its own directory scope, which does not reach the OSP targets. The sanitizer flags,
# the MSVC runtime library and the Release stripping follow POCO's cmake/DefinePlatformSpecific.cmake.
if(DEFINED POCO_SANITIZEFLAGS AND NOT "${POCO_SANITIZEFLAGS}" STREQUAL "")
	separate_arguments(_poco_osp_san_flags UNIX_COMMAND "${POCO_SANITIZEFLAGS}")
	add_compile_options(${_poco_osp_san_flags})
	add_link_options(${_poco_osp_san_flags})
	unset(_poco_osp_san_flags)
endif()
if(MSVC)
	if(POCO_MT)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
elseif(APPLE)
	add_link_options($<$<CONFIG:Release>:-Wl,-x>)
else()
	add_link_options($<$<CONFIG:Release>:-s>)
endif()
if(ENABLE_TRACE)
	add_compile_definitions(POCO_ENABLE_TRACE)
endif()
if(ENABLE_COMPILER_WARNINGS AND COMMAND poco_enable_detailed_compiler_warnings)
	poco_enable_detailed_compiler_warnings()
endif()

if(POCO_OSP_ENABLE_TESTS)
	enable_testing()
endif()

include(PocoOSPSetup)

message(STATUS "POCO OSP ${PROJECT_VERSION}")
message(STATUS "  C++ standard: C++${CMAKE_CXX_STANDARD}")
if(POCO_OSP_SUPERBUILD)
	message(STATUS "  POCO: superbuild, provided by the superproject")
else()
	message(STATUS "  POCO source: ${POCO_OSP_POCO_SOURCE}")
	if(POCO_OSP_POCO_REF AND POCO_OSP_POCO_VERSION STREQUAL "release" AND POCO_OSP_POCO_RESOLVED_FALLBACK)
		message(STATUS "  POCO version: release (no release at or above ${POCO_OSP_POCO_MIN_VERSION}, main), ref "
			"${POCO_OSP_POCO_REF}")
	elseif(POCO_OSP_POCO_REF)
		message(STATUS "  POCO version: ${POCO_OSP_POCO_VERSION}, ref ${POCO_OSP_POCO_REF}")
	else()
		message(STATUS "  POCO version: plain source tree, POCO_OSP_POCO_VERSION does not apply")
	endif()
endif()
set(_poco_osp_enabled "")
set(_poco_osp_disabled "")
foreach(_poco_osp_option IN LISTS POCO_OSP_PARTS)
	if(${_poco_osp_option})
		list(APPEND _poco_osp_enabled ${_poco_osp_option})
	elseif(DEFINED POCO_OSP_OFF_REASON_${_poco_osp_option})
		list(APPEND _poco_osp_disabled "${_poco_osp_option}: switched off, ${POCO_OSP_OFF_REASON_${_poco_osp_option}}")
	else()
		list(APPEND _poco_osp_disabled ${_poco_osp_option})
	endif()
endforeach()
list(JOIN _poco_osp_enabled " " _poco_osp_enabled)
message(STATUS "  Enabled: ${_poco_osp_enabled}")
message(STATUS "  Disabled:")
foreach(_poco_osp_line IN LISTS _poco_osp_disabled)
	message(STATUS "    ${_poco_osp_line}")
endforeach()
unset(_poco_osp_enabled)
unset(_poco_osp_disabled)
