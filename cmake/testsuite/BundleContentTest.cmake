#
# BundleContentTest.cmake
#
# Checks that a bundle file of the build contains the given libraries, and,
# except on Windows, where libraries have no RPATH, that its libraries have the
# RPATH of the code cache and none into the build directory.
#
# Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#
# Variables: BUNDLE (a glob expression for the bundle file), LIBRARIES (the
# file names of the libraries), BUILD_DIR (the build directory), WORK_DIR (a
# directory for this test only, removed afterwards).
#

include("${CMAKE_CURRENT_LIST_DIR}/../PocoOSPScriptTest.cmake")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

file(GLOB bundle "${BUNDLE}")
list(LENGTH bundle count)
poco_osp_expect("one bundle file matches ${BUNDLE} (${bundle})" count EQUAL 1)
poco_osp_expect_exit(${EXIT_OK} "list the bundle" "${CMAKE_COMMAND}" -E tar xf "${bundle}")
file(GLOB_RECURSE content RELATIVE "${WORK_DIR}" "${WORK_DIR}/*")
foreach(library IN LISTS LIBRARIES)
	set(found ${content})
	list(FILTER found INCLUDE REGEX "^bin/.*/${library}$")
	poco_osp_expect("the bundle contains ${library} (${content})" found)
endforeach()

list(FILTER content INCLUDE REGEX "^bin/")
if(CMAKE_HOST_WIN32)
	message(WARNING "Windows libraries have no RPATH; the RPATH checks are skipped.")
else()
	foreach(library IN LISTS content)
		if(APPLE)
			set(origin "@loader_path")
			execute_process(COMMAND otool -l "${WORK_DIR}/${library}" OUTPUT_VARIABLE commands
				COMMAND_ERROR_IS_FATAL ANY)
			string(REGEX MATCHALL "cmd LC_RPATH\n[^\n]*\n *path [^\n]* \\(offset" rpath "${commands}")
			list(TRANSFORM rpath REPLACE "^.* path (.*) \\(offset$" "\\1")
		else()
			set(origin "$ORIGIN")
			file(READ_ELF "${WORK_DIR}/${library}" RPATH rpath RUNPATH runpath)
			string(REPLACE ":" ";" rpath "${rpath}:${runpath}")
		endif()
		list(FIND rpath "${origin}" index)
		poco_osp_expect("${library} finds the libraries in the code cache (${rpath})" NOT index EQUAL -1)
		foreach(entry IN LISTS rpath)
			cmake_path(IS_PREFIX BUILD_DIR "${entry}" NORMALIZE inside)
			poco_osp_expect("${library} has no RPATH into the build directory (${entry})" NOT inside)
		endforeach()
	endforeach()
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
