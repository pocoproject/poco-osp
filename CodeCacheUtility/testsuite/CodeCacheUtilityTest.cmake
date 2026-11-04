#
# CodeCacheUtilityTest.cmake
#
# Tests of the code cache utility (ccutil).
#
# Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#
# Variables: CCUTIL (the tool), WORK_DIR (a directory for this test only,
# removed afterwards).
#

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/PocoOSPScriptTest.cmake")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/content/META-INF" "${WORK_DIR}/content/bin/TestOS/testarch" "${WORK_DIR}/bundles" "${WORK_DIR}/cache")
file(WRITE "${WORK_DIR}/content/META-INF/manifest.mf"
	"Manifest-Version: 1.0\n"
	"Bundle-Name: Code Cache Test Bundle\n"
	"Bundle-SymbolicName: com.example.codecache\n"
	"Bundle-Version: 1.0.0\n"
	"Bundle-Vendor: Test\n")
file(WRITE "${WORK_DIR}/content/bin/TestOS/testarch/libexample.so" "not a real library\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${WORK_DIR}/bundles/com.example.codecache_1.0.0.bndl" --format=zip META-INF bin
	WORKING_DIRECTORY "${WORK_DIR}/content"
	COMMAND_ERROR_IS_FATAL ANY)
file(WRITE "${WORK_DIR}/cache/marker.txt" "kept unless the code cache is cleaned\n")

poco_osp_expect_exit(${EXIT_OK} "populate the code cache" "${CCUTIL}"
	${OPT}bundles=bundles ${OPT}cache=cache ${OPT}osname=TestOS ${OPT}osarch=testarch)
poco_osp_expect("the library of the bundle is in the code cache" EXISTS "${WORK_DIR}/cache/libexample.so")
poco_osp_expect("the code cache is not cleaned without --clean" EXISTS "${WORK_DIR}/cache/marker.txt")

poco_osp_expect_exit(${EXIT_OK} "clean and populate the code cache" "${CCUTIL}"
	${OPT}clean ${OPT}bundles=bundles ${OPT}cache=cache ${OPT}osname=TestOS ${OPT}osarch=testarch)
poco_osp_expect("the library of the bundle is in the code cache after --clean" EXISTS "${WORK_DIR}/cache/libexample.so")
poco_osp_expect("the code cache is cleaned with --clean" NOT EXISTS "${WORK_DIR}/cache/marker.txt")

file(REMOVE_RECURSE "${WORK_DIR}")
