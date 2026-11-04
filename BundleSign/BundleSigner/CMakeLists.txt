# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Version Resource
if(MSVC)
	source_group("Resources" FILES ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
	list(APPEND SRCS ${PROJECT_SOURCE_DIR}/DLLVersion.rc)
endif()

add_executable(BundleSigner ${SRCS})
set_target_properties(BundleSigner
    PROPERTIES
    OUTPUT_NAME signbundle
)

target_link_libraries(BundleSigner PRIVATE Poco::OSPBundleSign Poco::OSP Poco::Crypto Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation)

install(
    TARGETS BundleSigner EXPORT BundleSignerTargets
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
install(EXPORT BundleSignerTargets
  FILE
    BundleSignerTargets.cmake
  NAMESPACE
    Poco::OSP::
  DESTINATION
    lib${LIB_SUFFIX}/cmake/Poco
)

# When cross compiling it is assumed that the bundle is already in PATH
if(CMAKE_CROSSCOMPILING)
    find_program(POCO_OSP_BUNDLESIGNER NAMES "signbundle")
    if(NOT POCO_OSP_BUNDLESIGNER)
      message(FATAL_ERROR "Poco::OSP:BundleSigner (signbundle) not found in PATH.")
    endif()
    add_executable(Poco::OSP::BundleSigner IMPORTED GLOBAL)
    set_target_properties(Poco::OSP::BundleSigner PROPERTIES IMPORTED_LOCATION "${POCO_OSP_BUNDLESIGNER}")
else()
    add_executable(Poco::OSP::BundleSigner ALIAS BundleSigner)
endif(CMAKE_CROSSCOMPILING)

if(POCO_OSP_ENABLE_TESTS)
	add_test(
		NAME OSPBundleSigner
		COMMAND ${CMAKE_COMMAND}
			-DSIGNBUNDLE=$<TARGET_FILE:BundleSigner>
			-DDATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../testsuite/data
			-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/SignBundleTest
			-P ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/SignBundleTest.cmake
	)
endif()
