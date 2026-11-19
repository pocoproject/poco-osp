#
# SymlinkBundleCheck.cmake
#
# A check of the tests of signbundle and stripbundle, which both rewrite
# symlinks.bndl.
#
# Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/PocoOSPScriptTest.cmake")

# expect_link_and_mode_kept(<subject> <bundle> <directory>)
# Extracts <bundle>, a rewritten symlinks.bndl, into <directory> and checks
# that res/link is the symbolic link to target.txt and that res/run.sh is
# executable. <subject> starts the descriptions of the checks.
function(expect_link_and_mode_kept subject bundle directory)
	file(MAKE_DIRECTORY "${directory}")
	execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${bundle}"
		WORKING_DIRECTORY "${directory}"
		COMMAND_ERROR_IS_FATAL ANY)
	set(linkTarget "")
	if(IS_SYMLINK "${directory}/res/link")
		file(READ_SYMLINK "${directory}/res/link" linkTarget)
	endif()
	poco_osp_expect("${subject} keeps the symbolic link" linkTarget STREQUAL "target.txt")
	execute_process(COMMAND "${directory}/res/run.sh" RESULT_VARIABLE result)
	poco_osp_expect("${subject} keeps the mode of the executable file" result STREQUAL "0")
endfunction()
