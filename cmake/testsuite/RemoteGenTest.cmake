#
# RemoteGenTest.cmake
#
# Tests of poco_remote_gen() in PocoOspRemoteGen.cmake with a copy of the
# project in RemoteGen, which runs a stand-in for RemoteGenNG, and with the
# RemoteGenNG of the build when it has one.
#
# Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#
# Variables: GENERATOR (the CMake generator), CXX_COMPILER (the C++ compiler
# of the build), WORK_DIR (a directory for this test only, removed afterwards),
# REMOTEGEN_NG (the RemoteGenNG of the build, if it has one) and POCO_BASE (the
# POCO source tree of that RemoteGenNG).
#

include("${CMAKE_CURRENT_LIST_DIR}/../PocoOSPScriptTest.cmake")

file(REMOVE_RECURSE "${WORK_DIR}")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/RemoteGen/" DESTINATION "${WORK_DIR}/source")
set(SOURCE "${WORK_DIR}/source")
set(INCLUDE "${SOURCE}/base/include")
set(GENERATED "${WORK_DIR}/build/generated")

# Sets runs, lists and help_runs to the numbers of runs of each kind of the stand-in for RemoteGenNG.
macro(count_runs)
	foreach(kind IN ITEMS runs lists help-runs)
		set(lines "")
		if(EXISTS "${SOURCE}/${kind}.txt")
			file(STRINGS "${SOURCE}/${kind}.txt" lines)
		endif()
		string(MAKE_C_IDENTIFIER "${kind}" variable)
		list(LENGTH lines ${variable})
	endforeach()
endmacro()

# Configures the project in WORK_DIR/build with the given list of further arguments, from WORK_DIR. Sets result, out,
# runs, lists and help_runs.
macro(run_configure arguments)
	execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SOURCE}" -B "${WORK_DIR}/build" -G "${GENERATOR}"
			-DCMAKE_CXX_COMPILER=${CXX_COMPILER} -DMODULE_DIR=${CMAKE_CURRENT_LIST_DIR}/..
			"-DPOCO_REMOTEGEN_NG=/bin/sh;${SOURCE}/remotegen.sh" ${arguments}
		WORKING_DIRECTORY "${WORK_DIR}"
		RESULT_VARIABLE result
		OUTPUT_VARIABLE out
		ERROR_VARIABLE out)
	count_runs()
endmacro()

# Configures the project with the given further arguments and expects success. Sets output, runs, lists and help_runs.
function(configure description)
	run_configure("${ARGN}")
	poco_osp_expect("${description}: the configure succeeds (${result}):\n${out}" result EQUAL 0)
	set(output "${out}" PARENT_SCOPE)
	set(runs ${runs} PARENT_SCOPE)
	set(lists ${lists} PARENT_SCOPE)
	set(help_runs ${help_runs} PARENT_SCOPE)
endfunction()

# Configures as configure() does and expects the configure to stop with a message that matches the expression, with
# each run of white space as one space. Sets output, runs, lists and help_runs.
function(configure_stopped description expression)
	run_configure("${ARGN}")
	string(REGEX REPLACE "[ \t\r\n]+" " " out "${out}")
	poco_osp_expect("${description} stops the configure (${result}):\n${out}"
		NOT result EQUAL 0 AND out MATCHES "${expression}")
	set(output "${out}" PARENT_SCOPE)
	set(runs ${runs} PARENT_SCOPE)
	set(lists ${lists} PARENT_SCOPE)
	set(help_runs ${help_runs} PARENT_SCOPE)
endfunction()

# Builds the project, which configures again when a dependency changed. Sets runs and lists.
function(build description)
	poco_osp_expect_exit(${EXIT_OK} "${description}" "${CMAKE_COMMAND}" --build "${WORK_DIR}/build")
	count_runs()
	set(runs ${runs} PARENT_SCOPE)
	set(lists ${lists} PARENT_SCOPE)
endfunction()

# The headers and the directory that the stand-in for RemoteGenNG lists for the project in SOURCE.
file(WRITE "${SOURCE}/list.txt" "file ${INCLUDE}/Service.h\nfile ${INCLUDE}/Event Service.h\n"
	"directory ${INCLUDE}/more\n")

file(GLOB_RECURSE source_files LIST_DIRECTORIES true RELATIVE "${SOURCE}" "${SOURCE}/*")
configure("first configure")
poco_osp_expect("the first configure runs RemoteGenNG" runs EQUAL 1)
poco_osp_expect("the message names the directory that is written"
	output MATCHES "Generate sources with RemoteGenNG in: ${GENERATED}\n")
poco_osp_expect("the outputs are in the build directory" EXISTS "${GENERATED}/src/Service.cpp")
file(GLOB_RECURSE added LIST_DIRECTORIES true RELATIVE "${SOURCE}" "${SOURCE}/*")
list(REMOVE_ITEM added ${source_files} runs.txt lists.txt help-runs.txt)
poco_osp_expect("a generation adds nothing to the source directory (${added})" NOT added)
string(REGEX REPLACE "[ \t\r\n]+" " " echoed "${output}")
poco_osp_expect("RemoteGenNG gets the source directory and generates without timestamps:\n${echoed}"
	echoed MATCHES "'-DCMAKE_CURRENT_SOURCE_DIR=${SOURCE}' '-DRemoteGen\\.output\\.timestamps=false'")
build("first build")
poco_osp_expect("the first build does not run RemoteGenNG" runs EQUAL 1)

configure("configure without a change")
poco_osp_expect("a configure without a change does not run RemoteGenNG" runs EQUAL 1)
file(GLOB_RECURSE markers LIST_DIRECTORIES false "${WORK_DIR}/.poco_remote_gen_*")
poco_osp_expect("a configure leaves no marker file in the existing output directory (${markers})" NOT markers)
poco_osp_expect("a configure without a change reports nothing at the default log level"
	NOT output MATCHES "Generated sources")
configure("configure without a change, verbose" --log-level=VERBOSE)
poco_osp_expect("the verbose message names the directory of the generated sources"
	output MATCHES "Generated sources are up to date: ${GENERATED}\n")

file(TOUCH "${INCLUDE}/Service.h")
configure("configure after a change of a listed header")
poco_osp_expect("a configure after a change of a listed header runs RemoteGenNG again" runs EQUAL 2)
file(TOUCH "${INCLUDE}/Service.h")
build("build after a change of a listed header")
poco_osp_expect("a build after a change of a listed header runs RemoteGenNG again" runs EQUAL 3)
file(TOUCH "${INCLUDE}/Event Service.h")
build("build after a change of a listed header with a space in its name")
poco_osp_expect("a build after a change of a listed header with a space in its name runs RemoteGenNG again"
	runs EQUAL 4)

set(lists_before ${lists})
file(WRITE "${INCLUDE}/more/Added.h" "")
build("build after a file was added to a listed directory")
poco_osp_expect("a file added to a listed directory configures again (lists: ${lists_before}, ${lists})"
	lists GREATER lists_before)
poco_osp_expect("a file added to a listed directory without a change of the list does not run RemoteGenNG"
	runs EQUAL 4)
# The list names the new directory, whose time is not checked.
file(APPEND "${SOURCE}/list.txt" "directory ${INCLUDE}/more/new\n")
file(MAKE_DIRECTORY "${INCLUDE}/more/new")
build("build after a directory was added to a listed directory and to the list")
poco_osp_expect("a list that a build changes runs RemoteGenNG again" runs EQUAL 5)
file(WRITE "${SOURCE}/list.txt" "file ${INCLUDE}/Service.h\nfile ${INCLUDE}/Event Service.h\n"
	"directory ${INCLUDE}/more\n")
configure("configure after a change of the list alone")
poco_osp_expect("a list that changes without a change of a time runs RemoteGenNG again" runs EQUAL 6)
file(APPEND "${SOURCE}/list.txt" "file ${INCLUDE}/Missing.h\n")
configure("configure with a listed header that does not exist")
configure("configure with a listed header that does not exist, again")
poco_osp_expect("a listed header that does not exist runs RemoteGenNG at every configure" runs EQUAL 8)
file(WRITE "${SOURCE}/list.txt" "file ${INCLUDE}/Service.h\nfile ${INCLUDE}/Event Service.h\n"
	"directory ${INCLUDE}/more\n")
configure("configure after the listed header that does not exist was removed from the list")

file(REMOVE_RECURSE "${GENERATED}")
file(TOUCH "${SOURCE}/fail")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SOURCE}" -B "${WORK_DIR}/build" -G "${GENERATOR}"
		-DCMAKE_CXX_COMPILER=${CXX_COMPILER} -DMODULE_DIR=${CMAKE_CURRENT_LIST_DIR}/..
		"-DPOCO_REMOTEGEN_NG=/bin/sh;${SOURCE}/remotegen.sh"
	RESULT_VARIABLE result
	OUTPUT_QUIET
	ERROR_QUIET)
poco_osp_expect("a RemoteGenNG run that fails after it wrote part of the directory stops the configure (${result})"
	NOT result EQUAL 0)
poco_osp_expect("a RemoteGenNG run that fails adds no output to the build directory" NOT EXISTS "${GENERATED}")
file(REMOVE "${SOURCE}/fail")
configure("configure after a failed run")
file(STRINGS "${GENERATED}/src/Service.cpp" generated)
poco_osp_expect("a configure after a failed run runs RemoteGenNG again (${generated})"
	runs EQUAL 10 AND generated STREQUAL "// generated")

file(TOUCH "${SOURCE}/RemoteGen.xml")
configure("configure after a change of the configuration")
poco_osp_expect("a configuration newer than the last generation runs RemoteGenNG again" runs EQUAL 11)
file(TOUCH "${SOURCE}/RemoteGen.xml")
build("build after a change of the configuration")
poco_osp_expect("a build after a change of the configuration runs RemoteGenNG again" runs EQUAL 12)

file(CHMOD "${SOURCE}/remotegen.sh" FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
set(executable "-DPOCO_REMOTEGEN_NG=${SOURCE}/remotegen.sh")
configure("configure with the stand-in for RemoteGenNG as the executable" "${executable}")
poco_osp_expect("another RemoteGenNG command runs RemoteGenNG again" runs EQUAL 13)
configure("configure with the executable without a change" "${executable}")
poco_osp_expect("an executable older than the last generation does not run RemoteGenNG" runs EQUAL 13)
file(TOUCH "${SOURCE}/remotegen.sh")
configure("configure after a change of the executable" "${executable}")
poco_osp_expect("an executable newer than the last generation runs RemoteGenNG again" runs EQUAL 14)
configure("configure with the executable without a change, again" "${executable}")
poco_osp_expect("a configure without a change of the executable does not run RemoteGenNG" runs EQUAL 14)
# The configure runs outside the source directory, in which the relative path names the executable.
set(relative_executable "-DPOCO_REMOTEGEN_NG=./remotegen.sh")
configure("configure with the executable named relative to the source directory" "${relative_executable}")
file(TOUCH "${SOURCE}/remotegen.sh")
configure("configure after a change of the executable named relative to the source directory"
	"${relative_executable}")
poco_osp_expect("an executable named relative to the source directory and newer than the last generation runs it"
	runs EQUAL 16)
# A RemoteGenNG named without a directory is looked up on PATH, so a file of that name in the working directory of
# cmake is not RemoteGenNG.
set(path_executable "-DPOCO_REMOTEGEN_NG=sh\;${SOURCE}/remotegen.sh")
configure("configure with a RemoteGenNG named without a directory" "${path_executable}")
file(WRITE "${WORK_DIR}/sh" "")
configure("configure with a RemoteGenNG named without a directory and a newer file of that name in WORK_DIR"
	"${path_executable}")
poco_osp_expect("a newer file in the working directory of cmake with the name of RemoteGenNG does not run it"
	runs EQUAL 17)
file(REMOVE "${WORK_DIR}/sh")
configure("configure with the stand-in for RemoteGenNG run by /bin/sh again")
poco_osp_expect("a configure back to the first RemoteGenNG command runs RemoteGenNG again" runs EQUAL 18)

configure("configure with a second call in the directory" -DCLIENT=ON)
poco_osp_expect("a second call in the directory runs RemoteGenNG for its directory" runs EQUAL 19)
math(EXPR expected_help_runs "${help_runs} + 1")
configure("configure with two calls in the directory without a change")
poco_osp_expect("a configure with two calls reads the help of RemoteGenNG once (${help_runs})"
	help_runs EQUAL expected_help_runs)
configure("configure with two calls in the directory without a change, again")
poco_osp_expect("a configure without a change runs RemoteGenNG for neither of two calls in the directory"
	runs EQUAL 19)
file(TOUCH "${SOURCE}/RemoteGen.xml" "${SOURCE}/RemoteGenClient.xml")
configure("configure after a change of the configurations of two calls in the directory")
poco_osp_expect("a change of the configurations of two calls in the directory runs RemoteGenNG for both"
	runs EQUAL 21)

# Configures with two calls and a RemoteGenNG without the option list-dependencies, and expects one warning and a
# generation of each call.
function(configure_without_list description)
	math(EXPR expected_runs "${runs} + 2")
	configure("${description}")
	string(REGEX MATCHALL "does not name the option list-dependencies" warnings "${output}")
	list(LENGTH warnings count)
	poco_osp_expect("${description} warns once for two calls (${count}):\n${output}" count EQUAL 1)
	poco_osp_expect("${description} runs RemoteGenNG for both calls" runs EQUAL expected_runs)
	set(output "${output}" PARENT_SCOPE)
	set(runs ${runs} PARENT_SCOPE)
endfunction()

file(WRITE "${SOURCE}/no-list" "")
configure_without_list("a configure with a RemoteGenNG without the option list-dependencies")
string(REGEX REPLACE "[ \t\r\n]+" " " output "${output}")
poco_osp_expect("the warning names RemoteGenNG" output MATCHES
	"the help of RemoteGenNG /bin/sh ${SOURCE}/remotegen\\.sh does not name the option list-dependencies, so it")
configure_without_list("a second configure with a RemoteGenNG without the option list-dependencies")
file(REMOVE "${SOURCE}/no-list")
configure("configure with a RemoteGenNG that lists again")

# The stand-in writes the list before it fails, so that only the exit status shows the failure.
set(runs_before ${runs})
math(EXPR lists_limit "${lists} + 1")
file(WRITE "${SOURCE}/list-fail" "")
configure_stopped("a list run of RemoteGenNG that fails" "remotegen.sh: the list run fails")
poco_osp_expect("a list run of RemoteGenNG that fails stops the configure before a generation or a second list run"
	runs EQUAL runs_before AND lists LESS_EQUAL lists_limit)
file(REMOVE "${SOURCE}/list-fail")

file(WRITE "${SOURCE}/list.txt" "file ${INCLUDE}/Service.h")
configure_stopped("a list whose last line has no line feed" "does not end with a line feed")
file(WRITE "${SOURCE}/list.txt" "file ${INCLUDE}/Service.h\nheader ${INCLUDE}/Event Service.h\n")
configure_stopped("a list with a line that names neither a file nor a directory"
	"has a line that is neither file <path> nor directory <path>: header ")
file(WRITE "${SOURCE}/list.txt" "file ${INCLUDE}/Service.h\nfile ${INCLUDE}/Event Service.h\n"
	"directory ${INCLUDE}/more\n")

# A list run that writes no list stops the configure, which does not read the list that the last configure left.
set(runs_before ${runs})
file(WRITE "${SOURCE}/list-silent" "")
configure_stopped("a list run of RemoteGenNG that writes no list" "exited with 0 but did not write the list")
poco_osp_expect("a list run of RemoteGenNG that writes no list stops the configure before a generation"
	runs EQUAL runs_before)
file(REMOVE "${SOURCE}/list-silent")

file(WRITE "${SOURCE}/change-header" "")
file(TOUCH "${SOURCE}/RemoteGen.xml")
configure("configure in which RemoteGenNG changes a listed header while it generates")
math(EXPR expected_runs "${runs} + 1")
configure("configure after RemoteGenNG changed a listed header while it generated")
poco_osp_expect("a listed header changed while RemoteGenNG generates runs RemoteGenNG again at the next configure"
	runs EQUAL expected_runs)

math(EXPR expected_runs "${runs} + 1")
configure("configure with a property in DEFINES" -DEXTRA_DEFINES=A=1)
poco_osp_expect("a property added to DEFINES runs RemoteGenNG again" runs EQUAL expected_runs)
math(EXPR expected_runs "${runs} + 1")
configure("configure after a change of a property in DEFINES" -DEXTRA_DEFINES=A=2)
poco_osp_expect("a changed value of a property in DEFINES runs RemoteGenNG again" runs EQUAL expected_runs)
configure("configure without a change of DEFINES")
poco_osp_expect("a configure without a change of DEFINES does not run RemoteGenNG" runs EQUAL expected_runs)
# The copy keeps the time of the configuration, which is older than the last generation.
file(COPY "${SOURCE}/RemoteGen.xml" DESTINATION "${SOURCE}/copy")
math(EXPR expected_runs "${runs} + 1")
configure("configure with a copy of the configuration" -DCONFIG=copy/RemoteGen.xml)
poco_osp_expect("another path of the configuration runs RemoteGenNG again" runs EQUAL expected_runs)
math(EXPR expected_runs "${runs} + 1")
configure("configure with the configuration again" -DCONFIG=RemoteGen.xml)
poco_osp_expect("the path of the configuration before the copy runs RemoteGenNG again" runs EQUAL expected_runs)

set(service "${GENERATED}/src/Service.cpp")
set(extra "${GENERATED}/src/Extra.cpp")
file(TIMESTAMP "${service}" written "%Y-%m-%d %H:%M:%S.%f")
file(WRITE "${SOURCE}/extra-output" "")
file(TOUCH "${SOURCE}/RemoteGen.xml")
configure("configure with an added output")
file(TIMESTAMP "${service}" kept "%Y-%m-%d %H:%M:%S.%f")
poco_osp_expect("a generation adds a new output to the build directory" EXISTS "${extra}")
poco_osp_expect("an output with the same content keeps its time (${written}, ${kept})" kept STREQUAL written)
file(REMOVE "${SOURCE}/extra-output")
file(WRITE "${SOURCE}/changed-output" "")
file(TOUCH "${SOURCE}/RemoteGen.xml")
configure("configure with a changed output and without the added one")
file(TIMESTAMP "${service}" changed "%Y-%m-%d %H:%M:%S.%f")
file(READ "${service}" content)
poco_osp_expect("an output with another content is copied (${content}, ${changed})"
	content STREQUAL "// changed\n" AND NOT changed STREQUAL kept)
poco_osp_expect("an output that the generation no longer writes is removed" NOT EXISTS "${extra}")
file(REMOVE "${SOURCE}/changed-output")
# On a file system that ignores the case, both names are the same file.
file(WRITE "${SOURCE}/upper-case-output" "")
file(TOUCH "${SOURCE}/RemoteGen.xml")
configure("configure with an output whose name differs only in the case")
poco_osp_expect("an output whose name differs only in the case from the last one is in the build directory"
	EXISTS "${GENERATED}/src/SERVICE.cpp")
file(REMOVE "${SOURCE}/upper-case-output")
file(TOUCH "${SOURCE}/RemoteGen.xml")
configure("configure with the output name of the original case")
poco_osp_expect("an output renamed back to the original case is in the build directory" EXISTS "${service}")

# A directory in place of an output, which can be neither replaced nor removed. Extra.cpp is copied before Service.cpp.
file(WRITE "${SOURCE}/extra-output" "")
file(REMOVE "${service}")
file(MAKE_DIRECTORY "${service}/obstacle")
file(TOUCH "${SOURCE}/RemoteGen.xml")
configure_stopped("an output that cannot be copied" "the generated file generated/src/Service\\.cpp cannot be copied")
poco_osp_expect("an output copied before the one that cannot be copied is in the build directory" EXISTS "${extra}")
file(REMOVE_RECURSE "${service}")
file(REMOVE "${SOURCE}/extra-output")
math(EXPR expected_runs "${runs} + 1")
configure("configure after an output could not be copied")
poco_osp_expect("a configure after an output could not be copied generates again and copies it"
	runs EQUAL expected_runs AND EXISTS "${service}")
poco_osp_expect("an output that a stopped configure copied is removed when the generation no longer writes it"
	NOT EXISTS "${extra}")
file(WRITE "${SOURCE}/extra-output" "")
file(TOUCH "${SOURCE}/RemoteGen.xml")
configure("configure with an added output again")
file(REMOVE "${SOURCE}/extra-output" "${extra}")
file(MAKE_DIRECTORY "${extra}/obstacle")
file(TOUCH "${SOURCE}/RemoteGen.xml")
configure_stopped("an output that the generation no longer writes and that cannot be removed"
	"/generated/src/Extra\\.cpp, which the last generation wrote and this one does not, cannot be removed")
file(REMOVE_RECURSE "${extra}")
math(EXPR expected_runs "${runs} + 1")
configure("configure after an output could not be removed")
poco_osp_expect("a configure after an output could not be removed generates again" runs EQUAL expected_runs)

# U+00C4 and U+00E4 in UTF-8
string(ASCII 195 132 upper_a_umlaut)
string(ASCII 195 164 lower_a_umlaut)
set(non_ascii "${GENERATED}/src/${upper_a_umlaut}.cpp")
file(WRITE "${SOURCE}/non-ascii-output" "")
file(TOUCH "${SOURCE}/RemoteGen.xml")
configure("configure with an output with a non-ASCII name")
poco_osp_expect("an output with a non-ASCII name is in the build directory" EXISTS "${non_ascii}")
math(EXPR expected_runs "${runs} + 1")
file(TOUCH "${SOURCE}/RemoteGen.xml")
configure("configure with an output with a non-ASCII name again")
poco_osp_expect("a generation that writes an output with a non-ASCII name again completes" runs EQUAL expected_runs)
file(REMOVE "${SOURCE}/non-ascii-output")
file(TOUCH "${SOURCE}/RemoteGen.xml")
configure("configure without the output with a non-ASCII name")
poco_osp_expect("an output with a non-ASCII name that the generation no longer writes is removed"
	NOT EXISTS "${non_ascii}")

configure_stopped("a RemoteGenNG that cannot be started"
	"the help run of RemoteGenNG ${WORK_DIR}/missing/RemoteGenNG did not end with an exit status"
	"-DPOCO_REMOTEGEN_NG=${WORK_DIR}/missing/RemoteGenNG")
poco_osp_expect("a RemoteGenNG that cannot be started gives no warning about its help"
	NOT output MATCHES "does not name the option")
file(WRITE "${SOURCE}/help-signal" "")
configure_stopped("a RemoteGenNG that a signal ends in its help run"
	"the help run of RemoteGenNG /bin/sh ${SOURCE}/remotegen\\.sh did not end with an exit status")
poco_osp_expect("a RemoteGenNG that a signal ends in its help run gives no warning about its help"
	NOT output MATCHES "does not name the option")
file(REMOVE "${SOURCE}/help-signal")
configure_stopped("an empty POCO_REMOTEGEN_NG" "POCO_REMOTEGEN_NG is empty" "-DPOCO_REMOTEGEN_NG=")

# Configures the project in its own build directory with the module of the given directory and
# without POCO_OSP_BASE, and stores the output in the variable output.
function(configure_default_base description build module_dir)
	execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SOURCE}" -B "${WORK_DIR}/${build}" -G "${GENERATOR}"
			-DCMAKE_CXX_COMPILER=${CXX_COMPILER} -DMODULE_DIR=${module_dir} -DDEFAULT_OSP_BASE=ON
			"-DPOCO_REMOTEGEN_NG=/bin/sh;${SOURCE}/remotegen.sh"
		RESULT_VARIABLE result
		OUTPUT_VARIABLE out
		ERROR_VARIABLE out)
	poco_osp_expect("${description}: the configure succeeds (${result}):\n${out}" result EQUAL 0)
	set(output "${out}" PARENT_SCOPE)
endfunction()

get_filename_component(OSP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
configure_default_base("the module of the source tree" source-module "${OSP_ROOT}/cmake")
poco_osp_expect("POCO_OSP_BASE defaults to the root of the source tree"
	output MATCHES "-DPOCO_OSP_BASE=${OSP_ROOT}[' \n]")

# The layout of an installation on Windows: the module in <prefix>/cmake, the OSP headers in <prefix>/include.
set(PREFIX "${WORK_DIR}/prefix")
file(COPY "${OSP_ROOT}/cmake/PocoOspRemoteGen.cmake" DESTINATION "${PREFIX}/cmake")
file(WRITE "${PREFIX}/include/Poco/OSP/OSP.h" "")
configure_default_base("an installed module" installed-module "${PREFIX}/cmake")
poco_osp_expect("an installed module does not set POCO_OSP_BASE" NOT output MATCHES "POCO_OSP_BASE")

# Configures the given project directory in its own build directory with the given GEN_DIR_NAME of
# the first call and further arguments, and stores the exit status in the variable result and the
# output, with each run of white space replaced by a space, in the variable output.
function(configure_gen_dir source gen_dir build)
	execute_process(COMMAND "${CMAKE_COMMAND}" -S "${source}" -B "${WORK_DIR}/${build}" -G "${GENERATOR}"
			-DCMAKE_CXX_COMPILER=${CXX_COMPILER} -DMODULE_DIR=${CMAKE_CURRENT_LIST_DIR}/.. "-DGEN_DIR=${gen_dir}"
			"-DPOCO_REMOTEGEN_NG=/bin/sh;${SOURCE}/remotegen.sh" ${ARGN}
		RESULT_VARIABLE result
		OUTPUT_VARIABLE out
		ERROR_VARIABLE out)
	string(REGEX REPLACE "[ \t\r\n]+" " " out "${out}")
	set(result "${result}" PARENT_SCOPE)
	set(output "${out}" PARENT_SCOPE)
endfunction()

# Configures as configure_gen_dir() does and expects the configure to stop and to leave the project
# directory as it was.
function(expect_gen_dir_rejected description source gen_dir build)
	configure_gen_dir("${source}" "${gen_dir}" "${build}" ${ARGN})
	string(FIND "${output}" "poco_remote_gen: GEN_DIR_NAME \"${gen_dir}\" names" position)
	poco_osp_expect("${description} stops the configure (${result})" NOT result EQUAL 0 AND position GREATER -1)
	poco_osp_expect("${description} leaves the project directory as it was"
		EXISTS "${source}/CMakeLists.txt" AND EXISTS "${source}/RemoteGen.xml")
endfunction()

# Creates a symbolic link and sets the variable linked to whether it was created. Where symbolic
# links cannot be created, as on Windows without the privilege, the test with the given description
# is skipped with a message.
function(create_link original link description)
	file(CREATE_LINK "${original}" "${link}" RESULT result SYMBOLIC)
	if(result EQUAL 0)
		set(linked TRUE PARENT_SCOPE)
	else()
		message(STATUS "Skipped: ${description}: the symbolic link ${link} cannot be created: ${result}")
		set(linked FALSE PARENT_SCOPE)
	endif()
endfunction()

expect_gen_dir_rejected("a GEN_DIR_NAME that names the source directory" "${SOURCE}" ../source source-gen-dir)
file(GLOB_RECURSE markers LIST_DIRECTORIES false "${WORK_DIR}/.poco_remote_gen_*")
poco_osp_expect("a stopped configure leaves no marker file (${markers})" NOT markers)

# A file that another configure created in the directory is neither the marker file of this one nor removed.
string(MD5 other_id "${SOURCE}")
set(other_marker "${SOURCE}/.poco_remote_gen_${other_id}")
file(WRITE "${other_marker}" "")
expect_gen_dir_rejected("a GEN_DIR_NAME that names the source directory and has a marker file of another configure"
	"${SOURCE}" ../source other-marker-gen-dir)
poco_osp_expect("a marker file of another configure remains" EXISTS "${other_marker}")
file(REMOVE "${other_marker}")

# The marker file cannot be created in a regular file. The message names the file, so that two configures of
# one build directory show the names they use.
file(WRITE "${SOURCE}/Plain.txt" "")
set(description "a GEN_DIR_NAME in which the marker file cannot be created")
set(marker_names "")
foreach(attempt IN ITEMS first second)
	configure_gen_dir("${SOURCE}" ../source/Plain.txt plain-gen-dir)
	poco_osp_expect("${description} stops the ${attempt} configure (${result})"
		NOT result EQUAL 0 AND output MATCHES "cannot be created in ${SOURCE}/Plain\\.txt")
	string(REGEX MATCH "the file ([^ ]+) cannot be created" match "${output}")
	list(APPEND marker_names "${CMAKE_MATCH_1}")
endforeach()
list(REMOVE_DUPLICATES marker_names)
list(LENGTH marker_names count)
poco_osp_expect("two configures of one build directory use different names for the marker file (${marker_names})"
	count EQUAL 2)
poco_osp_expect("${description} is not removed" EXISTS "${SOURCE}/Plain.txt")
file(REMOVE "${SOURCE}/Plain.txt")

set(description "a GEN_DIR_NAME that names the source directory through a symbolic link")
create_link("${WORK_DIR}" "${SOURCE}/up" "${description}")
if(linked)
	expect_gen_dir_rejected("${description}" "${SOURCE}" ../source/up/source linked-gen-dir)
	file(REMOVE "${SOURCE}/up")
endif()

# On a file system that ignores the letter case, the name in the other case names the source directory.
set(description "a GEN_DIR_NAME that names the source directory in another case of ASCII letters")
if(EXISTS "${WORK_DIR}/SOURCE")
	expect_gen_dir_rejected("${description}" "${SOURCE}" ../SOURCE upper-case-gen-dir)
else()
	message(STATUS "Skipped: ${description}: the file system distinguishes the case of ASCII letters")
endif()
file(COPY "${CMAKE_CURRENT_LIST_DIR}/RemoteGen/" DESTINATION "${WORK_DIR}/${upper_a_umlaut}")
set(description "a GEN_DIR_NAME that names the source directory in another case of a non-ASCII letter")
if(EXISTS "${WORK_DIR}/${lower_a_umlaut}")
	expect_gen_dir_rejected("${description}" "${WORK_DIR}/${upper_a_umlaut}" ../${lower_a_umlaut}
		non-ascii-case-gen-dir)
else()
	message(STATUS "Skipped: ${description}: the file system distinguishes the case of non-ASCII letters")
endif()

file(WRITE "${WORK_DIR}/binary-gen-dir/Kept.txt" "")
expect_gen_dir_rejected("a GEN_DIR_NAME that names the build directory" "${SOURCE}" ../binary-gen-dir
	binary-gen-dir)
poco_osp_expect("a GEN_DIR_NAME that names the build directory leaves the build directory as it was"
	EXISTS "${WORK_DIR}/binary-gen-dir/Kept.txt")

# A configuration that is a symbolic link into the directory shared.
file(COPY "${SOURCE}/RemoteGen.xml" DESTINATION "${WORK_DIR}/shared")
file(WRITE "${SOURCE}/cfg/Other.xml" "")
set(description "a GEN_DIR_NAME that names the directory of a configuration that is a symbolic link")
create_link("${WORK_DIR}/shared/RemoteGen.xml" "${SOURCE}/cfg/RemoteGen.xml" "${description}")
if(linked)
	expect_gen_dir_rejected("${description}" "${SOURCE}" ../source/cfg linked-config-gen-dir -DCONFIG=cfg/RemoteGen.xml)
	poco_osp_expect("${description} leaves that directory as it was"
		EXISTS "${SOURCE}/cfg/RemoteGen.xml" AND EXISTS "${SOURCE}/cfg/Other.xml")
endif()
set(description "a GEN_DIR_NAME that names the directory that a configuration resolves to")
create_link("${WORK_DIR}/shared/RemoteGen.xml" "${SOURCE}/Linked.xml" "${description}")
if(linked)
	expect_gen_dir_rejected("${description}" "${SOURCE}" ../shared resolved-config-gen-dir -DCONFIG=Linked.xml)
	poco_osp_expect("${description} leaves that directory as it was" EXISTS "${WORK_DIR}/shared/RemoteGen.xml")
endif()

# A source directory configured as top/src, a symbolic link to it. The configuration is outside top, so
# that only the source directory as named is in GEN_DIR_NAME.
file(WRITE "${WORK_DIR}/top/Other.txt" "")
set(description "a GEN_DIR_NAME that contains a source directory configured through a symbolic link")
create_link("${SOURCE}" "${WORK_DIR}/top/src" "${description}")
if(linked)
	expect_gen_dir_rejected("${description}" "${WORK_DIR}/top/src" ../top linked-source-gen-dir
		-DCONFIG=${WORK_DIR}/shared/RemoteGen.xml)
	poco_osp_expect("${description} leaves the directory that holds the link as it was"
		EXISTS "${WORK_DIR}/top/Other.txt")
endif()

# A copy of the project in resolved-source/src, configured as resolved-top/src, a symbolic link to it. The
# configuration is outside resolved-source, so that only the resolved source directory is in GEN_DIR_NAME.
file(COPY "${CMAKE_CURRENT_LIST_DIR}/RemoteGen/" DESTINATION "${WORK_DIR}/resolved-source/src")
file(WRITE "${WORK_DIR}/resolved-source/Other.txt" "")
file(MAKE_DIRECTORY "${WORK_DIR}/resolved-top")
set(description "a GEN_DIR_NAME that contains the resolved source directory, but not the symbolic link to it")
create_link("${WORK_DIR}/resolved-source/src" "${WORK_DIR}/resolved-top/src" "${description}")
if(linked)
	expect_gen_dir_rejected("${description}" "${WORK_DIR}/resolved-top/src" ../resolved-source
		resolved-source-gen-dir -DCONFIG=${WORK_DIR}/shared/RemoteGen.xml)
	poco_osp_expect("${description} leaves the directory that holds the source directory as it was"
		EXISTS "${WORK_DIR}/resolved-source/Other.txt")
endif()

# A build directory configured as build-top/build, a symbolic link to it.
file(WRITE "${WORK_DIR}/build-top/Other.txt" "")
file(MAKE_DIRECTORY "${WORK_DIR}/linked-build")
set(description "a GEN_DIR_NAME that contains a build directory configured through a symbolic link")
create_link("${WORK_DIR}/linked-build" "${WORK_DIR}/build-top/build" "${description}")
if(linked)
	expect_gen_dir_rejected("${description}" "${SOURCE}" .. build-top/build)
	poco_osp_expect("${description} leaves the directory that holds the link as it was"
		EXISTS "${WORK_DIR}/build-top/Other.txt")
endif()

# A build directory configured as resolved-build-top/build, a symbolic link to resolved-build/build.
file(WRITE "${WORK_DIR}/resolved-build/Other.txt" "")
file(MAKE_DIRECTORY "${WORK_DIR}/resolved-build/build" "${WORK_DIR}/resolved-build-top")
set(description "a GEN_DIR_NAME that contains the resolved build directory, but not the symbolic link to it")
create_link("${WORK_DIR}/resolved-build/build" "${WORK_DIR}/resolved-build-top/build" "${description}")
if(linked)
	expect_gen_dir_rejected("${description}" "${SOURCE}" ../../resolved-build resolved-build-top/build)
	poco_osp_expect("${description} leaves the directory that holds the build directory as it was"
		EXISTS "${WORK_DIR}/resolved-build/Other.txt")
endif()

# Calls from a subdirectory, with the configuration outside the source and build directories, so that
# only the directory that the case names is in GEN_DIR_NAME.
set(shared_config -DCONFIG=${WORK_DIR}/shared/RemoteGen.xml)
expect_gen_dir_rejected("a GEN_DIR_NAME that names the source directory of a subdirectory" "${SOURCE}" ../../source/sub
	subdirectory-source-gen-dir -DSUBDIRECTORY=sub ${shared_config})
poco_osp_expect("a GEN_DIR_NAME that names the source directory of a subdirectory leaves the subdirectory as it was"
	EXISTS "${SOURCE}/sub/CMakeLists.txt")

file(WRITE "${WORK_DIR}/subdirectory-binary/Other.txt" "")
set(description "a GEN_DIR_NAME that names the binary directory of a subdirectory outside the build directory")
expect_gen_dir_rejected("${description}" "${SOURCE}" "${WORK_DIR}/subdirectory-binary" subdirectory-binary-gen-dir
	-DSUBDIRECTORY=sub -DSUBDIRECTORY_BINARY=${WORK_DIR}/subdirectory-binary ${shared_config})
poco_osp_expect("${description} leaves that directory as it was" EXISTS "${WORK_DIR}/subdirectory-binary/Other.txt")

file(COPY "${CMAKE_CURRENT_LIST_DIR}/RemoteGen/sub/" DESTINATION "${WORK_DIR}/outside-source")
set(description "a GEN_DIR_NAME that names the top-level source directory from a subdirectory outside it")
expect_gen_dir_rejected("${description}" "${SOURCE}" "${SOURCE}" outside-source-gen-dir
	-DSUBDIRECTORY=${WORK_DIR}/outside-source -DSUBDIRECTORY_BINARY=outside-source ${shared_config})

file(WRITE "${WORK_DIR}/top-binary-gen-dir/Other.txt" "")
set(description "a GEN_DIR_NAME that names the top-level build directory from a subdirectory built outside it")
expect_gen_dir_rejected("${description}" "${SOURCE}" "${WORK_DIR}/top-binary-gen-dir" top-binary-gen-dir
	-DSUBDIRECTORY=sub -DSUBDIRECTORY_BINARY=${WORK_DIR}/outside-binary ${shared_config})
poco_osp_expect("${description} leaves that directory as it was" EXISTS "${WORK_DIR}/top-binary-gen-dir/Other.txt")

# A source directory whose path contains a semicolon, with the configuration outside it.
# poco_osp_expect() takes its condition as a list, which would split that path, so the
# results are checked first.
set(description "a GEN_DIR_NAME that contains a source directory whose path contains a semicolon")
set(semicolon_dir "${WORK_DIR}/a;b")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/RemoteGen/" DESTINATION "${semicolon_dir}/src")
file(WRITE "${semicolon_dir}/Other.txt" "")
configure_gen_dir("${semicolon_dir}/src" "../a;b" semicolon-gen-dir -DCONFIG=${WORK_DIR}/shared/RemoteGen.xml)
string(FIND "${output}" "poco_remote_gen: GEN_DIR_NAME \"../a;b\" names" position)
poco_osp_expect("${description} stops the configure (${result})" NOT result EQUAL 0 AND position GREATER -1)
if(EXISTS "${semicolon_dir}/Other.txt" AND EXISTS "${semicolon_dir}/src/CMakeLists.txt")
	set(kept TRUE)
else()
	set(kept FALSE)
endif()
poco_osp_expect("${description} leaves the directory that holds the source directory as it was" kept)

# Configures dir three times and builds it twice without a change. Sets output (of the first configure), added_runs
# (the runs of RemoteGenNG in run_dir after it) and build_configures (the configures in the builds).
function(configure_unchanged dir run_dir build generator)
	set(GENERATOR "${generator}")
	configure_gen_dir("${dir}" generated "${build}" ${ARGN})
	poco_osp_expect("the first configure in ${dir} succeeds (${result}):\n${output}" result EQUAL 0)
	set(first_output "${output}")
	file(STRINGS "${run_dir}/runs.txt" lines)
	list(LENGTH lines first_runs)
	foreach(attempt IN ITEMS second third)
		configure_gen_dir("${dir}" generated "${build}")
		poco_osp_expect("the ${attempt} configure in ${dir} succeeds (${result}):\n${output}" result EQUAL 0)
	endforeach()
	file(STRINGS "${run_dir}/lists.txt" lines)
	list(LENGTH lines configures)
	foreach(attempt IN ITEMS first second)
		poco_osp_expect_exit(${EXIT_OK} "the ${attempt} build in ${dir}"
			"${CMAKE_COMMAND}" --build "${WORK_DIR}/${build}")
	endforeach()
	file(STRINGS "${run_dir}/runs.txt" lines)
	list(LENGTH lines runs)
	math(EXPR added "${runs} - ${first_runs}")
	file(STRINGS "${run_dir}/lists.txt" lines)
	list(LENGTH lines lists)
	math(EXPR build_configures "${lists} - ${configures}")
	set(output "${first_output}" PARENT_SCOPE)
	set(added_runs ${added} PARENT_SCOPE)
	set(build_configures ${build_configures} PARENT_SCOPE)
endfunction()

# Changes the file and configures the project directory with the generator in its build directory again. Sets
# added_runs.
function(configure_changed dir build generator changed)
	set(GENERATOR "${generator}")
	file(STRINGS "${dir}/runs.txt" lines)
	list(LENGTH lines first_runs)
	file(TOUCH "${changed}")
	configure_gen_dir("${dir}" generated "${build}")
	poco_osp_expect("the configure in ${dir} after a change succeeds (${result}):\n${output}" result EQUAL 0)
	file(STRINGS "${dir}/runs.txt" lines)
	list(LENGTH lines runs)
	math(EXPR added "${runs} - ${first_runs}")
	set(added_runs ${added} PARENT_SCOPE)
endfunction()

# Expects that a listed binary directory, as named through a symbolic link and as resolved, is not a dependency:
# builds without a change do not configure again, and the warning names both paths.
function(expect_listed_binary_names dir build generator binary description)
	file(MAKE_DIRECTORY "${binary}-resolved" "${WORK_DIR}/outside")
	create_link("${binary}-resolved" "${binary}" "${description}")
	if(NOT linked)
		return()
	endif()
	create_link("${WORK_DIR}/outside" "${binary}-resolved/out" "${description}")
	if(NOT linked)
		return()
	endif()
	file(WRITE "${dir}/sub/list.txt" "file ${dir}/base/include/Service.h\ndirectory ${binary}-resolved/\n"
		"directory ${binary}/out/\n")
	configure_unchanged("${dir}" "${dir}/sub" "${build}" "${generator}" -DSUBDIRECTORY=sub ${ARGN})
	poco_osp_expect("${description}: builds without a change do not configure again (${build_configures})"
		build_configures EQUAL 0)
	string(FIND "${output}" "only at the next configure: ${binary}-resolved/ ${binary}/out/ Call Stack" position)
	poco_osp_expect("${description}: the warning names it resolved and as named:\n${output}" position GREATER -1)
endfunction()

# Each generator checks in its own way whether the build directory is up to date.
set(generators "${GENERATOR}")
find_program(MAKE_PROGRAM NAMES make gmake)
if(MAKE_PROGRAM)
	list(APPEND generators "Unix Makefiles")
	list(REMOVE_DUPLICATES generators)
else()
	message(STATUS "Skipped: the listed paths with Unix Makefiles: no make program")
endif()
foreach(generator IN LISTS generators)
	string(MAKE_C_IDENTIFIER "${generator}" build)
	set(dir "${WORK_DIR}/${build}-source")
	file(COPY "${CMAKE_CURRENT_LIST_DIR}/RemoteGen/" DESTINATION "${dir}")
	set(bracket "${dir}/base/odd/x[1.h")
	set(semicolon "${dir}/base/odd/a;b.h")
	file(WRITE "${bracket}" "")
	file(WRITE "${semicolon}" "")
	# The build directory is configured through a symbolic link, and the list names it as named, resolved and through
	# another symbolic link.
	set(listed_builds "${WORK_DIR}/${build}/")
	file(MAKE_DIRECTORY "${WORK_DIR}/${build}-resolved")
	create_link("${WORK_DIR}/${build}-resolved" "${WORK_DIR}/${build}"
		"the listed resolved build directory with ${generator}")
	if(linked)
		list(APPEND listed_builds "${WORK_DIR}/${build}-resolved/")
		create_link("${WORK_DIR}/${build}-resolved" "${WORK_DIR}/${build}-alias"
			"the build directory listed through another symbolic link with ${generator}")
		if(linked)
			list(APPEND listed_builds "${WORK_DIR}/${build}-alias/")
		endif()
	endif()
	list(TRANSFORM listed_builds PREPEND "directory " OUTPUT_VARIABLE build_lines)
	list(JOIN build_lines "\n" build_lines)
	file(WRITE "${dir}/list.txt" "file ${dir}/base/include/Service.h\nfile ${dir}/base/include/Event Service.h\n"
		"directory ${dir}/base/include/more\nfile ${bracket}\nfile ${semicolon}\ndirectory ${dir}/base/odd\n"
		"${build_lines}\n")
	set(description "listed paths with ${generator}")
	configure_unchanged("${dir}" "${dir}" "${build}" "${generator}")
	poco_osp_expect("${description} add no RemoteGenNG run in configures and builds without a change"
		added_runs EQUAL 0)
	poco_osp_expect("${description}: builds without a change do not configure again (${build_configures})"
		build_configures EQUAL 0)
	string(FIND "${output}" "only at the next configure: ${bracket} ${semicolon} Call Stack" position)
	poco_osp_expect("the warning names the listed paths with a [ and a ; and no other path:\n${output}"
		position GREATER -1)
	list(JOIN listed_builds " " listed_builds)
	string(CONCAT expected "into which a configure writes; a change of them runs RemoteGenNG only at the next "
		"configure: ${listed_builds} Call Stack")
	string(FIND "${output}" "${expected}" position)
	poco_osp_expect("the warning names the listed build directory, by each of its names, and no other path:\n${output}"
		position GREATER -1)
	foreach(changed IN ITEMS bracket semicolon)
		configure_changed("${dir}" "${build}" "${generator}" "${${changed}}")
		poco_osp_expect("a change of the listed path with a ${changed} runs RemoteGenNG again at the next configure"
			added_runs EQUAL 1)
	endforeach()
	# The listed paths with a [ and a ; leave out of the dependencies only themselves.
	file(STRINGS "${dir}/lists.txt" lines)
	list(LENGTH lines lists_before)
	file(WRITE "${dir}/base/odd/Added.h" "")
	poco_osp_expect_exit(${EXIT_OK} "the build in ${dir} after a file was added to a listed directory"
		"${CMAKE_COMMAND}" --build "${WORK_DIR}/${build}")
	file(STRINGS "${dir}/lists.txt" lines)
	list(LENGTH lines lists)
	poco_osp_expect("${description}: a file added to the directory listed after them configures again (${lists})"
		lists GREATER lists_before)

	set(description "the build directory of a call in a subdirectory, configured through a symbolic link")
	expect_listed_binary_names("${dir}" "${build}-sub" "${generator}" "${WORK_DIR}/${build}-sub"
		"${description} with ${generator}" ${shared_config})
	set(binary "${WORK_DIR}/${build}-call-binary")
	set(description "a binary directory of a call outside the build directory, configured through a symbolic link")
	expect_listed_binary_names("${dir}" "${build}-call" "${generator}" "${binary}" "${description} with ${generator}"
		-DSUBDIRECTORY_BINARY=${binary} ${shared_config})
endforeach()

# A configuration in a directory whose name has a [, which is not a dependency of the build.
set(dir "${WORK_DIR}/bracket-config-source")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/RemoteGen/" DESTINATION "${dir}")
file(COPY "${dir}/RemoteGen.xml" DESTINATION "${dir}/cfg[1")
file(WRITE "${dir}/list.txt" "file ${dir}/base/include/Service.h\n")
set(description "a configuration in a directory whose name has a [")
configure_gen_dir("${dir}" generated bracket-config "-DCONFIG=cfg[1/RemoteGen.xml")
string(FIND "${output}" "only at the next configure: ${dir}/cfg[1/RemoteGen.xml Call Stack" position)
poco_osp_expect("${description} is named in the warning (${result}):\n${output}" result EQUAL 0 AND position GREATER -1)
file(STRINGS "${dir}/runs.txt" lines)
list(LENGTH lines runs_before)
file(TOUCH "${dir}/base/include/Service.h")
poco_osp_expect_exit(${EXIT_OK} "the build of ${description} after a change of a listed header"
	"${CMAKE_COMMAND}" --build "${WORK_DIR}/bracket-config")
file(STRINGS "${dir}/runs.txt" lines)
list(LENGTH lines runs)
math(EXPR expected_runs "${runs_before} + 1")
poco_osp_expect("a change of a listed header of ${description} runs RemoteGenNG through the build"
	runs EQUAL expected_runs)
file(TOUCH "${dir}/cfg[1/RemoteGen.xml")
configure_gen_dir("${dir}" generated bracket-config)
file(STRINGS "${dir}/runs.txt" lines)
list(LENGTH lines runs)
math(EXPR expected_runs "${expected_runs} + 1")
poco_osp_expect("a change of ${description} runs RemoteGenNG at the next configure (${result})"
	result EQUAL 0 AND runs EQUAL expected_runs)

# A listed directory beside the build directory, whose name is that of the build directory and \x.
set(dir "${WORK_DIR}/backslash-source")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/RemoteGen/" DESTINATION "${dir}")
set(backslash "${WORK_DIR}/backslash\\x")
# file(MAKE_DIRECTORY) turns the \ into a /.
execute_process(COMMAND mkdir "${backslash}" COMMAND_ERROR_IS_FATAL ANY)
file(WRITE "${dir}/list.txt" "file ${dir}/base/include/Service.h\ndirectory ${backslash}/\n")
set(description "a listed directory beside the build directory whose name has a \\")
configure_gen_dir("${dir}" generated backslash)
string(FIND "${output}" "control character; a change of them runs RemoteGenNG only at the next configure: ${backslash}/"
	position)
poco_osp_expect("${description} is named in the warning about the characters (${result}):\n${output}"
	result EQUAL 0 AND position GREATER -1)
poco_osp_expect("${description} is not named as in the build directory and gives no policy warning:\n${output}"
	NOT output MATCHES "into which a configure writes|CMP0152")

set(dir "${WORK_DIR}/bracket-build-source")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/RemoteGen/" DESTINATION "${dir}")
configure_gen_dir("${dir}" generated "bracket-build[1]")
poco_osp_expect("a build directory whose name has a [ and a ] receives the outputs (${result}):\n${output}"
	result EQUAL 0 AND EXISTS "${WORK_DIR}/bracket-build[1]/generated/src/Service.cpp")

# A RemoteGenNG named relative to the source directory is another stand-in in each directory; the one in sub does not
# name the option list-dependencies.
set(dir "${WORK_DIR}/relative-executables-source")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/RemoteGen/" DESTINATION "${dir}")
file(COPY "${dir}/remotegen.sh" DESTINATION "${dir}/sub")
file(CHMOD "${dir}/remotegen.sh" "${dir}/sub/remotegen.sh" FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
file(WRITE "${dir}/sub/no-list" "")
set(description "a RemoteGenNG named relative to the source directory, in two directories")
configure_gen_dir("${dir}" generated relative-executables -DSUBDIRECTORY=sub -DTOP_CALL=ON ${shared_config}
	-DPOCO_REMOTEGEN_NG=./remotegen.sh)
poco_osp_expect("${description}: the configure succeeds (${result}):\n${output}" result EQUAL 0)
foreach(help_dir IN ITEMS "${dir}" "${dir}/sub")
	set(lines "")
	if(EXISTS "${help_dir}/help-runs.txt")
		file(STRINGS "${help_dir}/help-runs.txt" lines)
	endif()
	list(LENGTH lines help_runs)
	poco_osp_expect("${description}: the help of the stand-in in ${help_dir} is read once (${help_runs})"
		help_runs EQUAL 1)
endforeach()
# Each warning up to the first file of its call stack.
string(REGEX MATCHALL "does not name the option list-dependencies[^:]*: [^ ]+" warnings "${output}")
poco_osp_expect("${description}: the call in sub alone warns about the option list-dependencies:\n${output}"
	warnings MATCHES "^[^;]*[ /]sub/CMakeLists\\.txt:[0-9]+$")

if(NOT DEFINED REMOTEGEN_NG)
	message(STATUS "Skipped: the cases with the RemoteGenNG of the build: the build has no RemoteGenNG")
else()
	set(dir "${WORK_DIR}/real-source")
	file(COPY "${CMAKE_CURRENT_LIST_DIR}/RemoteGen/" DESTINATION "${dir}")
	set(real -DCONFIG=RemoteGenReal.xml "-DPOCO_REMOTEGEN_NG=${REMOTEGEN_NG}" "-DEXTRA_DEFINES=POCO_BASE=${POCO_BASE}")
	configure_gen_dir("${dir}" real real ${real})
	poco_osp_expect("the RemoteGenNG of the build generates into the build directory (${result}):\n${output}"
		result EQUAL 0 AND EXISTS "${WORK_DIR}/real/real/include/IReal.h")
	file(GLOB_RECURSE preprocessed "${dir}/*.i")
	poco_osp_expect("the RemoteGenNG of the build adds nothing to the source directory (${preprocessed})"
		NOT EXISTS "${dir}/real" AND NOT preprocessed)
	execute_process(COMMAND "${REMOTEGEN_NG}" ${OPT}help OUTPUT_VARIABLE help ERROR_VARIABLE help)
	if(help MATCHES "list-dependencies")
		configure_gen_dir("${dir}" real real ${real})
		poco_osp_expect("a configure without a change does not run the RemoteGenNG of the build:\n${output}"
			result EQUAL 0 AND NOT output MATCHES "Generate sources with RemoteGenNG")
	else()
		poco_osp_expect("the RemoteGenNG of the build without the option list-dependencies is reported"
			output MATCHES "does not name the option list-dependencies")
	endif()
	file(TOUCH "${dir}/base/include/Real.h")
	configure_gen_dir("${dir}" real real ${real})
	poco_osp_expect("a change of a header runs the RemoteGenNG of the build again:\n${output}"
		result EQUAL 0 AND output MATCHES "Generate sources with RemoteGenNG in: ${WORK_DIR}/real/real ")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
