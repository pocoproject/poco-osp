#
# ScriptTestFunctionsTest.cmake
#
# Tests of poco_osp_expect_exit() and poco_osp_wait_for_newer_time() in
# PocoOSPScriptTest.cmake. Without CASE, the script runs itself once per case
# of poco_osp_expect_exit() and checks the result of each run.
#
# Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#
# Variables: WORK_DIR (a directory for the files of the wait case), CASE (the
# case to run, set by the script itself).
#

include("${CMAKE_CURRENT_LIST_DIR}/../PocoOSPScriptTest.cmake")

if(CASE STREQUAL "exit")
	poco_osp_expect_exit(FAILURE "a failure exit status" /bin/sh -c "exit 3")
	return()
elseif(CASE STREQUAL "signal")
	poco_osp_expect_exit(FAILURE "a program ended by a signal" /bin/sh -c "kill -SEGV $$")
	return()
endif()

# Runs the case and checks whether the check of the case passed.
function(run_case case expected_pass)
	execute_process(COMMAND "${CMAKE_COMMAND}" -DCASE=${case} -P "${CMAKE_CURRENT_LIST_FILE}"
		RESULT_VARIABLE result
		OUTPUT_VARIABLE output
		ERROR_VARIABLE output)
	if(expected_pass)
		poco_osp_expect("FAILURE accepts a ${case} with a failure exit status:\n${output}" result EQUAL 0)
	else()
		poco_osp_expect("FAILURE does not accept a ${case}:\n${output}" NOT result EQUAL 0)
	endif()
endfunction()

run_case(exit TRUE)
run_case(signal FALSE)

# The make of the Makefiles generators can compare file times in whole seconds.
set(GENERATOR "Unix Makefiles")
file(REMOVE_RECURSE "${WORK_DIR}")
file(WRITE "${WORK_DIR}/before" "")
poco_osp_wait_for_newer_time()
file(WRITE "${WORK_DIR}/after" "")
file(TIMESTAMP "${WORK_DIR}/before" before "%s")
file(TIMESTAMP "${WORK_DIR}/after" after "%s")
poco_osp_expect("with Unix Makefiles, a file written after the wait is newer in whole seconds (${before}, ${after})"
	after GREATER before)
file(REMOVE_RECURSE "${WORK_DIR}")
