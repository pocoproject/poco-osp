#
# BundleFunctionTest.cmake
#
# Tests of poco_osp_bundle() in PocoOspBundle.cmake with the project in
# BundleFunction and the bundle creator of the build.
#
# Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#
# Variables: BUNDLE_CREATOR (the bundle creator), GENERATOR (the CMake
# generator), CXX_COMPILER (the C++ compiler of the build), WORK_DIR (a
# directory for this test only, removed afterwards).
#

include("${CMAKE_CURRENT_LIST_DIR}/../PocoOSPScriptTest.cmake")

file(REMOVE_RECURSE "${WORK_DIR}")
# A copy of the project, whose spec the test changes.
set(SOURCE "${WORK_DIR}/source")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/BundleFunction/" DESTINATION "${SOURCE}")
set(BUILD "${WORK_DIR}/build")
set(CONFIGURE "${CMAKE_COMMAND}" -S "${SOURCE}" -B "${BUILD}" -G "${GENERATOR}"
	-DMODULE_DIR=${CMAKE_CURRENT_LIST_DIR}/.. -DBUNDLE_CREATOR=${BUNDLE_CREATOR})

poco_osp_expect_exit(${EXIT_OK} "configure" ${CONFIGURE})
# An older version of the bundle, left in the bundle directory by an earlier build.
file(WRITE "${BUILD}/bundles/com.example.a_0.9.0.bndl" "an older version\n")
poco_osp_expect_exit(${EXIT_OK} "build" "${CMAKE_COMMAND}" --build "${BUILD}")
poco_osp_expect_exit(${EXIT_OK} "install the component A" "${CMAKE_COMMAND}" --install "${BUILD}" --component A
	--prefix "${WORK_DIR}/install")
file(GLOB installed RELATIVE "${WORK_DIR}/install" "${WORK_DIR}/install/*/*")
poco_osp_expect("the component installs its bundle file and no other one (${installed})"
	installed STREQUAL "bundles/com.example.a_1.0.0.bndl")

poco_osp_wait_for_newer_time()
file(READ "${SOURCE}/a/a.bndlspec" spec)
string(REPLACE "<version>1.0.0</version>" "<version>1.0.1</version>" spec "${spec}")
file(WRITE "${SOURCE}/a/a.bndlspec" "${spec}")
poco_osp_expect_exit(${EXIT_OK} "build after a version change" "${CMAKE_COMMAND}" --build "${BUILD}")
poco_osp_expect_exit(${EXIT_OK} "install the component A after a version change" "${CMAKE_COMMAND}" --install
	"${BUILD}" --component A --prefix "${WORK_DIR}/install-changed")
file(GLOB installed RELATIVE "${WORK_DIR}/install-changed" "${WORK_DIR}/install-changed/*/*")
poco_osp_expect("after a version change the component installs the bundle file of the new version (${installed})"
	installed STREQUAL "bundles/com.example.a_1.0.1.bndl")

execute_process(COMMAND ${CONFIGURE} -DMISSPELT_KEYWORD=ON
	RESULT_VARIABLE result
	OUTPUT_VARIABLE output
	ERROR_VARIABLE output)
poco_osp_expect("a misspelt keyword stops the configure:\n${output}"
	NOT result EQUAL 0 AND output MATCHES "OSP bundle BundleTypo: unknown arguments: DEPEND.BundleA")

# Configures the project with CODE_DEFINED_LATER in its own build directory with the given further
# arguments and checks that the configure stops.
function(configure_late description build)
	execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SOURCE}" -B "${WORK_DIR}/${build}" -G "${GENERATOR}"
			-DMODULE_DIR=${CMAKE_CURRENT_LIST_DIR}/.. -DBUNDLE_CREATOR=${BUNDLE_CREATOR}
			-DCMAKE_CXX_COMPILER=${CXX_COMPILER} -DCODE_DEFINED_LATER=ON ${ARGN}
		RESULT_VARIABLE result
		OUTPUT_VARIABLE output
		ERROR_VARIABLE output)
	poco_osp_expect("${description}:\n${output}"
		NOT result EQUAL 0 AND output MATCHES "OSP bundle BundleLate: Late in CODE must be defined before")
endfunction()

configure_late("a library in CODE that is defined after the call stops the configure" late)
# The project stops before it enables C++, so the host needs no compiler for Windows.
configure_late("a library in CODE that is defined after the call stops the configure for Windows too"
	late-windows -DCMAKE_SYSTEM_NAME=Windows)

# Configures the project with GENERATED_VARIABLE in its own build directory with the given further arguments and
# stores the exit status and the output in result and output.
function(configure_variable build)
	execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SOURCE}" -B "${WORK_DIR}/${build}" -G "${GENERATOR}"
			-DMODULE_DIR=${CMAKE_CURRENT_LIST_DIR}/.. -DBUNDLE_CREATOR=${BUNDLE_CREATOR} ${ARGN}
		RESULT_VARIABLE result
		OUTPUT_VARIABLE output
		ERROR_VARIABLE output)
	set(result "${result}" PARENT_SCOPE)
	set(output "${output}" PARENT_SCOPE)
endfunction()

configure_variable(variable -DGENERATED_VARIABLE=webapp)
poco_osp_expect("the variable POCO_OSP_BUNDLE_GENERATED without GENERATED stops the configure:\n${output}"
	NOT result EQUAL 0 AND output MATCHES "OSP bundle BundleVariable: the variable POCO_OSP_BUNDLE_GENERATED")
configure_variable(variable-keyword -DGENERATED_VARIABLE=webapp -DGENERATED_KEYWORD=webapp)
poco_osp_expect("the variable POCO_OSP_BUNDLE_GENERATED with GENERATED is silent:\n${output}"
	result EQUAL 0 AND NOT output MATCHES "POCO_OSP_BUNDLE_GENERATED")
configure_variable(variable-keyword-empty -DGENERATED_VARIABLE=webapp -DGENERATED_KEYWORD=)
poco_osp_expect("the variable POCO_OSP_BUNDLE_GENERATED with GENERATED and no directories is silent:\n${output}"
	result EQUAL 0 AND NOT output MATCHES "POCO_OSP_BUNDLE_GENERATED")
configure_variable(variable-empty -DGENERATED_VARIABLE=)
poco_osp_expect("the empty variable POCO_OSP_BUNDLE_GENERATED is silent:\n${output}"
	result EQUAL 0 AND NOT output MATCHES "POCO_OSP_BUNDLE_GENERATED")

file(REMOVE_RECURSE "${WORK_DIR}")
