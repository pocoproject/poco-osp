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
# and CXX_COMPILER (the compilers of the build), WORK_DIR (a directory for this
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

file(REMOVE_RECURSE "${WORK_DIR}")
