if (NOT DEFINED POCO_INSTALL_BUNDLEDIR)
	set(POCO_INSTALL_BUNDLEDIR "lib${LIB_SUFFIX}")
endif()

# Append our module directory to CMake
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if(ENABLE_OSP_BUNDLE_CREATOR)
	add_subdirectory(BundleCreator)
endif()

if (ENABLE_OSP_BUNDLES)
	include(PocoOspBundle)

	if (ENABLE_OSP_AUTH)
		add_subdirectory(Auth)
		add_subdirectory(Auth/Data)
		add_subdirectory(Auth/TwoFactor)
		if (ENABLE_REMOTING)
			add_subdirectory(Auth/Remoting)
		endif()
	endif()
	add_subdirectory(Core)

	if (ENABLE_CRYPTO)
		add_subdirectory(Crypto)
	endif()
	if (ENABLE_DATA)
		add_subdirectory(Data)
	endif()
	if (ENABLE_DATA_SQLITE)
		add_subdirectory(Data/SQLite)
	endif()
	if (ENABLE_DATA_ODBC)
		add_subdirectory(Data/ODBC)
	endif()
	if (ENABLE_REDIS)
		add_subdirectory(Redis)
	endif()
	if (ENABLE_JWT)
		add_subdirectory(JWT)
	endif()
	if (ENABLE_NET)
		add_subdirectory(Mail)
	endif()
	if (ENABLE_NET)
		add_subdirectory(Net)
	endif()
	if (ENABLE_NETSSL)
		add_subdirectory(NetSSL_OpenSSL)
	endif()
	if (ENABLE_PROMETHEUS)
		add_subdirectory(Prometheus)
	endif()
	if (ENABLE_REMOTING)
		add_subdirectory(RemotingNG/TCP)
		add_subdirectory(RemotingNG/REST)
	endif()
	if (ENABLE_NETSSL)
		add_subdirectory(SecureMail)
		add_subdirectory(SecureWebServer)
	endif()
	if (ENABLE_NET AND ENABLE_ZIP)
		add_subdirectory(Shell)
		add_subdirectory(Shell/Commands)
		add_subdirectory(Shell/LogLevelCommand)
	endif()
	add_subdirectory(SimpleAuth)
	if (ENABLE_NET)
		add_subdirectory(Web)
		add_subdirectory(WebEvent)
		add_subdirectory(WebServer)
	endif()
	if (ENABLE_JS)
		add_subdirectory(JS)
	endif()
endif()

# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "include/*.h")
POCO_HEADERS_AUTO(SRCS ${HDRS_G})

# Version Resource
if(MSVC AND BUILD_SHARED_LIBS)
	source_group("Resources" FILES ${PROJECT_SOURCE_DIR}/ProDLLVersion.rc)
	list(APPEND SRCS ${PROJECT_SOURCE_DIR}/ProDLLVersion.rc)
endif()

add_library(OSP ${SRCS})
add_library(Poco::OSP ALIAS OSP)
set_target_properties(OSP
	PROPERTIES
	VERSION ${SHARED_LIBRARY_VERSION} SOVERSION ${SHARED_LIBRARY_VERSION}
	OUTPUT_NAME PocoOSP
	DEFINE_SYMBOL OSP_EXPORTS
)

target_link_libraries(OSP PUBLIC Poco::Foundation Poco::XML Poco::Util Poco::Zip)
target_include_directories(OSP
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

POCO_INSTALL(OSP)
POCO_GENERATE_PACKAGE(OSP)

if(ENABLE_TESTS)
	add_subdirectory(testsuite)
endif()
	
if (ENABLE_SAMPLES)
	add_subdirectory(samples)
endif()
