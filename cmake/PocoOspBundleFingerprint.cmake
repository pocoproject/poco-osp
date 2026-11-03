# Fingerprint an OCI bundle content directory (see PocoOspBundle.cmake).
#
# Runs at BUILD time (cmake -P) before each bundle packaging step:
# hashes the directory's file listing (relative path, size, mtime - the
# same change signal ninja itself uses) and rewrites STAMP_FILE only when
# the fingerprint changed, so the bundle command that depends on the stamp
# repacks exactly when the tree content changed. find is used instead
# of file(GLOB_RECURSE) because the latter returns incomplete,
# cache-dependent results on large trees on some file systems, and because
# a build-time scan also picks up files added after the CMake configure ran.
#
# Expected -D inputs:
#   OCI_DIR    - directory to fingerprint
#   STAMP_FILE - stamp file to (re)write on change

if(NOT OCI_DIR OR NOT STAMP_FILE)
	message(FATAL_ERROR "PocoOspBundleFingerprint: OCI_DIR and STAMP_FILE are required")
endif()

if(NOT EXISTS ${OCI_DIR})
	message(FATAL_ERROR "PocoOspBundleFingerprint: no such directory: ${OCI_DIR}")
endif()

# -type l includes dangling symlinks (common in container rootfs trees);
# %T@ has sub-second precision. sort makes the listing order-stable across
# filesystems and runs.
# BSD find (macOS) has no -printf: BSD stat prints the same fields there
# (%Fm has sub-second precision), and the ./ in front of each path is removed.
execute_process(
	COMMAND find ${OCI_DIR} -maxdepth 0 -printf ""
	RESULT_VARIABLE _printf_result
	OUTPUT_QUIET
	ERROR_QUIET
)
if(_printf_result EQUAL 0)
	execute_process(
		COMMAND find ${OCI_DIR} -mindepth 1 ( -type f -o -type l ) -printf "%P|%s|%T@\n"
		COMMAND sort
		OUTPUT_VARIABLE _listing
		RESULTS_VARIABLE _results
	)
else()
	execute_process(
		COMMAND find . -mindepth 1 ( -type f -o -type l ) -exec stat -f "%N|%z|%Fm" {} +
		COMMAND sort
		WORKING_DIRECTORY ${OCI_DIR}
		OUTPUT_VARIABLE _listing
		RESULTS_VARIABLE _results
	)
	string(REGEX REPLACE "(^|\n)\\./" "\\1" _listing "${_listing}")
endif()
foreach(_result IN LISTS _results)
	if(NOT _result EQUAL 0)
		message(FATAL_ERROR "PocoOspBundleFingerprint: find|sort failed for ${OCI_DIR}: ${_results}")
	endif()
endforeach()

string(SHA1 _fingerprint "${_listing}")

set(_previous "")
if(EXISTS ${STAMP_FILE})
	file(READ ${STAMP_FILE} _previous)
endif()

if(NOT _previous STREQUAL "${_fingerprint}\n")
	file(WRITE ${STAMP_FILE} "${_fingerprint}\n")
endif()
