# Sources
file(GLOB SRCS "src/*.cpp")

# Version Resource
if(MSVC)
	source_group("Resources" FILES ${PROJECT_SOURCE_DIR}/ProDLLVersion.rc)
	list(APPEND SRCS ${PROJECT_SOURCE_DIR}/ProDLLVersion.rc)
endif()

add_executable(BundleCreator ${SRCS})
set_target_properties(BundleCreator
    PROPERTIES
    OUTPUT_NAME bundle
    )

target_link_libraries(BundleCreator Poco::OSP Poco::Zip Poco::Util Poco::XML Poco::JSON Poco::Foundation)

if(WIN32)
	set(PocoConfigPackageLocation "cmake")
else()
	set(PocoConfigPackageLocation "${CMAKE_INSTALL_LIBDIR}/")
endif()

install(
    TARGETS BundleCreator EXPORT BundleCreatorTargets
    LIBRARY DESTINATION lib${LIB_SUFFIX} COMPONENT Devel
    ARCHIVE DESTINATION lib${LIB_SUFFIX} COMPONENT Devel
    RUNTIME DESTINATION bin               COMPONENT Runtime
    INCLUDES DESTINATION include
)

install(
  FILES
    ${PROJECT_SOURCE_DIR}/OSP/cmake/PocoOspBundle.cmake
  DESTINATION
	"${PocoConfigPackageLocation}"
  COMPONENT Devel
)

install(EXPORT BundleCreatorTargets
  FILE
    BundleCreatorTargets.cmake
  NAMESPACE
    Poco::OSP::
  DESTINATION
	"${PocoConfigPackageLocation}"
  COMPONENT Devel
)

# When cross compiling it is assumed that the bundle is already in PATH
if(CMAKE_CROSSCOMPILING)
    find_program(POCO_OSP_BUNDLER NAMES "bundle")
    if(NOT POCO_OSP_BUNDLER)
      message(FATAL_ERROR "Poco::OSP:BundleCreator (bundle) not found in PATH.")
    endif()
    add_executable(Poco::OSP::BundleCreator IMPORTED GLOBAL)
    set_target_properties(Poco::OSP::BundleCreator PROPERTIES IMPORTED_LOCATION "${POCO_OSP_BUNDLER}")
else()
    add_executable(Poco::OSP::BundleCreator ALIAS BundleCreator)
endif(CMAKE_CROSSCOMPILING)
