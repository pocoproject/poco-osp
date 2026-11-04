# Tests of the CMake modules and of the installed packages.

add_test(
	NAME OSPFetchPoco
	COMMAND ${CMAKE_COMMAND}
		-DGENERATOR=${CMAKE_GENERATOR}
		-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/FetchPocoTest
		-P ${CMAKE_CURRENT_SOURCE_DIR}/FetchPocoTest.cmake
)

# The tests run the bundle creator of the build.
if(ENABLE_OSP_BUNDLE_CREATOR)
	add_test(
		NAME OSPInstallConsumer
		COMMAND ${CMAKE_COMMAND}
			-DBUILD_DIR=${CMAKE_BINARY_DIR}
			-DCONFIG=$<CONFIG>
			-DGENERATOR=${CMAKE_GENERATOR}
			-DCXX_COMPILER=${CMAKE_CXX_COMPILER}
			-DOSP_SOURCE_DIR=${PROJECT_SOURCE_DIR}
			-DOSP_VERSION=${PROJECT_VERSION}
			-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/InstallConsumerTest
			-P ${CMAKE_CURRENT_SOURCE_DIR}/InstallConsumerTest.cmake
	)
	add_test(
		NAME OSPBundleFunction
		COMMAND ${CMAKE_COMMAND}
			-DBUNDLE_CREATOR=$<TARGET_FILE:BundleCreator>
			-DGENERATOR=${CMAKE_GENERATOR}
			-DCXX_COMPILER=${CMAKE_CXX_COMPILER}
			-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/BundleFunctionTest
			-P ${CMAKE_CURRENT_SOURCE_DIR}/BundleFunctionTest.cmake
	)
endif()

# The test configures the repository on its own, with the POCO sources of this build and a
# stand-in for a tool of the build host, written as a shell script.
if(NOT POCO_OSP_SUPERBUILD AND NOT WIN32)
	add_test(
		NAME OSPConfigure
		COMMAND ${CMAKE_COMMAND}
			-DOSP_SOURCE_DIR=${PROJECT_SOURCE_DIR}
			-DPOCO_SOURCE_DIR=${poco_SOURCE_DIR}
			-DGENERATOR=${CMAKE_GENERATOR}
			-DC_COMPILER=${CMAKE_C_COMPILER}
			-DCXX_COMPILER=${CMAKE_CXX_COMPILER}
			-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/ConfigureTest
			-P ${CMAKE_CURRENT_SOURCE_DIR}/ConfigureTest.cmake
	)
endif()

# The cases run /bin/sh.
if(NOT WIN32)
	add_test(
		NAME OSPScriptTestFunctions
		COMMAND ${CMAKE_COMMAND}
			-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/ScriptTestFunctionsTest
			-P ${CMAKE_CURRENT_SOURCE_DIR}/ScriptTestFunctionsTest.cmake
	)
endif()

# The stand-in for RemoteGenNG is a shell script. The cases with the RemoteGenNG of the build run when POCO
# builds it, with the POCO source tree that it is built from.
if(NOT WIN32)
	set(_poco_osp_remotegen "")
	if(TARGET RemoteGenNG)
		get_target_property(_poco_osp_remotegen_dir RemoteGenNG SOURCE_DIR)
		cmake_path(GET _poco_osp_remotegen_dir PARENT_PATH _poco_osp_remotegen_dir)
		cmake_path(GET _poco_osp_remotegen_dir PARENT_PATH _poco_osp_remotegen_dir)
		set(_poco_osp_remotegen -DREMOTEGEN_NG=$<TARGET_FILE:RemoteGenNG> -DPOCO_BASE=${_poco_osp_remotegen_dir})
	endif()
	add_test(
		NAME OSPRemoteGen
		COMMAND ${CMAKE_COMMAND}
			-DGENERATOR=${CMAKE_GENERATOR}
			-DCXX_COMPILER=${CMAKE_CXX_COMPILER}
			-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/RemoteGenTest
			${_poco_osp_remotegen}
			-P ${CMAKE_CURRENT_SOURCE_DIR}/RemoteGenTest.cmake
	)
	unset(_poco_osp_remotegen)
	unset(_poco_osp_remotegen_dir)
endif()

add_test(
	NAME OSPDirectoryCopy
	COMMAND ${CMAKE_COMMAND}
		-DGENERATOR=${CMAKE_GENERATOR}
		-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/DirectoryCopyTest
		-P ${CMAKE_CURRENT_SOURCE_DIR}/DirectoryCopyTest.cmake
)

if(ENABLE_OSP_SHELL_SSH)
	add_test(
		NAME OSPShellBundleContent
		COMMAND ${CMAKE_COMMAND}
			-DBUNDLE=${POCO_OSP_BUNDLE_DIR}/com.appinf.osp.shell_*.bndl
			-DLIBRARIES=$<TARGET_FILE_NAME:Poco::SSH>
			-DBUILD_DIR=${CMAKE_BINARY_DIR}
			-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/ShellBundleContentTest
			-P ${CMAKE_CURRENT_SOURCE_DIR}/BundleContentTest.cmake
	)
endif()
