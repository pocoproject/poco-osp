#
# FetchPocoTest.cmake
#
# Tests of PocoOSPFetchPoco.cmake with a stand-in POCO: a git repository with
# release tags and a plain source tree, each with a CMakeLists.txt that defines
# Poco::Foundation and prints the content of its marker file.
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

find_program(GIT git REQUIRED)

file(REMOVE_RECURSE "${WORK_DIR}")
set(REPOSITORY "${WORK_DIR}/poco")
set(TREE "${WORK_DIR}/tree")

function(git)
	execute_process(COMMAND "${GIT}" -c user.name=test -c user.email=test@example.com -c init.defaultBranch=main ${ARGN}
		WORKING_DIRECTORY "${REPOSITORY}"
		OUTPUT_QUIET
		COMMAND_ERROR_IS_FATAL ANY)
endfunction()

# Commits the marker and tags the commit with each of the given tags.
function(commit marker)
	file(WRITE "${REPOSITORY}/marker.txt" "${marker}\n")
	git(add -A)
	git(commit -q -m "${marker}")
	foreach(tag IN LISTS ARGN)
		git(tag "${tag}")
	endforeach()
endfunction()

# Configures the test project in the build directory, with WORK_DIR as the working directory, and
# stores the output in the variable output. Checks the result against expected, which is OK or FAILURE.
function(configure description build expected)
	execute_process(COMMAND "${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_LIST_DIR}/FetchPoco" -B "${WORK_DIR}/${build}"
			-G "${GENERATOR}" ${ARGN}
		WORKING_DIRECTORY "${WORK_DIR}"
		RESULT_VARIABLE result
		OUTPUT_VARIABLE out
		ERROR_VARIABLE out)
	if(expected STREQUAL "OK")
		poco_osp_expect("${description}: the configure succeeds (${result}):\n${out}" result EQUAL 0)
	else()
		poco_osp_expect("${description}: the configure fails" NOT result EQUAL 0)
	endif()
	set(output "${out}" PARENT_SCOPE)
endfunction()

file(MAKE_DIRECTORY "${REPOSITORY}/Foundation")
file(WRITE "${REPOSITORY}/CMakeLists.txt"
	"cmake_minimum_required(VERSION 3.26)\n"
	"project(StandInPoco NONE)\n"
	"add_library(Foundation INTERFACE)\n"
	"add_library(Poco::Foundation ALIAS Foundation)\n"
	"file(STRINGS \"\${CMAKE_CURRENT_SOURCE_DIR}/marker.txt\" marker)\n"
	"message(STATUS \"marker: \${marker}\")\n")
file(WRITE "${REPOSITORY}/Foundation/README" "Foundation\n")
git(init -q)
commit("1.15.4" poco-1.15.4-release)
file(WRITE "${REPOSITORY}/RemotingNG/CMakeLists.txt" "\n")
commit("2.0.0" poco-2.0.0-release)
commit("2.0.1" poco-2.0.1-release)
commit("2.0.1p1" poco-2.0.1p1-release)
commit("3.0.0" poco-3.0.0-release)
commit("main-1")
file(COPY "${REPOSITORY}/" DESTINATION "${TREE}" PATTERN ".git" EXCLUDE)
file(WRITE "${TREE}/marker.txt" "tree\n")

configure("release" release OK -DPOCO_OSP_POCO_REPOSITORY=${REPOSITORY})
poco_osp_expect("release resolves to the newest release of a supported major, a patch release included"
	output MATCHES "marker: 2\\.0\\.1p1\n.*summary ref: poco-2\\.0\\.1p1-release\n")

execute_process(COMMAND "${GIT}" rev-parse main WORKING_DIRECTORY "${REPOSITORY}"
	OUTPUT_VARIABLE main_commit OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
configure("release without a release at the minimum" release-fallback OK -DPOCO_OSP_POCO_REPOSITORY=${REPOSITORY}
	-DPOCO_OSP_POCO_MIN_VERSION=2.1.0)
poco_osp_expect("release without a release at the minimum builds the commit of main and names the fallback"
	output MATCHES "marker: main-1\n.*summary ref: ${main_commit}\n" AND output MATCHES "summary fallback: ON\n")
configure("release without a release at the minimum, reconfigured" release-fallback OK)
poco_osp_expect("a reconfigure names the fallback of the first configure"
	output MATCHES "summary ref: ${main_commit}\n" AND output MATCHES "summary fallback: ON\n")
configure("release resolved again after a fallback" release-fallback OK -U POCO_OSP_POCO_RESOLVED_REF
	-DPOCO_OSP_POCO_MIN_VERSION=2.0.0)
poco_osp_expect("a release resolved after a fallback no longer names the fallback"
	output MATCHES "summary ref: poco-2\\.0\\.1p1-release\n" AND output MATCHES "summary fallback: OFF\n")

# A URL, as a local directory that does not exist stops the configure before FetchContent.
if(REPOSITORY MATCHES "^/")
	set(URL "file://${REPOSITORY}")
else()
	set(URL "file:///${REPOSITORY}")
endif()
configure("latest" latest OK -DPOCO_OSP_POCO_REPOSITORY=${URL} -DPOCO_OSP_POCO_VERSION=latest)
poco_osp_expect("latest builds main" output MATCHES "marker: main-1\n")
poco_osp_expect("latest is fetched as the commit of main" output MATCHES "summary ref: ${main_commit}\n")
commit("main-2")
configure("latest after a commit on main" latest OK)
poco_osp_expect("a reconfigure keeps the POCO of the first configure" output MATCHES "marker: main-1\n")
file(RENAME "${REPOSITORY}" "${WORK_DIR}/moved")
configure("latest without the repository" latest OK)
poco_osp_expect("a reconfigure does not need the repository" output MATCHES "marker: main-1\n")
file(RENAME "${WORK_DIR}/moved" "${REPOSITORY}")

configure("a branch" branch OK -DPOCO_OSP_POCO_REPOSITORY=${URL} -DPOCO_OSP_POCO_VERSION=main)
poco_osp_expect("a branch builds its head" output MATCHES "marker: main-2\n")
commit("main-3")
configure("a branch after a commit on it" branch OK)
poco_osp_expect("a reconfigure keeps the commit of the branch fetched first" output MATCHES "marker: main-2\n")
file(RENAME "${REPOSITORY}" "${WORK_DIR}/moved")
configure("a branch without the repository" branch OK)
poco_osp_expect("a reconfigure of a branch does not need the repository" output MATCHES "marker: main-2\n")
file(RENAME "${WORK_DIR}/moved" "${REPOSITORY}")

configure("FETCHCONTENT_SOURCE_DIR_POCO" override OK -DPOCO_OSP_POCO_REPOSITORY=https://invalid.invalid/poco.git
	-DFETCHCONTENT_SOURCE_DIR_POCO=${TREE})
poco_osp_expect("FETCHCONTENT_SOURCE_DIR_POCO is configured and named in the summary, without a ref"
	output MATCHES "marker: tree\n.*summary source: ${TREE}\n-- summary ref: \n-- configured source: ${TREE}\n")

configure("a relative FETCHCONTENT_SOURCE_DIR_POCO" relative-override OK -DFETCHCONTENT_SOURCE_DIR_POCO=tree)
# poco_osp_expect() splits its arguments at semicolons: the "." stands for the one after the directory.
poco_osp_expect("a relative FETCHCONTENT_SOURCE_DIR_POCO is named before POCO's output and in the summary"
	output MATCHES "POCO: FETCHCONTENT_SOURCE_DIR_POCO ${TREE}. POCO_OSP_POCO_REPOSITORY.*marker: tree\n"
	AND output MATCHES "summary source: ${TREE}\n-- summary ref: \n-- configured source: ${TREE}\n")
configure("a relative FETCHCONTENT_SOURCE_DIR_POCO of the type PATH" relative-path-override FAILURE
	-DFETCHCONTENT_SOURCE_DIR_POCO:PATH=tree)
poco_osp_expect("a relative FETCHCONTENT_SOURCE_DIR_POCO that stays relative stops the configure"
	output MATCHES "FETCHCONTENT_SOURCE_DIR_POCO: tree is a relative path")

configure("plain tree" switch OK -DPOCO_OSP_POCO_REPOSITORY=${TREE})
poco_osp_expect("a plain tree is configured" output MATCHES "marker: tree\n")
configure("git repository after a plain tree" switch OK -DPOCO_OSP_POCO_REPOSITORY=${REPOSITORY}
	-DPOCO_OSP_POCO_VERSION=poco-2.0.0-release)
poco_osp_expect("the build directory of a plain tree switches to the git repository"
	output MATCHES "marker: 2\\.0\\.0\n.*summary source: ${REPOSITORY}\n")

configure("a POCO older than 2.0" old FAILURE -DPOCO_OSP_POCO_REPOSITORY=${REPOSITORY}
	-DPOCO_OSP_POCO_VERSION=poco-1.15.4-release)
poco_osp_expect("a POCO older than 2.0 stops the configure with the ref"
	output MATCHES "poco-1\\.15\\.4-release has no RemotingNG, so it is older than POCO 2\\.0\\.0")

set(OLD_TREE "${WORK_DIR}/old-tree")
file(COPY "${TREE}/" DESTINATION "${OLD_TREE}" PATTERN "RemotingNG" EXCLUDE)
# CMake wraps the lines of an error message.
configure("a plain tree older than 2.0" old-tree FAILURE -DPOCO_OSP_POCO_REPOSITORY=${OLD_TREE})
string(REGEX REPLACE "[ \n]+" " " output "${output}")
poco_osp_expect("a plain tree older than 2.0 stops the configure with the tree and the setting that names it"
	output MATCHES "POCO ${OLD_TREE} has no RemotingNG.* Set POCO_OSP_POCO_REPOSITORY to a source tree of POCO")
configure("FETCHCONTENT_SOURCE_DIR_POCO older than 2.0" old-override FAILURE
	-DFETCHCONTENT_SOURCE_DIR_POCO=${OLD_TREE})
string(REGEX REPLACE "[ \n]+" " " output "${output}")
poco_osp_expect("FETCHCONTENT_SOURCE_DIR_POCO older than 2.0 stops the configure with the tree and the setting"
	output MATCHES "POCO ${OLD_TREE} has no RemotingNG.* Set FETCHCONTENT_SOURCE_DIR_POCO to a source tree of POCO")

file(REMOVE_RECURSE "${WORK_DIR}")
