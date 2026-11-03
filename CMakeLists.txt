cmake_minimum_required(VERSION 3.26)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" POCO_OSP_VERSION LIMIT_COUNT 1)

# C because the pulled POCO builds its bundled C libraries in the same build.
project(PocoOSP VERSION ${POCO_OSP_VERSION} LANGUAGES C CXX)

# The OSP code uses POCO 2.0 APIs, such as Net::ProxyConfig and RemotingNG.
set(POCO_OSP_POCO_MIN_VERSION 2.0.0)
# release resolution only picks POCO releases with these major versions, so a future major cannot change what a
# fresh configure builds.
set(POCO_OSP_POCO_SUPPORTED_MAJORS 2)

if(cxx_std_20 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
	set(CMAKE_CXX_STANDARD 20)
else()
	set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(PocoOSPOptions)

# POCO's default build type, set here so that it applies to the OSP sources too: POCO sets it in its own
# directory scope, which a superproject's other directories do not see.
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

# A superproject that added POCO already provides it.
if(TARGET Poco::Foundation)
	set(POCO_OSP_SUPERBUILD ON)
else()
	set(POCO_OSP_SUPERBUILD OFF)
	include(PocoOSPFetchPoco)
endif()

# POCO applies its sanitizer flags, trace definition and maintainer warnings in its own directory scope, which
# does not reach the OSP targets; the sanitizer flags are applied as in POCO's cmake/DefinePlatformSpecific.cmake.
if(DEFINED POCO_SANITIZEFLAGS AND NOT "${POCO_SANITIZEFLAGS}" STREQUAL "")
	separate_arguments(_poco_osp_san_flags UNIX_COMMAND "${POCO_SANITIZEFLAGS}")
	add_compile_options(${_poco_osp_san_flags})
	add_link_options(${_poco_osp_san_flags})
	unset(_poco_osp_san_flags)
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
if(POCO_OSP_SUPERBUILD)
	message(STATUS "  POCO: superbuild, provided by the superproject")
else()
	message(STATUS "  POCO source: ${POCO_OSP_POCO_SOURCE}")
	if(POCO_OSP_POCO_REF)
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
