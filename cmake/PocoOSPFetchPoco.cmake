# Pulls POCO into a standalone configure of the repository.
#
# POCO_OSP_POCO_REPOSITORY is a git URL or a local directory. A local git repository is cloned
# like a remote one; a plain source tree is used as is, without POCO_OSP_POCO_VERSION.

include_guard(GLOBAL)

include(FetchContent)
find_package(Git REQUIRED)

set(POCO_OSP_POCO_REPOSITORY "https://github.com/pocoproject/poco.git" CACHE STRING
	"POCO git URL, or a local directory (a git repository or a plain source tree), relative to the repository root")
set(POCO_OSP_POCO_VERSION "release" CACHE STRING
	"POCO version: release (the newest release at or above ${POCO_OSP_POCO_MIN_VERSION}, or main while none exists), \
latest (main), or a tag, branch or commit")

# A URL has a scheme or the scp-like form host:path; a drive letter is not a host.
if(POCO_OSP_POCO_REPOSITORY MATCHES "^[A-Za-z][A-Za-z0-9+.-]*://"
	OR (POCO_OSP_POCO_REPOSITORY MATCHES "^[^/:]+:" AND NOT POCO_OSP_POCO_REPOSITORY MATCHES "^[A-Za-z]:"))
	set(_poco_osp_poco_git "${POCO_OSP_POCO_REPOSITORY}")
else()
	get_filename_component(_poco_osp_poco_dir "${POCO_OSP_POCO_REPOSITORY}" ABSOLUTE
		BASE_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
	if(EXISTS "${_poco_osp_poco_dir}/.git")
		set(_poco_osp_poco_git "${_poco_osp_poco_dir}")
	elseif(EXISTS "${_poco_osp_poco_dir}/CMakeLists.txt" AND IS_DIRECTORY "${_poco_osp_poco_dir}/Foundation")
		set(_poco_osp_poco_tree "${_poco_osp_poco_dir}")
	elseif(EXISTS "${_poco_osp_poco_dir}")
		message(FATAL_ERROR "POCO_OSP_POCO_REPOSITORY: ${_poco_osp_poco_dir} is neither a git repository nor a POCO "
			"source tree.")
	else()
		message(FATAL_ERROR "POCO_OSP_POCO_REPOSITORY: ${_poco_osp_poco_dir} does not exist.")
	endif()
endif()

if(DEFINED _poco_osp_poco_tree)
	set(POCO_OSP_POCO_SOURCE "${_poco_osp_poco_tree}")
	set(POCO_OSP_POCO_REF "")
	message(STATUS "POCO: plain source tree ${_poco_osp_poco_tree}; POCO_OSP_POCO_VERSION does not apply")
	set(FETCHCONTENT_SOURCE_DIR_POCO "${_poco_osp_poco_tree}")
	set(_poco_osp_poco_declare "")
else()
	set(POCO_OSP_POCO_SOURCE "${_poco_osp_poco_git}")
	if(POCO_OSP_POCO_VERSION STREQUAL "release")
		if(POCO_OSP_POCO_RESOLVED_REF
			AND POCO_OSP_POCO_RESOLVED_REPOSITORY STREQUAL _poco_osp_poco_git
			AND POCO_OSP_POCO_RESOLVED_VERSION STREQUAL POCO_OSP_POCO_VERSION)
			if(POCO_OSP_POCO_RESOLVED_REF STREQUAL "main")
				message(STATUS "POCO release: an earlier configure found no release at or above "
					"${POCO_OSP_POCO_MIN_VERSION} and uses POCO main; clear POCO_OSP_POCO_RESOLVED_REF "
					"(cmake -U POCO_OSP_POCO_RESOLVED_REF) to resolve again")
			else()
				message(STATUS "POCO release: ${POCO_OSP_POCO_RESOLVED_REF}, resolved by an earlier configure")
			endif()
		else()
			execute_process(
				COMMAND ${GIT_EXECUTABLE} ls-remote --tags --refs ${_poco_osp_poco_git} "poco-*-release"
				RESULT_VARIABLE _poco_osp_result
				OUTPUT_VARIABLE _poco_osp_output
				ERROR_VARIABLE _poco_osp_error
			)
			if(NOT _poco_osp_result EQUAL 0)
				message(FATAL_ERROR "POCO release: git ls-remote ${_poco_osp_poco_git} failed, the network or the "
					"repository is unavailable:\n${_poco_osp_error}\nSet POCO_OSP_POCO_VERSION to an explicit tag, "
					"or POCO_OSP_POCO_REPOSITORY to a local directory.")
			endif()
			string(REGEX MATCHALL "refs/tags/poco-[0-9]+\\.[0-9]+\\.[0-9]+-release" _poco_osp_tags
				"${_poco_osp_output}")
			set(_poco_osp_best "")
			foreach(_poco_osp_tag IN LISTS _poco_osp_tags)
				string(REGEX REPLACE "^refs/tags/poco-([0-9.]+)-release$" "\\1" _poco_osp_tag_version
					"${_poco_osp_tag}")
				string(REGEX REPLACE "\\..*$" "" _poco_osp_major "${_poco_osp_tag_version}")
				if(_poco_osp_major IN_LIST POCO_OSP_POCO_SUPPORTED_MAJORS
					AND _poco_osp_tag_version VERSION_GREATER_EQUAL POCO_OSP_POCO_MIN_VERSION
					AND (NOT _poco_osp_best OR _poco_osp_tag_version VERSION_GREATER _poco_osp_best))
					set(_poco_osp_best "${_poco_osp_tag_version}")
				endif()
			endforeach()
			if(_poco_osp_best)
				set(POCO_OSP_POCO_RESOLVED_REF "poco-${_poco_osp_best}-release" CACHE INTERNAL "")
				message(STATUS "POCO release: ${POCO_OSP_POCO_RESOLVED_REF}, resolved from ${_poco_osp_poco_git}")
			else()
				set(POCO_OSP_POCO_RESOLVED_REF "main" CACHE INTERNAL "")
				message(STATUS "POCO release: no POCO release at or above ${POCO_OSP_POCO_MIN_VERSION} exists yet; "
					"the build uses POCO main")
			endif()
			unset(_poco_osp_result)
			unset(_poco_osp_output)
			unset(_poco_osp_error)
			unset(_poco_osp_tags)
			unset(_poco_osp_best)
			unset(_poco_osp_tag_version)
			unset(_poco_osp_major)
		endif()
	elseif(POCO_OSP_POCO_VERSION STREQUAL "latest")
		set(POCO_OSP_POCO_RESOLVED_REF "main" CACHE INTERNAL "")
	else()
		set(POCO_OSP_POCO_RESOLVED_REF "${POCO_OSP_POCO_VERSION}" CACHE INTERNAL "")
	endif()
	set(POCO_OSP_POCO_RESOLVED_REPOSITORY "${_poco_osp_poco_git}" CACHE INTERNAL "")
	set(POCO_OSP_POCO_RESOLVED_VERSION "${POCO_OSP_POCO_VERSION}" CACHE INTERNAL "")
	set(POCO_OSP_POCO_REF "${POCO_OSP_POCO_RESOLVED_REF}")

	# A shallow clone cannot fetch an arbitrary commit.
	string(LENGTH "${POCO_OSP_POCO_REF}" _poco_osp_ref_length)
	if(POCO_OSP_POCO_REF MATCHES "^[0-9a-fA-F]+$" AND _poco_osp_ref_length GREATER_EQUAL 7
		AND _poco_osp_ref_length LESS_EQUAL 40)
		set(_poco_osp_shallow FALSE)
	else()
		set(_poco_osp_shallow TRUE)
	endif()
	set(_poco_osp_poco_declare
		GIT_REPOSITORY "${_poco_osp_poco_git}"
		GIT_TAG "${POCO_OSP_POCO_REF}"
		GIT_SHALLOW ${_poco_osp_shallow}
	)
	unset(_poco_osp_ref_length)
	unset(_poco_osp_shallow)
endif()
unset(_poco_osp_poco_git)
unset(_poco_osp_poco_dir)
unset(_poco_osp_poco_tree)

# Normal variables, and only ON: POCO forces options it needs ON in the cache, and a normal variable set to OFF
# would hide that. ENABLE_TESTS stays untouched, so POCO's own test suites are not built; POCO builds CppUnit on
# ENABLE_CPPUNIT alone.
set(POCO_MINIMAL_BUILD ON)
foreach(_poco_osp_poco_option IN LISTS POCO_OSP_POCO_OPTIONS)
	set(${_poco_osp_poco_option} ON)
endforeach()

FetchContent_Declare(poco ${_poco_osp_poco_declare})
unset(_poco_osp_poco_declare)
FetchContent_MakeAvailable(poco)

if(NOT TARGET Poco::Foundation)
	message(FATAL_ERROR "POCO_OSP_POCO_REPOSITORY: ${POCO_OSP_POCO_SOURCE} ${POCO_OSP_POCO_REF} does not build "
		"Poco::Foundation, so it is not a POCO source tree.")
endif()
