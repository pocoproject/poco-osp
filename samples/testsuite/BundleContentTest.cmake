#
# BundleContentTest.cmake
#
# Tests that an incremental build of a bundle made with poco_osp_bundle()
# drops the resources that were renamed or removed.
#
# Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#
# Variables: BUNDLE_CREATOR (the BundleCreator program), CMAKE_DIR (the
# directory of PocoOspBundle.cmake), GENERATOR, MAKE_PROGRAM and CONFIG (the
# generator, its build program and the configuration of the build), WORK_DIR
# (a directory for this test only, removed afterwards).
#

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/PocoOSPScriptTest.cmake")

set(SOURCE_DIR "${WORK_DIR}/source")
set(BUILD_DIR "${WORK_DIR}/build")
set(BUNDLE_FILE "${BUILD_DIR}/bundles/osp.test.content_1.0.0.bndl")

# Stores the names of the entries of the bundle file in the given variable.
function(list_bundle_entries variable)
	execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${BUNDLE_FILE}"
		RESULT_VARIABLE result
		OUTPUT_VARIABLE entries
		ERROR_VARIABLE entries)
	if(NOT result EQUAL 0)
		message(SEND_ERROR "the bundle file cannot be listed: ${entries}")
	endif()
	set(${variable} "${entries}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

file(WRITE "${SOURCE_DIR}/CMakeLists.txt" "
cmake_minimum_required(VERSION 3.26)
project(BundleContentTest NONE)
add_executable(Poco::OSP::BundleCreator IMPORTED)
set_target_properties(Poco::OSP::BundleCreator PROPERTIES IMPORTED_LOCATION \"${BUNDLE_CREATOR}\")
include(\"${CMAKE_DIR}/PocoOspBundle.cmake\")
poco_osp_bundle(osp.test.content-bundle
	SPEC \${CMAKE_CURRENT_SOURCE_DIR}/Content.bndlspec
	DESTINATION \${CMAKE_CURRENT_BINARY_DIR}/bundles
)
")
file(WRITE "${SOURCE_DIR}/Content.bndlspec" "<?xml version=\"1.0\"?>
<bundlespec>
  <manifest>
    <name>Content Test</name>
    <symbolicName>osp.test.content</symbolicName>
    <version>1.0.0</version>
    <vendor>Applied Informatics</vendor>
    <copyright>(c) 2026, Applied Informatics Software Engineering GmbH</copyright>
  </manifest>
  <code>
  </code>
  <files>
    bundle/*
  </files>
</bundlespec>
")
file(WRITE "${SOURCE_DIR}/bundle/old/resource.txt" "resource\n")

set(configure_args -S "${SOURCE_DIR}" -B "${BUILD_DIR}" -G "${GENERATOR}")
if(MAKE_PROGRAM)
	list(APPEND configure_args "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
endif()
poco_osp_expect_exit(${EXIT_OK} "configure the bundle project" "${CMAKE_COMMAND}" ${configure_args})
poco_osp_expect_exit(${EXIT_OK} "build the bundle" "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --config "${CONFIG}")
list_bundle_entries(entries)
poco_osp_expect("the bundle contains the resource" entries MATCHES "old/resource\\.txt")

poco_osp_wait_for_newer_time()
# A rename keeps the time of the file, which is older than the bundle.
file(RENAME "${SOURCE_DIR}/bundle/old" "${SOURCE_DIR}/bundle/new")
poco_osp_expect_exit(${EXIT_OK} "build the bundle after the rename" "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --config "${CONFIG}")
list_bundle_entries(entries)
poco_osp_expect("the bundle contains the renamed resource" entries MATCHES "new/resource\\.txt")
poco_osp_expect("the bundle does not contain the resource under its old name" NOT entries MATCHES "old/")

file(REMOVE_RECURSE "${WORK_DIR}")
