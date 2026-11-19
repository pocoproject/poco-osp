# A project that uses an installed poco-osp: it finds the components named in COMPONENTS, with
# OSP_VERSION also each component's package with that version, compiles a source with the usage
# requirements of each OSP library among them, which must make it C++20, and, with
# OSPBundleCreator among them, creates a bundle with poco_osp_bundle() and has poco_remote_gen().

cmake_minimum_required(VERSION 3.26)
project(InstalledPocoOSPConsumer LANGUAGES CXX)

# Below C++20, so that only the usage requirements of the OSP libraries make Standard.cpp C++20, whatever the
# default of the compiler.
set(CMAKE_CXX_STANDARD 17)

find_package(Poco REQUIRED COMPONENTS ${COMPONENTS})
# The OSP packages have the poco-osp version, which find_package(Poco) does not check.
if(OSP_VERSION)
	foreach(component IN LISTS COMPONENTS)
		find_package(Poco${component} ${OSP_VERSION} CONFIG REQUIRED PATHS "${Poco_DIR}" NO_DEFAULT_PATH)
	endforeach()
endif()

foreach(component IN LISTS COMPONENTS)
	if(TARGET Poco::${component})
		add_library(Standard${component} OBJECT Standard.cpp)
		target_link_libraries(Standard${component} PRIVATE Poco::${component})
	# The bundle creator is a program, whose target is Poco::OSP::BundleCreator.
	elseif(NOT component STREQUAL "OSPBundleCreator")
		message(FATAL_ERROR "find_package(Poco COMPONENTS ${component}) does not provide the target "
			"Poco::${component}.")
	endif()
endforeach()

if(OSPBundleCreator IN_LIST COMPONENTS)
	if(NOT COMMAND poco_remote_gen)
		message(FATAL_ERROR "find_package(Poco COMPONENTS OSPBundleCreator) does not provide poco_remote_gen().")
	endif()
	poco_osp_bundle(ConsumerBundle
		SPEC ${CMAKE_CURRENT_SOURCE_DIR}/Consumer.bndlspec
		DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bundles
	)
endif()
