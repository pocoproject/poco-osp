#
# InstallConsumerTest.cmake
#
# Installs the build tree into a temporary prefix and uses the installed
# packages: on ELF platforms the installed bundle creator loads the POCO and
# OSP libraries of the installation, each OSP component is found on its own and
# with the poco-osp version, a source that uses an OSP library is compiled as
# C++20, a bundle is created with the installed bundle creator, and a
# superproject with the installed POCO is refused with a message.
#
# Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#
# Variables: BUILD_DIR (the build tree), CONFIG (the build configuration),
# GENERATOR (the CMake generator), CXX_COMPILER (the C++ compiler of the build),
# OSP_SOURCE_DIR (the poco-osp source tree), OSP_VERSION (the poco-osp
# version), WORK_DIR (a directory for this test only, removed afterwards).
#

include("${CMAKE_CURRENT_LIST_DIR}/../PocoOSPScriptTest.cmake")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
set(PREFIX "${WORK_DIR}/install")

poco_osp_expect_exit(${EXIT_OK} "install the build tree" "${CMAKE_COMMAND}" --install "${BUILD_DIR}"
	--config "${CONFIG}" --prefix "${PREFIX}")

file(GLOB_RECURSE poco_config "${PREFIX}/*/PocoConfig.cmake")
poco_osp_expect("the POCO package is installed (${poco_config})" poco_config MATCHES "PocoConfig.cmake$")
get_filename_component(package_dir "${poco_config}" DIRECTORY)
file(GLOB_RECURSE cmake_files RELATIVE "${PREFIX}" "${PREFIX}/*.cmake")
list(FILTER cmake_files INCLUDE REGEX "(^|/)Poco[^/]*\\.cmake$")
file(RELATIVE_PATH package_path "${PREFIX}" "${package_dir}")
list(FILTER cmake_files EXCLUDE REGEX "^${package_path}/")
poco_osp_expect("every package file is in the POCO package directory ${package_path} (${cmake_files})" NOT cmake_files)

# POCO's postfix, so that the tools of a Debug and a Release installation do not replace each other.
if(CONFIG STREQUAL "Debug")
	file(GLOB tools RELATIVE "${PREFIX}" "${PREFIX}/bin/bundle*")
	poco_osp_expect("the Debug bundle creator has the postfix d (${tools})" tools MATCHES "^bin/bundled(\\.exe)?$")
endif()

# The loader follows the RUNPATH of a library for its own dependencies only, so every installed POCO and OSP
# library needs one; a library found outside the installation can be another POCO with the same SONAME.
if(NOT WIN32 AND NOT APPLE)
	file(GLOB bundle_creator "${PREFIX}/bin/bundle*")
	poco_osp_expect("the bundle creator is installed in bin (${bundle_creator})" bundle_creator)
	file(GET_RUNTIME_DEPENDENCIES EXECUTABLES ${bundle_creator}
		RESOLVED_DEPENDENCIES_VAR resolved
		UNRESOLVED_DEPENDENCIES_VAR unresolved
		PRE_INCLUDE_REGEXES "Poco"
		PRE_EXCLUDE_REGEXES ".*")
	poco_osp_expect("the installed bundle creator finds every POCO and OSP library (unresolved: ${unresolved})"
		NOT unresolved)
	foreach(library IN LISTS resolved)
		cmake_path(IS_PREFIX PREFIX "${library}" NORMALIZE inside)
		poco_osp_expect("the installed bundle creator loads ${library} from the installation" inside)
	endforeach()
endif()

if(EXISTS "${PREFIX}/include/Poco/OSP/Unit")
	poco_osp_expect("the installed OSPUnit headers find the CppUnit headers" EXISTS "${PREFIX}/include/CppUnit")
endif()

# One build directory, so that the compiler is checked once; the imported targets of one configure
# are not seen by the next one.
file(GLOB osp_configs RELATIVE "${package_dir}" "${package_dir}/PocoOSP*Config.cmake")
list(TRANSFORM osp_configs REPLACE "^Poco(.*)Config\\.cmake$" "\\1")
list(FIND osp_configs OSPBundleCreator index)
poco_osp_expect("the bundle creator package is installed (${osp_configs})" NOT index EQUAL -1)
foreach(component IN LISTS osp_configs)
	poco_osp_expect_exit(${EXIT_OK} "find_package(Poco COMPONENTS ${component}), version ${OSP_VERSION}"
		"${CMAKE_COMMAND}"
		-S "${CMAKE_CURRENT_LIST_DIR}/Consumer" -B "${WORK_DIR}/consumer" -G "${GENERATOR}"
		-DCMAKE_CXX_COMPILER=${CXX_COMPILER} -DCMAKE_BUILD_TYPE=${CONFIG} -DCMAKE_PREFIX_PATH=${PREFIX}
		-DCOMPONENTS=${component} -DOSP_VERSION=${OSP_VERSION})
	# The bundle creator is built below.
	if(NOT component STREQUAL "OSPBundleCreator")
		poco_osp_expect_exit(${EXIT_OK} "compile a source that uses ${component} as C++20" "${CMAKE_COMMAND}"
			--build "${WORK_DIR}/consumer" --config "${CONFIG}")
	endif()
endforeach()
poco_osp_expect_exit(${EXIT_OK} "configure a bundle with the installed bundle creator" "${CMAKE_COMMAND}"
	-S "${CMAKE_CURRENT_LIST_DIR}/Consumer" -B "${WORK_DIR}/consumer" -DCOMPONENTS=OSPBundleCreator)
poco_osp_expect_exit(${EXIT_OK} "build a bundle with the installed bundle creator" "${CMAKE_COMMAND}"
	--build "${WORK_DIR}/consumer" --config "${CONFIG}")
poco_osp_expect("the installed bundle creator creates the bundle"
	EXISTS "${WORK_DIR}/consumer/bundles/com.appinf.osp.consumer_1.0.0.bndl")

execute_process(COMMAND "${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_LIST_DIR}/InstalledPoco" -B "${WORK_DIR}/superproject"
		-G "${GENERATOR}" -DCMAKE_CXX_COMPILER=${CXX_COMPILER} -DCMAKE_PREFIX_PATH=${PREFIX}
		-DOSP_SOURCE_DIR=${OSP_SOURCE_DIR}
	RESULT_VARIABLE result
	OUTPUT_VARIABLE output
	ERROR_VARIABLE output)
poco_osp_expect("a superproject with an installed POCO is refused with a message:\n${output}"
	NOT result EQUAL 0 AND output MATCHES "Poco::Foundation is imported from an installed POCO")

file(REMOVE_RECURSE "${WORK_DIR}")
