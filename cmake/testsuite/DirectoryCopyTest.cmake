#
# DirectoryCopyTest.cmake
#
# Tests of _poco_osp_copy_directory() in PocoOspBundle.cmake with a copy of
# the project in DirectoryCopy: the copy of a directory in the build directory
# follows renamed and removed files, and the commands that follow the copy run
# again when one of their dependencies changes.
#
# Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#
# Variables: GENERATOR (the CMake generator), WORK_DIR (a directory for this
# test only, removed afterwards).
#

include("${CMAKE_CURRENT_LIST_DIR}/../PocoOSPScriptTest.cmake")

file(REMOVE_RECURSE "${WORK_DIR}")
# A copy of the project, whose files the test renames and removes.
set(SOURCE "${WORK_DIR}/source")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/DirectoryCopy/" DESTINATION "${SOURCE}")
set(BUILD "${WORK_DIR}/build")

# Builds the project and stores the names of the files in the copy, separated by spaces, in the
# variable copied.
function(build description)
	poco_osp_expect_exit(${EXIT_OK} "${description}" "${CMAKE_COMMAND}" --build "${BUILD}")
	file(GLOB files RELATIVE "${BUILD}/data" "${BUILD}/data/*")
	list(JOIN files " " files)
	set(copied "${files}" PARENT_SCOPE)
endfunction()

poco_osp_expect_exit(${EXIT_OK} "configure" "${CMAKE_COMMAND}" -S "${SOURCE}" -B "${BUILD}" -G "${GENERATOR}"
	-DMODULE_DIR=${CMAKE_CURRENT_LIST_DIR}/..)
build("build")
poco_osp_expect("the build copies the directory and runs the commands (${copied})"
	copied STREQUAL "a.txt b.txt extra.txt")

poco_osp_wait_for_newer_time()
file(WRITE "${SOURCE}/extra.txt" "changed")
build("build after a change of a dependency")
file(READ "${BUILD}/data/extra.txt" extra)
poco_osp_expect("a changed dependency runs the commands again (${extra})" extra STREQUAL "changed")

poco_osp_wait_for_newer_time()
file(RENAME "${SOURCE}/data/b.txt" "${SOURCE}/data/c.txt")
file(REMOVE "${SOURCE}/data/a.txt")
build("build after a rename and a removal")
poco_osp_expect("the copy follows a renamed and a removed file (${copied})" copied STREQUAL "c.txt extra.txt")

poco_osp_wait_for_newer_time()
file(REMOVE "${SOURCE}/data/c.txt")
build("build after the removal of the last file")
poco_osp_expect("the copy follows the removal of the last file (${copied})" copied STREQUAL "extra.txt")

file(REMOVE_RECURSE "${WORK_DIR}")
