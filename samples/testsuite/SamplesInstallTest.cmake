#
# SamplesInstallTest.cmake
#
# Tests of the install components of the sample bundles.
#
# Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#
# Variables: BUILD_DIR (the build tree), CONFIG (the build configuration),
# WORK_DIR (a directory for this test only, removed afterwards).
#

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/PocoOSPScriptTest.cmake")

# Stores the names of the bundle files installed below the given directory in the given variable.
function(list_installed_bundles directory variable)
	file(GLOB_RECURSE bundles RELATIVE "${directory}" "${directory}/*.bndl")
	set(${variable} "${bundles}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

poco_osp_expect_exit(${EXIT_OK} "install the Bundles component" "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --config "${CONFIG}" --component Bundles --prefix "${WORK_DIR}/bundles")
list_installed_bundles("${WORK_DIR}/bundles" installed)
poco_osp_expect("the Bundles component installs the OSP bundles" installed MATCHES "osp\\.core_")
list(FILTER installed INCLUDE REGEX "com\\.appinf\\.osp\\.(samples\\.|unit\\.sample_)")
poco_osp_expect("the Bundles component installs no sample bundle (${installed})" NOT installed)

poco_osp_expect_exit(${EXIT_OK} "install the Samples component" "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --config "${CONFIG}" --component Samples --prefix "${WORK_DIR}/samples")
list_installed_bundles("${WORK_DIR}/samples" installed)
poco_osp_expect("the Samples component installs the sample bundles" installed MATCHES "com\\.appinf\\.osp\\.samples\\.webecho_")
poco_osp_expect("the Samples component installs no OSP bundle" NOT installed MATCHES "osp\\.core_")

file(REMOVE_RECURSE "${WORK_DIR}")
