# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "src/*.h")
POCO_HEADERS_AUTO(SRCS ${HDRS_G})

# Version Resource
if(MSVC)
	source_group("Resources" FILES ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
	list(APPEND SRCS ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
endif()

poco_add_executable(BundleCreator ${SRCS})
set_target_properties(BundleCreator
	PROPERTIES
	OUTPUT_NAME bundle
)

target_link_libraries(BundleCreator PRIVATE Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation)

install(
	TARGETS BundleCreator EXPORT OSPBundleCreatorTargets
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
)

# find_package(Poco COMPONENTS OSPBundleCreator) provides the tool and the helper modules,
# installed where POCO_GENERATE_PACKAGE installs the package files of the libraries.
if(WIN32)
	set(_poco_osp_package_dir "cmake")
else()
	set(_poco_osp_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
endif()
install(EXPORT OSPBundleCreatorTargets
	FILE ${PROJECT_NAME}OSPBundleCreatorTargets.cmake
	NAMESPACE Poco::OSP::
	DESTINATION ${_poco_osp_package_dir}
	COMPONENT Devel
)
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}OSPBundleCreatorConfigVersion.cmake
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY AnyNewerVersion
)
install(
	FILES
		${CMAKE_CURRENT_SOURCE_DIR}/cmake/PocoOSPBundleCreatorConfig.cmake
		${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}OSPBundleCreatorConfigVersion.cmake
		${PROJECT_SOURCE_DIR}/cmake/PocoOspBundle.cmake
		${PROJECT_SOURCE_DIR}/cmake/PocoOspBundleFingerprint.cmake
		${PROJECT_SOURCE_DIR}/cmake/PocoOspRemoteGen.cmake
	DESTINATION ${_poco_osp_package_dir}
	COMPONENT Devel
)
unset(_poco_osp_package_dir)

# When cross compiling it is assumed that the bundle is already in PATH
if(CMAKE_CROSSCOMPILING)
	find_program(POCO_OSP_BUNDLER NAMES "bundle")
	if(NOT POCO_OSP_BUNDLER)
		message(FATAL_ERROR "Poco::OSP::BundleCreator (bundle) not found in PATH.")
	endif()
	add_executable(Poco::OSP::BundleCreator IMPORTED GLOBAL)
	set_target_properties(Poco::OSP::BundleCreator PROPERTIES IMPORTED_LOCATION "${POCO_OSP_BUNDLER}")
else()
	add_executable(Poco::OSP::BundleCreator ALIAS BundleCreator)
endif()

if(POCO_OSP_ENABLE_TESTS)
	add_test(
		NAME OSPBundleCreator
		COMMAND ${CMAKE_COMMAND}
			-DBUNDLE=$<TARGET_FILE:BundleCreator>
			-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/BundleCreatorTest
			-P ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/BundleCreatorTest.cmake
	)
endif()
