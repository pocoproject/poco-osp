# Pulls POCO into a standalone configure of the repository.
#
# POCO_OSP_POCO_REPOSITORY is a git URL or a local directory. A local git repository is cloned
# like a remote one; a plain source tree is used as is, without POCO_OSP_POCO_VERSION. A
# FETCHCONTENT_SOURCE_DIR_POCO set by the user is used like a plain source tree.
#
# A reconfigure does not update a fetched POCO: release and latest are resolved to a tag or a
# commit once, and the populated source is updated only when the repository or the ref changes.

include_guard(GLOBAL)

include(FetchContent)

set(POCO_OSP_POCO_REPOSITORY "https://github.com/pocoproject/poco.git" CACHE STRING
	"POCO git URL, or a local directory (a git repository or a plain source tree), relative to the repository root")
set(POCO_OSP_POCO_VERSION "release" CACHE STRING
	"POCO version: release (the newest release at or above ${POCO_OSP_POCO_MIN_VERSION}, or main while none exists), \
latest (main), or a tag, branch or commit. release and latest are resolved at the first configure, \
cmake -U POCO_OSP_POCO_RESOLVED_REF resolves them again; a branch is fetched at the first configure only")

if(FETCHCONTENT_SOURCE_DIR_POCO)
	# Declared as a PATH, as FetchContent does: an untyped relative command-line value becomes absolute against the
	# working directory; FetchContent would resolve any other relative value against the directory that adds POCO.
	set(FETCHCONTENT_SOURCE_DIR_POCO "${FETCHCONTENT_SOURCE_DIR_POCO}"
		CACHE PATH "When not empty, overrides where to find pre-populated content for poco")
	if(NOT IS_ABSOLUTE "${FETCHCONTENT_SOURCE_DIR_POCO}")
		message(FATAL_ERROR "FETCHCONTENT_SOURCE_DIR_POCO: ${FETCHCONTENT_SOURCE_DIR_POCO} is a relative path. Set "
			"it to the absolute path of the POCO source tree.")
	endif()
	set(_poco_osp_poco_tree "${FETCHCONTENT_SOURCE_DIR_POCO}")
	message(STATUS "POCO: FETCHCONTENT_SOURCE_DIR_POCO ${_poco_osp_poco_tree}; POCO_OSP_POCO_REPOSITORY and "
		"POCO_OSP_POCO_VERSION do not apply")
# A URL has a scheme or the scp-like form host:path; a drive letter is not a host.
elseif(POCO_OSP_POCO_REPOSITORY MATCHES "^[A-Za-z][A-Za-z0-9+.-]*://"
	OR (POCO_OSP_POCO_REPOSITORY MATCHES "^[^/:]+:" AND NOT POCO_OSP_POCO_REPOSITORY MATCHES "^[A-Za-z]:"))
	set(_poco_osp_poco_git "${POCO_OSP_POCO_REPOSITORY}")
else()
	get_filename_component(_poco_osp_poco_dir "${POCO_OSP_POCO_REPOSITORY}" ABSOLUTE
		BASE_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
	if(EXISTS "${_poco_osp_poco_dir}/.git")
		set(_poco_osp_poco_git "${_poco_osp_poco_dir}")
	elseif(EXISTS "${_poco_osp_poco_dir}/CMakeLists.txt" AND IS_DIRECTORY "${_poco_osp_poco_dir}/Foundation")
		set(_poco_osp_poco_tree "${_poco_osp_poco_dir}")
		message(STATUS "POCO: plain source tree ${_poco_osp_poco_tree}; POCO_OSP_POCO_VERSION does not apply")
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
	# Without a download method, FetchContent uses SOURCE_DIR as it is.
	set(_poco_osp_poco_declare SOURCE_DIR "${_poco_osp_poco_tree}")
else()
	find_package(Git REQUIRED)
	set(POCO_OSP_POCO_SOURCE "${_poco_osp_poco_git}")
	if(POCO_OSP_POCO_VERSION STREQUAL "release" OR POCO_OSP_POCO_VERSION STREQUAL "latest")
		if(POCO_OSP_POCO_RESOLVED_REF
			AND POCO_OSP_POCO_RESOLVED_REPOSITORY STREQUAL _poco_osp_poco_git
			AND POCO_OSP_POCO_RESOLVED_VERSION STREQUAL POCO_OSP_POCO_VERSION)
			message(STATUS "POCO ${POCO_OSP_POCO_VERSION}: ${POCO_OSP_POCO_RESOLVED_REF}, resolved by an earlier "
				"configure; clear POCO_OSP_POCO_RESOLVED_REF (cmake -U POCO_OSP_POCO_RESOLVED_REF) to resolve again")
		else()
			execute_process(
				COMMAND ${GIT_EXECUTABLE} ls-remote --refs ${_poco_osp_poco_git} "poco-*-release" "refs/heads/main"
				RESULT_VARIABLE _poco_osp_result
				OUTPUT_VARIABLE _poco_osp_output
				ERROR_VARIABLE _poco_osp_error
			)
			if(NOT _poco_osp_result EQUAL 0)
				message(FATAL_ERROR "POCO ${POCO_OSP_POCO_VERSION}: git ls-remote ${_poco_osp_poco_git} failed, the "
					"network or the repository is unavailable:\n${_poco_osp_error}\nSet POCO_OSP_POCO_VERSION to an "
					"explicit tag, or POCO_OSP_POCO_REPOSITORY to a local directory.")
			endif()
			if(NOT _poco_osp_output MATCHES "([0-9a-f]+)\trefs/heads/main(\n|$)")
				message(FATAL_ERROR "POCO ${POCO_OSP_POCO_VERSION}: ${_poco_osp_poco_git} has no branch main.")
			endif()
			set(_poco_osp_main "${CMAKE_MATCH_1}")
			set(_poco_osp_best "")
			if(POCO_OSP_POCO_VERSION STREQUAL "release")
				# The patch release poco-X.Y.ZpN-release ranks as version X.Y.Z.N, above poco-X.Y.Z-release.
				string(REGEX MATCHALL "refs/tags/poco-[0-9]+\\.[0-9]+\\.[0-9]+(p[0-9]+)?-release" _poco_osp_tags
					"${_poco_osp_output}")
				set(_poco_osp_best_version "")
				foreach(_poco_osp_tag IN LISTS _poco_osp_tags)
					string(REGEX REPLACE "^refs/tags/poco-([0-9.]+)p?([0-9]*)-release$" "\\1.0\\2"
						_poco_osp_tag_version "${_poco_osp_tag}")
					string(REGEX REPLACE "\\..*$" "" _poco_osp_major "${_poco_osp_tag_version}")
					if(_poco_osp_major IN_LIST POCO_OSP_POCO_SUPPORTED_MAJORS
						AND _poco_osp_tag_version VERSION_GREATER_EQUAL POCO_OSP_POCO_MIN_VERSION
						AND (NOT _poco_osp_best OR _poco_osp_tag_version VERSION_GREATER _poco_osp_best_version))
						string(REPLACE "refs/tags/" "" _poco_osp_best "${_poco_osp_tag}")
						set(_poco_osp_best_version "${_poco_osp_tag_version}")
					endif()
				endforeach()
			endif()
			# The fallback is cached with the ref, so that the summary of a reconfigure names it too.
			if(_poco_osp_best)
				set(POCO_OSP_POCO_RESOLVED_REF "${_poco_osp_best}" CACHE INTERNAL "")
				set(POCO_OSP_POCO_RESOLVED_FALLBACK OFF CACHE INTERNAL "")
				message(STATUS "POCO release: ${POCO_OSP_POCO_RESOLVED_REF}, resolved from ${_poco_osp_poco_git}")
			else()
				# The commit, not the branch, so that a fetch cannot move the build to another POCO.
				set(POCO_OSP_POCO_RESOLVED_REF "${_poco_osp_main}" CACHE INTERNAL "")
				if(POCO_OSP_POCO_VERSION STREQUAL "release")
					set(POCO_OSP_POCO_RESOLVED_FALLBACK ON CACHE INTERNAL "")
					message(STATUS "POCO release: no POCO release at or above ${POCO_OSP_POCO_MIN_VERSION} exists "
						"yet; the build uses POCO main at ${_poco_osp_main}")
				else()
					message(STATUS "POCO latest: POCO main at ${_poco_osp_main}")
				endif()
			endif()
			unset(_poco_osp_result)
			unset(_poco_osp_output)
			unset(_poco_osp_error)
			unset(_poco_osp_main)
			unset(_poco_osp_tags)
			unset(_poco_osp_best)
			unset(_poco_osp_best_version)
			unset(_poco_osp_tag_version)
			unset(_poco_osp_major)
		endif()
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
	# Contacting the repository again only when the repository or the ref changed keeps a reconfigure
	# working offline and on the same POCO.
	if("${_poco_osp_poco_git} ${POCO_OSP_POCO_REF}" STREQUAL "${POCO_OSP_POCO_FETCHED}")
		set(FETCHCONTENT_UPDATES_DISCONNECTED_POCO ON)
	else()
		set(FETCHCONTENT_UPDATES_DISCONNECTED_POCO OFF)
	endif()
	unset(_poco_osp_ref_length)
	unset(_poco_osp_shallow)
endif()

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

# The setting that names the POCO, and the POCO as the messages below name it.
if(FETCHCONTENT_SOURCE_DIR_POCO)
	set(_poco_osp_poco_setting FETCHCONTENT_SOURCE_DIR_POCO)
else()
	set(_poco_osp_poco_setting POCO_OSP_POCO_REPOSITORY)
endif()
set(_poco_osp_poco_name "${POCO_OSP_POCO_SOURCE}")
if(DEFINED _poco_osp_poco_git)
	set(POCO_OSP_POCO_FETCHED "${_poco_osp_poco_git} ${POCO_OSP_POCO_REF}" CACHE INTERNAL "")
	string(APPEND _poco_osp_poco_name " ${POCO_OSP_POCO_REF}")
endif()

if(NOT TARGET Poco::Foundation)
	message(FATAL_ERROR "${_poco_osp_poco_setting}: ${_poco_osp_poco_name} does not build Poco::Foundation, so it "
		"is not a POCO source tree.")
endif()
# The VERSION file of POCO main can still name a 1.x release, so the check looks for RemotingNG,
# which POCO has from 2.0 on.
if(NOT EXISTS "${poco_SOURCE_DIR}/RemotingNG/CMakeLists.txt")
	if(DEFINED _poco_osp_poco_git)
		message(FATAL_ERROR "POCO ${_poco_osp_poco_name} has no RemotingNG, so it is older than POCO "
			"${POCO_OSP_POCO_MIN_VERSION}, which OSP needs. Set POCO_OSP_POCO_VERSION to release, latest or a "
			"POCO ${POCO_OSP_POCO_MIN_VERSION} ref.")
	else()
		message(FATAL_ERROR "POCO ${_poco_osp_poco_name} has no RemotingNG, so it is older than POCO "
			"${POCO_OSP_POCO_MIN_VERSION}, which OSP needs. Set ${_poco_osp_poco_setting} to a source tree of POCO "
			"${POCO_OSP_POCO_MIN_VERSION} or newer.")
	endif()
endif()
unset(_poco_osp_poco_git)
unset(_poco_osp_poco_dir)
unset(_poco_osp_poco_tree)
unset(_poco_osp_poco_setting)
unset(_poco_osp_poco_name)
