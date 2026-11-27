#
# ConfigureTest.cmake
#
# Configures the repository in configurations that the build itself does not
# use and checks the result of the configure.
#
# Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#
# Variables: OSP_SOURCE_DIR (the poco-osp source tree), POCO_SOURCE_DIR (the
# POCO source tree of the build), GENERATOR (the CMake generator), C_COMPILER
# and CXX_COMPILER (the compilers of the build), AUTH_LDAP (ON if the build has
# the LDAP library that OSP Auth LDAP needs), WORK_DIR (a directory for this
# test only, removed afterwards).
#

include("${CMAKE_CURRENT_LIST_DIR}/../PocoOSPScriptTest.cmake")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

# Configures the repository in the build directory with the given options. Sets result and out.
macro(run_configure build)
	execute_process(COMMAND "${CMAKE_COMMAND}" -S "${OSP_SOURCE_DIR}" -B "${WORK_DIR}/${build}" -G "${GENERATOR}"
			-DCMAKE_C_COMPILER=${C_COMPILER} -DCMAKE_CXX_COMPILER=${CXX_COMPILER}
			-DFETCHCONTENT_SOURCE_DIR_POCO=${POCO_SOURCE_DIR} ${ARGN}
		RESULT_VARIABLE result
		OUTPUT_VARIABLE out
		ERROR_VARIABLE out)
endmacro()

# Configures the repository in the build directory with the given options and stores the output in the
# variable output.
function(configure description build)
	run_configure(${build} ${ARGN})
	poco_osp_expect("${description}: the configure succeeds (${result}):\n${out}" result EQUAL 0)
	set(output "${out}" PARENT_SCOPE)
endfunction()

# A cross build takes the bundle creator of the build host from PATH.
file(WRITE "${WORK_DIR}/host/bundle" "#!/bin/sh\nexit 0\n")
file(CHMOD "${WORK_DIR}/host/bundle" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
set(ENV{PATH} "${WORK_DIR}/host:$ENV{PATH}")
configure("a cross build with the bundle signer" cross
	-DCMAKE_SYSTEM_NAME=${CMAKE_HOST_SYSTEM_NAME} -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_HOST_SYSTEM_PROCESSOR}
	-DPOCO_OSP_MINIMAL_BUILD=ON -DENABLE_OSP_BUNDLESIGN=ON)

configure("a static build" static -DBUILD_SHARED_LIBS=OFF -DPOCO_OSP_ENABLE_TESTS=ON)
poco_osp_expect("a static build switches the bundles off"
	output MATCHES "ENABLE_OSP_BUNDLES: switched off, bundles need shared libraries")
poco_osp_expect("a static build switches the tests off, which load bundles"
	output MATCHES "POCO_OSP_ENABLE_TESTS: switched off, the tests load bundles")
execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" -N --test-dir "${WORK_DIR}/static"
	OUTPUT_VARIABLE tests
	ERROR_VARIABLE tests)
poco_osp_expect("a static build has no tests:\n${tests}" tests MATCHES "Total Tests: 0\n")
poco_osp_expect("a static build switches off the parts that need bundles"
	output MATCHES "ENABLE_OSP_WEB: switched off, it needs ENABLE_OSP_BUNDLES")
poco_osp_expect("a static build has no OSP Auth library, whose package would need the LDAP library"
	output MATCHES "ENABLE_OSP_AUTH: switched off, it needs ENABLE_OSP_BUNDLES")

configure("a minimal build with the tests and without the bundle creator" minimal-tests
	-DPOCO_OSP_MINIMAL_BUILD=ON -DPOCO_OSP_ENABLE_TESTS=ON -DENABLE_OSP_BUNDLE_CREATOR=OFF)

# The OSP Auth tests link Poco::Net only for the LDAP tests, so they do not need ENABLE_OSP_NET.
configure("a minimal build with OSP Auth and the tests" minimal-auth
	-DPOCO_OSP_MINIMAL_BUILD=ON -DPOCO_OSP_ENABLE_TESTS=ON -DENABLE_OSP_BUNDLES=ON -DENABLE_OSP_CRYPTO=ON
	-DENABLE_OSP_DATA=ON -DENABLE_OSP_DATA_SQLITE=ON -DENABLE_OSP_AUTH=ON)

# Configures the repository in the build directory with the given project include, which takes C++20 away from
# the compiler, and checks that the configure stops with a message that names the requirement.
function(configure_without_cxx20 description build include)
	run_configure(${build} -DCMAKE_PROJECT_PocoOSP_INCLUDE=${include})
	poco_osp_expect("${description}: the configure fails (${result}):\n${out}" NOT result EQUAL 0)
	poco_osp_expect("${description}: the configure names the requirement:\n${out}"
		out MATCHES "POCO OSP requires a C\\+\\+20 compiler")
endfunction()

file(WRITE "${WORK_DIR}/no-cxx20-feature.cmake"
	"list(FILTER CMAKE_CXX_COMPILE_FEATURES EXCLUDE REGEX \"^cxx_std_2[0-9]$\")\n")
configure_without_cxx20("a compiler without C++20 in CMake's feature list" no-cxx20-feature
	"${WORK_DIR}/no-cxx20-feature.cmake")

# A std::map without contains(), as in a standard library with a partial C++20 implementation.
file(WRITE "${WORK_DIR}/no-cxx20-library/map" "namespace std { template <class K, class V> class map {}; }\n")
file(WRITE "${WORK_DIR}/no-cxx20-library.cmake" "set(CMAKE_REQUIRED_INCLUDES \"${WORK_DIR}/no-cxx20-library\")\n")
configure_without_cxx20("a compiler whose standard library lacks parts of C++20" no-cxx20-library
	"${WORK_DIR}/no-cxx20-library.cmake")

run_configure(cxx17 -DCMAKE_CXX_STANDARD=17)
poco_osp_expect("C++17: the configure fails (${result}):\n${out}" NOT result EQUAL 0)
poco_osp_expect("C++17: the configure names the requirement:\n${out}"
	out MATCHES "POCO OSP requires C\\+\\+20 or later, but CMAKE_CXX_STANDARD is 17")

configure("a later standard" cxx23 -DCMAKE_CXX_STANDARD=23 -DPOCO_OSP_MINIMAL_BUILD=ON)
poco_osp_expect("a later standard is kept" output MATCHES "C\\+\\+ standard: C\\+\\+23\n")

# Writes a stand-in POCO source tree with Poco::Foundation in Foundation/ and the given content of
# Poco/Foundation.h and Poco/StreamCopier.h.
function(write_poco_tree dir foundation stream_copier)
	file(WRITE "${dir}/CMakeLists.txt"
		"cmake_minimum_required(VERSION 3.26)\nproject(StandInPoco NONE)\nadd_subdirectory(Foundation)\n")
	file(WRITE "${dir}/Foundation/CMakeLists.txt"
		"add_library(Foundation INTERFACE)\nadd_library(Poco::Foundation ALIAS Foundation)\n")
	file(WRITE "${dir}/Foundation/include/Poco/Foundation.h" "${foundation}\n")
	file(WRITE "${dir}/Foundation/include/Poco/StreamCopier.h" "${stream_copier}\n")
	file(WRITE "${dir}/RemotingNG/CMakeLists.txt" "\n")
endfunction()

write_poco_tree("${WORK_DIR}/poco-without-api-export" "" "copyStreamN64")
run_configure(without-api-export -DFETCHCONTENT_SOURCE_DIR_POCO=${WORK_DIR}/poco-without-api-export)
# CMake wraps the lines of an error message; join them before matching.
string(REGEX REPLACE "[ \n]+" " " out "${out}")
poco_osp_expect("a POCO without POCO_API_EXPORT: the configure fails (${result}):\n${out}" NOT result EQUAL 0)
poco_osp_expect("a POCO without POCO_API_EXPORT: the configure names the POCO it needs:\n${out}"
	out MATCHES "has no POCO_API_EXPORT in Poco/Foundation.h, which OSP uses.* or POCO main from 2026-11-17 ")

# A superproject that adds a POCO without the counted copy of StreamCopier, and then poco-osp.
write_poco_tree("${WORK_DIR}/superproject/poco" "POCO_API_EXPORT" "")
file(WRITE "${WORK_DIR}/superproject/CMakeLists.txt"
	"cmake_minimum_required(VERSION 3.26)\nproject(Superproject NONE)\nadd_subdirectory(poco)\n"
	"add_subdirectory(\"${OSP_SOURCE_DIR}\" osp)\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${WORK_DIR}/superproject" -B "${WORK_DIR}/superproject-build"
		-G "${GENERATOR}" -DCMAKE_C_COMPILER=${C_COMPILER} -DCMAKE_CXX_COMPILER=${CXX_COMPILER}
	RESULT_VARIABLE result
	OUTPUT_VARIABLE out
	ERROR_VARIABLE out)
string(REGEX REPLACE "[ \n]+" " " out "${out}")
poco_osp_expect("a superproject POCO without copyStreamN64: the configure fails (${result}):\n${out}"
	NOT result EQUAL 0)
poco_osp_expect("a superproject POCO without copyStreamN64: the configure names the POCO it needs:\n${out}"
	out MATCHES "The POCO in ${WORK_DIR}/superproject/poco has no copyStreamN64 in Poco/StreamCopier.h")

# A superproject whose POCO has no Poco::Net, with OSP Auth LDAP: only the LDAP tests of OSP Auth link Poco::Net.
function(configure_without_net build)
	execute_process(COMMAND "${CMAKE_COMMAND}" -S "${WORK_DIR}/superproject-without-net"
			-B "${WORK_DIR}/${build}" -G "${GENERATOR}" -DCMAKE_C_COMPILER=${C_COMPILER}
			-DCMAKE_CXX_COMPILER=${CXX_COMPILER} -DPOCO_MINIMAL_BUILD=ON -DENABLE_XML=ON -DENABLE_JSON=ON
			-DENABLE_UTIL=ON -DENABLE_ZIP=ON -DENABLE_CRYPTO=ON -DENABLE_DATA=ON -DENABLE_DATA_SQLITE=ON
			-DENABLE_CPPUNIT=ON -DENABLE_NET=OFF -DPOCO_OSP_MINIMAL_BUILD=ON -DENABLE_OSP_BUNDLES=ON
			-DENABLE_OSP_CRYPTO=ON -DENABLE_OSP_DATA=ON -DENABLE_OSP_DATA_SQLITE=ON -DENABLE_OSP_AUTH=ON
			-DENABLE_OSP_AUTH_LDAP=ON ${ARGN}
		RESULT_VARIABLE result
		OUTPUT_VARIABLE out
		ERROR_VARIABLE out)
	string(REGEX REPLACE "[ \n]+" " " out "${out}")
	set(result "${result}" PARENT_SCOPE)
	set(out "${out}" PARENT_SCOPE)
endfunction()

if(AUTH_LDAP)
	file(WRITE "${WORK_DIR}/superproject-without-net/CMakeLists.txt"
		"cmake_minimum_required(VERSION 3.26)\nproject(Superproject C CXX)\n"
		"add_subdirectory(\"${POCO_SOURCE_DIR}\" poco)\nadd_subdirectory(\"${OSP_SOURCE_DIR}\" osp)\n")
	configure_without_net(without-net -DPOCO_OSP_ENABLE_TESTS=OFF)
	poco_osp_expect("OSP Auth LDAP without the tests and a POCO without Poco::Net: the configure succeeds \
(${result}):\n${out}" result EQUAL 0)
	configure_without_net(without-net-tests -DPOCO_OSP_ENABLE_TESTS=ON)
	poco_osp_expect("OSP Auth LDAP with the tests and a POCO without Poco::Net: the configure fails \
(${result}):\n${out}" NOT result EQUAL 0)
	poco_osp_expect("OSP Auth LDAP with the tests and a POCO without Poco::Net: the configure names the tests:\n${out}"
		out MATCHES "POCO_OSP_ENABLE_TESTS needs Poco::Net: switch ENABLE_NET ON in the superproject")
else()
	message(STATUS "Skipped: the superproject with OSP Auth LDAP and a POCO without Poco::Net: the build has no LDAP")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
