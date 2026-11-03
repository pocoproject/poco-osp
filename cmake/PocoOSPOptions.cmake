# Options of the OSP build and the POCO components each OSP part needs.
#
# The repository and the POCO build it pulls share one CMake cache, so no option
# here reuses the name of a POCO option.

include_guard(GLOBAL)

option(POCO_OSP_MINIMAL_BUILD "Build only the OSP core library, the bundle creator and the parts enabled explicitly"
	OFF)

if(POCO_OSP_MINIMAL_BUILD)
	set(_poco_osp_default OFF)
else()
	set(_poco_osp_default ON)
endif()

option(ENABLE_OSP "Enable the OSP core library" ON)
option(ENABLE_OSP_BUNDLE_CREATOR "Enable the OSP bundle creator" ON)
option(ENABLE_OSP_BUNDLES "Enable creating OSP bundles" ${_poco_osp_default})
option(ENABLE_OSP_BUNDLESIGN "Enable the OSP bundle signing library and the bundle signer" ${_poco_osp_default})
option(ENABLE_OSP_STRIPBUNDLE "Enable the OSP bundle stripping tool stripbundle" ${_poco_osp_default})
option(ENABLE_OSP_CODECACHE "Enable the OSP code cache utility ccutil" ${_poco_osp_default})
option(ENABLE_OSP_AUTH "Enable the OSP Auth bundles" ${_poco_osp_default})
option(ENABLE_OSP_AUTH_LDAP "Enable LDAP in OSP Auth (requires a system LDAP library)" OFF)
option(ENABLE_OSP_SHELL "Enable the OSP Shell bundles" ${_poco_osp_default})
option(ENABLE_OSP_SHELL_SSH "Enable SSH in the OSP Shell (requires libssh)" OFF)
option(ENABLE_OSP_SHELL_CLIENT "Enable the OSP Shell client ospsh (line editing with libedit, if found)" OFF)
option(ENABLE_OSP_WEB "Enable the OSP Web, WebServer and WebEvent bundles" ${_poco_osp_default})
option(ENABLE_OSP_BUNDLEADMIN "Enable the OSP BundleAdmin bundle" ${_poco_osp_default})
option(ENABLE_OSP_NET "Enable the Net bundle" ${_poco_osp_default})
option(ENABLE_OSP_NETSSL "Enable the NetSSL_OpenSSL, SecureWebServer and SecureMail bundles" ${_poco_osp_default})
option(ENABLE_OSP_CRYPTO "Enable the Crypto bundle" ${_poco_osp_default})
option(ENABLE_OSP_DATA "Enable the Data bundle" ${_poco_osp_default})
option(ENABLE_OSP_DATA_SQLITE "Enable the Data SQLite bundle" ${_poco_osp_default})
option(ENABLE_OSP_DATA_ODBC "Enable the Data ODBC bundle (requires unixODBC)" OFF)
option(ENABLE_OSP_DATA_MYSQL "Enable the Data MySQL bundle (requires the MySQL or MariaDB client library)" OFF)
option(ENABLE_OSP_DATA_POSTGRESQL "Enable the Data PostgreSQL bundle (requires the PostgreSQL client library)" OFF)
option(ENABLE_OSP_ACTIVERECORD "Enable the ActiveRecord bundle" OFF)
option(ENABLE_OSP_REDIS "Enable the Redis bundle" ${_poco_osp_default})
option(ENABLE_OSP_JWT "Enable the JWT bundle" ${_poco_osp_default})
option(ENABLE_OSP_MAIL "Enable the OSP Mail bundle" ${_poco_osp_default})
option(ENABLE_OSP_PROMETHEUS "Enable the Prometheus bundle" ${_poco_osp_default})
option(ENABLE_OSP_SIMPLEAUTH "Enable the OSP SimpleAuth bundle" ${_poco_osp_default})
option(ENABLE_OSP_CORE "Enable the OSP Core bundle" ${_poco_osp_default})
option(ENABLE_OSP_UNIT "Enable the OSP Unit test runner bundle" OFF)
option(ENABLE_OSP_REMOTING "Enable the RemotingNG TCP and REST bundles and OSP Auth Remoting" ${_poco_osp_default})
unset(_poco_osp_default)

# A superproject that added POCO decides about tests and samples for the whole build.
if(TARGET Poco::Foundation AND ENABLE_TESTS)
	set(_poco_osp_tests_default ON)
else()
	set(_poco_osp_tests_default OFF)
endif()
if(TARGET Poco::Foundation AND ENABLE_SAMPLES)
	set(_poco_osp_samples_default ON)
else()
	set(_poco_osp_samples_default OFF)
endif()
option(POCO_OSP_ENABLE_TESTS "Build the OSP test suites" ${_poco_osp_tests_default})
option(POCO_OSP_ENABLE_SAMPLES "Build the OSP samples" ${_poco_osp_samples_default})
unset(_poco_osp_tests_default)
unset(_poco_osp_samples_default)

set(POCO_OSP_PARTS "")

function(_poco_osp_part _option)
	cmake_parse_arguments(PARSE_ARGV 1 _poco_osp_part "" "" "NEEDS;POCO")
	# A misspelt keyword would otherwise drop the values that follow it.
	if(DEFINED _poco_osp_part_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "_poco_osp_part(${_option}): unknown arguments: ${_poco_osp_part_UNPARSED_ARGUMENTS}; "
			"the keywords are NEEDS and POCO.")
	endif()
	list(APPEND POCO_OSP_PARTS ${_option})
	set(POCO_OSP_PARTS ${POCO_OSP_PARTS} PARENT_SCOPE)
	set(POCO_OSP_NEEDS_${_option} ${_poco_osp_part_NEEDS} PARENT_SCOPE)
	set(POCO_OSP_POCO_TARGETS_${_option} ${_poco_osp_part_POCO} PARENT_SCOPE)
endfunction()

# NEEDS lists the OSP parts a part links or whose bundles its .bndlspec files require at runtime;
# OSP Auth also needs the Data SQLite bundle, the database connector built by default for its
# user store. POCO lists the targets the part's CMakeLists.txt files link.
_poco_osp_part(ENABLE_OSP
	POCO Poco::Foundation Poco::XML Poco::Util Poco::Zip)
_poco_osp_part(ENABLE_OSP_BUNDLE_CREATOR NEEDS ENABLE_OSP
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip)
_poco_osp_part(ENABLE_OSP_BUNDLES NEEDS ENABLE_OSP_BUNDLE_CREATOR)
_poco_osp_part(ENABLE_OSP_BUNDLESIGN NEEDS ENABLE_OSP
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Crypto)
_poco_osp_part(ENABLE_OSP_STRIPBUNDLE
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip)
_poco_osp_part(ENABLE_OSP_CODECACHE NEEDS ENABLE_OSP
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip)
_poco_osp_part(ENABLE_OSP_CORE NEEDS ENABLE_OSP_BUNDLES
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip)
_poco_osp_part(ENABLE_OSP_NET NEEDS ENABLE_OSP_BUNDLES
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Net)
_poco_osp_part(ENABLE_OSP_CRYPTO NEEDS ENABLE_OSP_BUNDLES
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Crypto)
_poco_osp_part(ENABLE_OSP_DATA NEEDS ENABLE_OSP_BUNDLES
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Data)
_poco_osp_part(ENABLE_OSP_DATA_SQLITE NEEDS ENABLE_OSP_BUNDLES ENABLE_OSP_DATA
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Data Poco::DataSQLite)
_poco_osp_part(ENABLE_OSP_DATA_ODBC NEEDS ENABLE_OSP_BUNDLES ENABLE_OSP_DATA
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Data Poco::DataODBC)
_poco_osp_part(ENABLE_OSP_DATA_MYSQL NEEDS ENABLE_OSP_BUNDLES ENABLE_OSP_DATA
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Data Poco::DataMySQL)
_poco_osp_part(ENABLE_OSP_DATA_POSTGRESQL NEEDS ENABLE_OSP_BUNDLES ENABLE_OSP_DATA
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Data Poco::DataPostgreSQL)
_poco_osp_part(ENABLE_OSP_ACTIVERECORD NEEDS ENABLE_OSP_BUNDLES ENABLE_OSP_DATA
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::ActiveRecord)
_poco_osp_part(ENABLE_OSP_REDIS NEEDS ENABLE_OSP_BUNDLES
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Redis)
_poco_osp_part(ENABLE_OSP_MAIL NEEDS ENABLE_OSP_BUNDLES ENABLE_OSP_NET
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Net)
_poco_osp_part(ENABLE_OSP_WEB NEEDS ENABLE_OSP_BUNDLES ENABLE_OSP_NET
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Net)
_poco_osp_part(ENABLE_OSP_BUNDLEADMIN NEEDS ENABLE_OSP_BUNDLES ENABLE_OSP_WEB
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Net)
_poco_osp_part(ENABLE_OSP_NETSSL NEEDS ENABLE_OSP_BUNDLES ENABLE_OSP_NET ENABLE_OSP_CRYPTO ENABLE_OSP_MAIL
		ENABLE_OSP_WEB
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Net Poco::Crypto Poco::NetSSL)
_poco_osp_part(ENABLE_OSP_JWT NEEDS ENABLE_OSP_BUNDLES ENABLE_OSP_CRYPTO ENABLE_OSP_WEB
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Net Poco::Crypto Poco::JWT)
_poco_osp_part(ENABLE_OSP_PROMETHEUS NEEDS ENABLE_OSP_BUNDLES ENABLE_OSP_NET ENABLE_OSP_WEB
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Net Poco::Prometheus)
_poco_osp_part(ENABLE_OSP_SHELL NEEDS ENABLE_OSP_BUNDLES ENABLE_OSP_NET ENABLE_OSP_CORE
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Net)
_poco_osp_part(ENABLE_OSP_SHELL_SSH NEEDS ENABLE_OSP_SHELL
	POCO Poco::SSH)
_poco_osp_part(ENABLE_OSP_SHELL_CLIENT NEEDS ENABLE_OSP_SHELL
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Net)
_poco_osp_part(ENABLE_OSP_SIMPLEAUTH NEEDS ENABLE_OSP_BUNDLES
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util)
_poco_osp_part(ENABLE_OSP_AUTH NEEDS ENABLE_OSP_BUNDLES ENABLE_OSP_CRYPTO ENABLE_OSP_DATA ENABLE_OSP_DATA_SQLITE
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Crypto Poco::Data Poco::DataSQLite)
_poco_osp_part(ENABLE_OSP_AUTH_LDAP NEEDS ENABLE_OSP_AUTH)
_poco_osp_part(ENABLE_OSP_REMOTING NEEDS ENABLE_OSP_BUNDLES ENABLE_OSP_NET ENABLE_OSP_NETSSL
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Net Poco::NetSSL
		Poco::RemotingNG Poco::RemotingNGTCP Poco::RemotingNGREST)
_poco_osp_part(ENABLE_OSP_UNIT NEEDS ENABLE_OSP_BUNDLES
	POCO Poco::Foundation Poco::XML Poco::Util Poco::CppUnit)
_poco_osp_part(POCO_OSP_ENABLE_TESTS NEEDS ENABLE_OSP_BUNDLES ENABLE_OSP_WEB
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip Poco::Net Poco::CppUnit)
_poco_osp_part(POCO_OSP_ENABLE_SAMPLES NEEDS ENABLE_OSP_BUNDLES ENABLE_OSP_SHELL ENABLE_OSP_WEB
	POCO Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Zip)

# The POCO option that builds each target, by target name without the Poco:: prefix.
# Poco::Foundation is always built.
set(POCO_OSP_POCO_OPTION_XML ENABLE_XML)
set(POCO_OSP_POCO_OPTION_JSON ENABLE_JSON)
set(POCO_OSP_POCO_OPTION_Util ENABLE_UTIL)
set(POCO_OSP_POCO_OPTION_Net ENABLE_NET)
set(POCO_OSP_POCO_OPTION_Zip ENABLE_ZIP)
set(POCO_OSP_POCO_OPTION_Crypto ENABLE_CRYPTO)
set(POCO_OSP_POCO_OPTION_NetSSL ENABLE_NETSSL)
set(POCO_OSP_POCO_OPTION_Data ENABLE_DATA)
set(POCO_OSP_POCO_OPTION_DataSQLite ENABLE_DATA_SQLITE)
set(POCO_OSP_POCO_OPTION_DataODBC ENABLE_DATA_ODBC)
set(POCO_OSP_POCO_OPTION_DataMySQL ENABLE_DATA_MYSQL)
set(POCO_OSP_POCO_OPTION_DataPostgreSQL ENABLE_DATA_POSTGRESQL)
set(POCO_OSP_POCO_OPTION_ActiveRecord ENABLE_ACTIVERECORD)
set(POCO_OSP_POCO_OPTION_JWT ENABLE_JWT)
set(POCO_OSP_POCO_OPTION_Prometheus ENABLE_PROMETHEUS)
set(POCO_OSP_POCO_OPTION_Redis ENABLE_REDIS)
set(POCO_OSP_POCO_OPTION_SSH ENABLE_SSH)
set(POCO_OSP_POCO_OPTION_RemotingNG ENABLE_REMOTINGNG)
set(POCO_OSP_POCO_OPTION_RemotingNGTCP ENABLE_REMOTINGNG_TCP)
set(POCO_OSP_POCO_OPTION_RemotingNGREST ENABLE_REMOTINGNG_REST)
set(POCO_OSP_POCO_OPTION_CppUnit ENABLE_CPPUNIT)

# Nothing is enabled behind the user's back: a missing OSP dependency stops the configure.
set(_poco_osp_errors "")
set(POCO_OSP_POCO_OPTIONS "")
foreach(_poco_osp_option IN LISTS POCO_OSP_PARTS)
	if(NOT ${_poco_osp_option})
		continue()
	endif()
	set(_poco_osp_missing "")
	foreach(_poco_osp_need IN LISTS POCO_OSP_NEEDS_${_poco_osp_option})
		if(NOT ${_poco_osp_need})
			list(APPEND _poco_osp_missing ${_poco_osp_need})
		endif()
	endforeach()
	if(_poco_osp_missing)
		list(JOIN _poco_osp_missing ", " _poco_osp_missing)
		list(APPEND _poco_osp_errors
			"${_poco_osp_option} is ON and needs these options, which are OFF: ${_poco_osp_missing}")
	endif()
	foreach(_poco_osp_target IN LISTS POCO_OSP_POCO_TARGETS_${_poco_osp_option})
		string(REPLACE "Poco::" "" _poco_osp_name ${_poco_osp_target})
		if(DEFINED POCO_OSP_POCO_OPTION_${_poco_osp_name})
			list(APPEND POCO_OSP_POCO_OPTIONS ${POCO_OSP_POCO_OPTION_${_poco_osp_name}})
		endif()
	endforeach()
endforeach()
list(REMOVE_DUPLICATES POCO_OSP_POCO_OPTIONS)
unset(_poco_osp_missing)
unset(_poco_osp_name)

if(_poco_osp_errors)
	list(JOIN _poco_osp_errors "\n" _poco_osp_errors)
	message(FATAL_ERROR "OSP options conflict:\n${_poco_osp_errors}\nSwitch the options named ON, or switch off the "
		"options that need them.")
endif()
unset(_poco_osp_errors)
