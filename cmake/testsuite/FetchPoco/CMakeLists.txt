# A project that pulls POCO with PocoOSPFetchPoco.cmake, as the root CMakeLists.txt does, and prints
# what the configure summary reports next to the POCO source that was configured. POCO_OSP_POCO_MIN_VERSION
# can be set on the command line, so that no release reaches it.

cmake_minimum_required(VERSION 3.26)
project(FetchPocoTest NONE)

if(NOT DEFINED POCO_OSP_POCO_MIN_VERSION)
	set(POCO_OSP_POCO_MIN_VERSION 2.0.0)
endif()
set(POCO_OSP_POCO_SUPPORTED_MAJORS 2)
include("${CMAKE_CURRENT_SOURCE_DIR}/../../PocoOSPFetchPoco.cmake")

message(STATUS "summary source: ${POCO_OSP_POCO_SOURCE}")
message(STATUS "summary ref: ${POCO_OSP_POCO_REF}")
message(STATUS "configured source: ${poco_SOURCE_DIR}")
message(STATUS "summary fallback: ${POCO_OSP_POCO_RESOLVED_FALLBACK}")
