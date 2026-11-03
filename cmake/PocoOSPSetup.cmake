# Adds the OSP libraries, bundles and tools to the build; the repository's CMakeLists.txt includes
# this module after POCO is part of the build.

get_filename_component(_poco_osp_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL _poco_osp_root)
	message(FATAL_ERROR "PocoOSPSetup.cmake: a superproject adds the poco-osp repository with add_subdirectory() "
		"after POCO. Including this module from another directory is not supported.")
endif()

# A superproject chooses its POCO components, so a missing one is its configuration error. A
# standalone configure pulls what POCO provides and switches off the parts the pulled version
# cannot build.
set(_poco_osp_errors "")
foreach(_poco_osp_option IN LISTS POCO_OSP_PARTS)
	if(NOT ${_poco_osp_option})
		continue()
	endif()
	set(_poco_osp_missing "")
	set(_poco_osp_missing_options "")
	foreach(_poco_osp_target IN LISTS POCO_OSP_POCO_TARGETS_${_poco_osp_option})
		if(NOT TARGET ${_poco_osp_target})
			list(APPEND _poco_osp_missing ${_poco_osp_target})
			string(REPLACE "Poco::" "" _poco_osp_name ${_poco_osp_target})
			list(APPEND _poco_osp_missing_options ${POCO_OSP_POCO_OPTION_${_poco_osp_name}})
		endif()
	endforeach()
	if(NOT _poco_osp_missing)
		continue()
	endif()
	list(JOIN _poco_osp_missing ", " _poco_osp_missing_text)
	if(POCO_OSP_SUPERBUILD)
		list(JOIN _poco_osp_missing_options ", " _poco_osp_missing_options)
		list(APPEND _poco_osp_errors "${_poco_osp_option} needs ${_poco_osp_missing_text}: switch \
${_poco_osp_missing_options} ON in the superproject, or ${_poco_osp_option} OFF.")
	else()
		set(_poco_osp_reason "POCO does not provide ${_poco_osp_missing_text}")
		if(_poco_osp_missing MATCHES "Poco::RemotingNG")
			string(APPEND _poco_osp_reason " (POCO releases before 2.0 do not contain RemotingNG, "
				"POCO_OSP_POCO_VERSION=latest builds it)")
		endif()
		message(WARNING "${_poco_osp_option} is switched off: ${_poco_osp_reason}.")
		set(${_poco_osp_option} OFF)
		set(POCO_OSP_OFF_REASON_${_poco_osp_option} "${_poco_osp_reason}")
	endif()
endforeach()
unset(_poco_osp_missing)
unset(_poco_osp_missing_options)
unset(_poco_osp_name)
unset(_poco_osp_missing_text)
if(_poco_osp_errors)
	list(JOIN _poco_osp_errors "\n" _poco_osp_errors)
	message(FATAL_ERROR "POCO components missing for OSP:\n${_poco_osp_errors}")
endif()
unset(_poco_osp_errors)

# PocoOSPOptions.cmake has checked the dependencies between OSP parts, so a dependency that is off
# here has been switched off above.
set(_poco_osp_changed ON)
while(_poco_osp_changed)
	set(_poco_osp_changed OFF)
	foreach(_poco_osp_option IN LISTS POCO_OSP_PARTS)
		if(NOT ${_poco_osp_option})
			continue()
		endif()
		foreach(_poco_osp_need IN LISTS POCO_OSP_NEEDS_${_poco_osp_option})
			if(NOT ${_poco_osp_need})
				set(_poco_osp_reason "it needs ${_poco_osp_need}, which is switched off")
				message(WARNING "${_poco_osp_option} is switched off: ${_poco_osp_reason}.")
				set(${_poco_osp_option} OFF)
				set(POCO_OSP_OFF_REASON_${_poco_osp_option} "${_poco_osp_reason}")
				set(_poco_osp_changed ON)
				break()
			endif()
		endforeach()
	endforeach()
endwhile()
unset(_poco_osp_changed)
unset(_poco_osp_reason)

# Poco project context for the module subdirectories. The modules expect a Poco project name
# (POCO_GENERATE_PACKAGE writes Poco<Module>ConfigVersion.cmake), and
# SHARED_LIBRARY_VERSION for their sonames; without the latter the modules'
# VERSION/SOVERSION collapses and misnames the library libPoco<Module>.so.SOVERSION.
# The caller's values are restored at the end of this file.
file(STRINGS "${_poco_osp_root}/libversion" _poco_osp_libversion LIMIT_COUNT 1)
set(_poco_osp_saved_project_name "${PROJECT_NAME}")
set(_poco_osp_saved_shared_library_version "${SHARED_LIBRARY_VERSION}")
set(PROJECT_NAME "Poco")
set(SHARED_LIBRARY_VERSION "${_poco_osp_libversion}")

if(WIN32)
	set(POCO_OSP_CPPIGNORE_FILE "${_poco_osp_root}/cppignore.win")
else()
	set(POCO_OSP_CPPIGNORE_FILE "${_poco_osp_root}/cppignore.lnx")
endif()

# OSP bundles are written to ${CMAKE_BINARY_DIR}/${POCO_INSTALL_BUNDLEDIR}, a
# runtime bundle-repository path; without it the com.appinf.osp.* bundles land
# in lib/ and are unresolvable at server startup.
if(NOT DEFINED POCO_INSTALL_BUNDLEDIR)
	set(POCO_INSTALL_BUNDLEDIR "bundles")
endif()
# The top-level binary directory, not this repository's, so that the bundles of a superproject and the OSP
# bundles share one directory.
if(NOT DEFINED POCO_OSP_BUNDLE_DIR)
	set(POCO_OSP_BUNDLE_DIR "${CMAKE_BINARY_DIR}/${POCO_INSTALL_BUNDLEDIR}")
endif()

include(PocoOSPComponents)

# Restore the caller's project context.
set(PROJECT_NAME "${_poco_osp_saved_project_name}")
set(SHARED_LIBRARY_VERSION "${_poco_osp_saved_shared_library_version}")
unset(_poco_osp_saved_project_name)
unset(_poco_osp_saved_shared_library_version)
unset(_poco_osp_libversion)
unset(_poco_osp_root)
