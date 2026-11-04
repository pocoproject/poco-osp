#
# PocoOSPScriptTest.cmake
#
# Functions for the tests of the command-line tools, which are CMake
# scripts run with cmake -P. A script sets WORK_DIR, the directory the
# programs run in, before it calls them.
#
# Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#

# Exit statuses of Poco::Util::Application
set(EXIT_OK 0)
set(EXIT_USAGE 64)
set(EXIT_DATAERR 65)

# The prefix of an option of Poco::Util::Application, which takes options as
# /<name>[=<value>] on Windows and as --<name>[=<value>] elsewhere.
if(WIN32)
	set(OPT "/")
else()
	set(OPT "--")
endif()

# poco_osp_expect_exit(<expected> <description> <program> [<argument>...])
# Runs the program with the arguments in WORK_DIR and checks its exit status.
# <expected> is an exit status, or FAILURE for any status other than 0.
# A mismatch is reported with SEND_ERROR, so that the script runs the
# remaining checks and fails at its end.
function(poco_osp_expect_exit expected description program)
	execute_process(COMMAND "${program}" ${ARGN}
		WORKING_DIRECTORY "${WORK_DIR}"
		RESULT_VARIABLE result
		OUTPUT_VARIABLE output
		ERROR_VARIABLE output)
	list(JOIN ARGN " " arguments)
	get_filename_component(name "${program}" NAME_WE)
	if(expected STREQUAL "FAILURE")
		if(result EQUAL 0)
			message(SEND_ERROR "${description}: ${name} ${arguments} succeeded, expected a failure:\n${output}")
			return()
		endif()
	elseif(NOT result STREQUAL "${expected}")
		message(SEND_ERROR "${description}: ${name} ${arguments} exited with ${result}, expected ${expected}:\n${output}")
		return()
	endif()
	message(STATUS "${description}: exit ${result}")
endfunction()

# poco_osp_expect(<description> <condition>...)
# Checks a condition in the syntax of if().
function(poco_osp_expect description)
	if(${ARGN})
		message(STATUS "${description}")
	else()
		message(SEND_ERROR "not true: ${description}")
	endif()
endfunction()
