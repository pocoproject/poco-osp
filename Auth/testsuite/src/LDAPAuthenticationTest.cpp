//
// LDAPAuthenticationTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "LDAPAuthenticationTest.h"


#if defined(POCO_OSP_AUTH_ENABLE_LDAP)


#include "CppUnit/TestCaller.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/Auth/AbstractLDAPAuthAdminService.h"
#include "Poco/OSP/Auth/LDAPClient.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/DateTime.h"
#include "Poco/Exception.h"
#include "Poco/Logger.h"
#include "Poco/Timespan.h"
#include "Poco/Timestamp.h"
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>


using Poco::OSP::Auth::AbstractLDAPAuthAdminService;
using Poco::OSP::Auth::LDAPClient;
using namespace std::string_literals;


namespace
{
	struct Directory
		/// Scripted answers of the LDAP server and a record of the requests.
	{
		bool initializeSucceeds = true;
		LDAPClient::Result bindResult = LDAPClient::Result::SUCCESS_RESULT;
		bool findUserSucceeds = true;
		std::string foundUserDN = "cn=found,dc=example,dc=com"s;
		std::string accountName;
			/// The account name of the entry that the search finds or reads.
		std::string userPrincipalName;
			/// The user principal name of that entry.
		std::string principalName;
			/// The msDS-PrincipalName of that entry.
		bool fetchSucceeds = true;
		bool fetchReadsAccountName = true;
			/// The read of the entry after the bind returns accountName.
		std::string fullName;
		LDAPClient::Roles roles;
		LDAPClient::Permissions permissions;
		LDAPClient::Attributes attributes;
		std::map<std::string, LDAPClient::Permissions> groupPermissions;
			/// A group that is not listed cannot be read.

		int initializeCount = 0;
		std::vector<std::string> boundDNs;
		std::vector<Poco::Timespan> timeouts;
	};


	class ScriptedLDAPClient: public LDAPClient
	{
	public:
		ScriptedLDAPClient(Poco::Logger& logger, Directory& directory):
			LDAPClient(logger),
			_directory(directory)
		{
		}

		ScriptedLDAPClient() = delete;
		ScriptedLDAPClient(const ScriptedLDAPClient&) = delete;
		ScriptedLDAPClient(ScriptedLDAPClient&&) = delete;
		ScriptedLDAPClient& operator = (const ScriptedLDAPClient&) = delete;
		ScriptedLDAPClient& operator = (ScriptedLDAPClient&&) = delete;
		~ScriptedLDAPClient() override = default;

		bool initialize(const std::string&) override
		{
			++_directory.initializeCount;
			return _directory.initializeSucceeds;
		}

		bool setTimeLimit(int) override
		{
			return true;
		}

		bool setTimeout(const Poco::Timespan& timeout) override
		{
			_directory.timeouts.push_back(timeout);
			return true;
		}

		Result bindDirect(const std::string& bindName, const std::string&) override
		{
			_directory.boundDNs.push_back(bindName);
			return _directory.bindResult;
		}

		void unbind() override
		{
		}

		bool findUserDN(const std::string&, std::string& userDN) override
		{
			if (!_directory.findUserSucceeds) return false;
			userDN = _directory.foundUserDN;
			setAccountName(_directory.accountName);
			setUserPrincipalName(_directory.userPrincipalName);
			setPrincipalName(_directory.principalName);
			return true;
		}

		bool fetchOwnProperties() override
		{
			return fetchOwnProperties(""s);
		}

		bool fetchOwnProperties(const std::string&) override
		{
			if (!_directory.fetchSucceeds) return false;
			setOwnProperties(_directory.fullName, _directory.roles, _directory.permissions, _directory.attributes);
			setAccountName(_directory.fetchReadsAccountName ? _directory.accountName : ""s);
			setUserPrincipalName(_directory.userPrincipalName);
			setPrincipalName(_directory.principalName);
			return true;
		}

		bool fetchGroupPermissions(const std::string& groupDN, Permissions& permissions) override
		{
			const auto it = _directory.groupPermissions.find(groupDN);
			if (it == _directory.groupPermissions.end()) return false;
			permissions = it->second;
			return true;
		}

	private:
		Directory& _directory;
	};


	class TestLDAPAuthAdminService: public AbstractLDAPAuthAdminService
		/// Keeps users and roles in memory and talks to a ScriptedLDAPClient,
		/// or to a real LDAP server if no Directory is given.
		/// The AuthAdminService interface is not used by the tests.
	{
	public:
		using Permissions = LDAPClient::Permissions;
		using Roles = LDAPClient::Roles;
		using LDAPResult = AbstractLDAPAuthAdminService::LDAPResult;
		using LDAPUser = AbstractLDAPAuthAdminService::LDAPUser;

		TestLDAPAuthAdminService(const LDAPParams& params, Directory* pDirectory):
			AbstractLDAPAuthAdminService(Poco::Logger::get("LDAPAuthenticationTest"s), params),
			_pDirectory(pDirectory)
		{
		}

		TestLDAPAuthAdminService() = delete;
		TestLDAPAuthAdminService(const TestLDAPAuthAdminService&) = delete;
		TestLDAPAuthAdminService(TestLDAPAuthAdminService&&) = delete;
		TestLDAPAuthAdminService& operator = (const TestLDAPAuthAdminService&) = delete;
		TestLDAPAuthAdminService& operator = (TestLDAPAuthAdminService&&) = delete;
		~TestLDAPAuthAdminService() override = default;

		bool authenticate(const std::string& username, const std::string& credentials, bool& fallbackLocal) const
		{
			return authenticateLDAP(username, credentials, fallbackLocal);
		}

		[[nodiscard]] LDAPResult verify(const std::string& username, const std::string& credentials,
			LDAPUser& user) const
		{
			return verifyLDAP(username, credentials, user);
		}

		void lockOutAccount(const std::string& accountName)
			/// acceptLDAPAccount() refuses the given account name.
		{
			_lockedOutAccounts.insert(accountName);
		}

		[[nodiscard]] const std::vector<std::string>& checkedAccounts() const
			/// The account names that acceptLDAPAccount() was called with.
		{
			return _checkedAccounts;
		}

		bool hasUser(const std::string& username) const
		{
			return _credentials.find(username) != _credentials.end();
		}

		std::string userAttribute(const std::string& username, const std::string& attribute) const
		{
			return getUserAttributeImpl(username, attribute);
		}

		void setUserAttribute(const std::string& username, const std::string& attribute,
			const std::string& value) override
		{
			replaceUserAttributeImpl(username, attribute, value);
		}

		Permissions userPermissions(const std::string& username) const
		{
			return _userPermissions[username];
		}

		Roles userRoles(const std::string& username) const
		{
			return _userRoles[username];
		}

		void addRole(const std::string& rolename, const Permissions& permissions)
		{
			_roles.insert(rolename);
			_rolePermissions[rolename] = permissions;
		}

		void addLocalRole(const std::string& rolename, const Permissions& permissions)
		{
			addRole(rolename, permissions);
			_roleAttributes[rolename][ROLE_ATTR_LDAP] = "false"s;
		}

		Permissions rolePermissions(const std::string& rolename) const
		{
			return _rolePermissions[rolename];
		}

		bool roleExists(const std::string& rolename) const override
		{
			return _roles.find(rolename) != _roles.end();
		}

	protected:
		std::unique_ptr<LDAPClient> createLDAPClient() const override
		{
			if (_pDirectory == nullptr) return AbstractLDAPAuthAdminService::createLDAPClient();
			return std::make_unique<ScriptedLDAPClient>(_logger, *_pDirectory);
		}

		bool acceptLDAPAccount(const std::string&, const std::string& accountName) const override
		{
			_checkedAccounts.push_back(accountName);
			return _lockedOutAccounts.find(accountName) == _lockedOutAccounts.end();
		}

		void createOrUpdateRoleImpl(const std::string& rolename) const override
		{
			_roles.insert(rolename);
		}

		void createOrUpdateUserImpl(const std::string& username, const std::string& credentials) const override
		{
			_credentials[username] = credentials;
		}

		void replaceUserAttributeImpl(const std::string& username, const std::string& attribute,
			const std::string& value) const override
		{
			_userAttributes[username][attribute] = value;
		}

		void replaceRoleAttributeImpl(const std::string& rolename, const std::string& attribute,
			const std::string& value) const override
		{
			_roleAttributes[rolename][attribute] = value;
		}

		void replacePermissionsForUserImpl(const std::string& username,
			const std::set<std::string>& permissions) const override
		{
			_userPermissions[username] = permissions;
		}

		void replaceRolesForUserImpl(const std::string& username, const std::set<std::string>& roles) const override
		{
			_userRoles[username] = roles;
		}

		void replacePermissionsForRoleImpl(const std::string& rolename,
			const std::set<std::string>& permissions) const override
		{
			_rolePermissions[rolename] = permissions;
		}

		std::string getUserAttributeImpl(const std::string& username, const std::string& attribute,
			const std::string& deflt = std::string()) const override
		{
			return valueOf(_userAttributes, username, attribute, deflt);
		}

		std::string getRoleAttributeImpl(const std::string& rolename, const std::string& attribute,
			const std::string& deflt = std::string()) const override
		{
			return valueOf(_roleAttributes, rolename, attribute, deflt);
		}

		// AuthService
		bool authenticate(const std::string&, const std::string&) const override
			{ throw Poco::NotImplementedException(); }
		bool authorize(const std::string&, const std::string&) const override { throw Poco::NotImplementedException(); }
		bool authorize(const std::string&, const std::string&, const std::string&) const override
			{ throw Poco::NotImplementedException(); }
		bool userExists(const std::string&) const override { throw Poco::NotImplementedException(); }

		// AuthAdminService
		bool verifyCredentials(const std::string&, const std::string&) override
			{ throw Poco::NotImplementedException(); }
		void changePassword(const std::string&, const std::string&) override { throw Poco::NotImplementedException(); }
		void permissionsForUser(const std::string&, std::set<std::string>&) const override
			{ throw Poco::NotImplementedException(); }
		void effectivePermissionsForUser(const std::string&, std::set<std::string>&) const override
			{ throw Poco::NotImplementedException(); }
		void grantPermissionsToUser(const std::string&, const std::set<std::string>&) override
			{ throw Poco::NotImplementedException(); }
		void replacePermissionsForUser(const std::string&, const std::set<std::string>&) override
			{ throw Poco::NotImplementedException(); }
		void revokePermissionsFromUser(const std::string&, const std::set<std::string>&) override
			{ throw Poco::NotImplementedException(); }
		int users(std::set<std::string>&, int, int) const override { throw Poco::NotImplementedException(); }
		int usersWithName(std::set<std::string>&, const std::string&, int, int) const override
			{ throw Poco::NotImplementedException(); }
		int usersWithPermission(std::set<std::string>&, const std::string&, int, int) const override
			{ throw Poco::NotImplementedException(); }
		int usersWithRole(std::set<std::string>&, const std::string&, int, int) const override
			{ throw Poco::NotImplementedException(); }
		int usersWithAttribute(std::set<std::string>&, const std::string&, const std::string&, int, int) const override
			{ throw Poco::NotImplementedException(); }
		bool userEnabled(const std::string&) const override { throw Poco::NotImplementedException(); }
		void addUser(const std::string&, const std::string&) override { throw Poco::NotImplementedException(); }
		void removeUser(const std::string&) override { throw Poco::NotImplementedException(); }
		void permissionsForRole(const std::string&, std::set<std::string>&) const override
			{ throw Poco::NotImplementedException(); }
		void grantPermissionsToRole(const std::string&, const std::set<std::string>&) override
			{ throw Poco::NotImplementedException(); }
		void replacePermissionsForRole(const std::string&, const std::set<std::string>&) override
			{ throw Poco::NotImplementedException(); }
		void revokePermissionsFromRole(const std::string&, const std::set<std::string>&) override
			{ throw Poco::NotImplementedException(); }
		void roles(std::set<std::string>&) const override { throw Poco::NotImplementedException(); }
		void addRole(const std::string&) override { throw Poco::NotImplementedException(); }
		void removeRole(const std::string&) override { throw Poco::NotImplementedException(); }
		void rolesForUser(const std::string&, std::set<std::string>&) const override
			{ throw Poco::NotImplementedException(); }
		void assignRoleToUser(const std::string&, const std::string&) override
			{ throw Poco::NotImplementedException(); }
		void removeRoleFromUser(const std::string&, const std::string&) override
			{ throw Poco::NotImplementedException(); }
		void replaceRolesForUser(const std::string&, const std::set<std::string>&) override
			{ throw Poco::NotImplementedException(); }
		void uncacheUser(const std::string&) override { throw Poco::NotImplementedException(); }
		std::string getUserAttribute(const std::string&, const std::string&, const std::string&) const override
			{ throw Poco::NotImplementedException(); }
		void removeUserAttribute(const std::string&, const std::string&) override
			{ throw Poco::NotImplementedException(); }
		void setAuthAttribute(const std::string&, const std::string&, const std::string&) override
			{ throw Poco::NotImplementedException(); }
		void removeAuthAttribute(const std::string&, const std::string&) override
			{ throw Poco::NotImplementedException(); }
		void replaceUserAttributes(const std::string&, const std::map<std::string, std::string>&) override
			{ throw Poco::NotImplementedException(); }
		void updateUserAttributes(const std::string&, const std::map<std::string, std::string>&) override
			{ throw Poco::NotImplementedException(); }
		void attributesForUser(const std::string&, std::set<std::string>&) const override
			{ throw Poco::NotImplementedException(); }
		int attributesForUser(const std::string&, std::map<std::string, std::string>&) const override
			{ throw Poco::NotImplementedException(); }
		std::vector<std::string> findUsersByAttribute(const std::string&, const std::string&) const override
			{ throw Poco::NotImplementedException(); }
		void revokePermission(const std::string&) override { throw Poco::NotImplementedException(); }
		void lockOutUser(const std::string&, const Poco::DateTime&) override { throw Poco::NotImplementedException(); }
		void unlockOutUser(const std::string&) override { throw Poco::NotImplementedException(); }
		bool isUserLockedOut(const std::string&) const override { throw Poco::NotImplementedException(); }
		bool isExternalUser(const std::string&) const override { throw Poco::NotImplementedException(); }
		bool isExternalRole(const std::string&) const override { throw Poco::NotImplementedException(); }
		void allRoleMappings(std::map<std::string, std::string>&) const override
			{ throw Poco::NotImplementedException(); }
		void roleMappingForExternal(const std::string&, std::string&) const override
			{ throw Poco::NotImplementedException(); }
		void setRoleMapping(const std::string&, const std::string&) override { throw Poco::NotImplementedException(); }
		void removeRoleMapping(const std::string&) override { throw Poco::NotImplementedException(); }
		void reset() override { throw Poco::NotImplementedException(); }
		void invalidateCaches(int) override { throw Poco::NotImplementedException(); }

	private:
		using AttributeStore = std::map<std::string, std::map<std::string, std::string>>;

		static std::string valueOf(const AttributeStore& store, const std::string& name, const std::string& attribute,
			const std::string& deflt)
		{
			const auto itName = store.find(name);
			if (itName == store.end()) return deflt;
			const auto itAttr = itName->second.find(attribute);
			return itAttr == itName->second.end() ? deflt : itAttr->second;
		}

		Directory* _pDirectory;
		mutable std::map<std::string, std::string> _credentials;
		mutable AttributeStore _userAttributes;
		mutable AttributeStore _roleAttributes;
		mutable std::map<std::string, Permissions> _userPermissions;
		mutable std::map<std::string, Roles> _userRoles;
		mutable std::set<std::string> _roles;
		mutable std::map<std::string, Permissions> _rolePermissions;
		std::set<std::string> _lockedOutAccounts;
		mutable std::vector<std::string> _checkedAccounts;
	};


	AbstractLDAPAuthAdminService::LDAPParams dnParams()
	{
		AbstractLDAPAuthAdminService::LDAPParams params;
		params.uri = "ldap://127.0.0.1"s;
		params.authMode = "DN"s;
		params.bindDN = "uid=%s,ou=people,dc=example,dc=com"s;
		params.searchBase = params.bindDN;
		params.userObjectClass = "inetOrgPerson"s;
		params.groupObjectClass = "groupOfNames"s;
		params.permissionAttribute = "permission"s;
		params.roleAttribute = "memberOf"s;
		params.rolePermissionAttribute = "permission"s;
		return params;
	}
}


LDAPAuthenticationTest::LDAPAuthenticationTest(const std::string& name): CppUnit::TestCase(name)
{
}


LDAPAuthenticationTest::~LDAPAuthenticationTest() = default;


void LDAPAuthenticationTest::testEmptyPasswordIsRejectedWithoutBind()
{
	Directory directory;
	TestLDAPAuthAdminService service(dnParams(), &directory);

	bool fallbackLocal = true;
	assertFalse (service.authenticate("victim"s, ""s, fallbackLocal));
	assertFalse (fallbackLocal);
	assertEqual (0, directory.initializeCount);
	assertTrue (directory.boundDNs.empty());
	assertFalse (service.hasUser("victim"s));
}


void LDAPAuthenticationTest::testNulCharacterIsRejectedWithoutBind()
{
	Directory directory;
	TestLDAPAuthAdminService service(dnParams(), &directory);

	bool fallbackLocal = true;
	assertFalse (service.authenticate("victim"s, "\0abc"s, fallbackLocal));
	assertFalse (service.authenticate("victim"s, "abc\0def"s, fallbackLocal));
	assertFalse (service.authenticate("vic\0tim"s, "secret"s, fallbackLocal));
	assertFalse (fallbackLocal);
	assertEqual (0, directory.initializeCount);
	assertTrue (directory.boundDNs.empty());
	assertFalse (service.hasUser("victim"s));
	assertFalse (service.hasUser("vic\0tim"s));
}


void LDAPAuthenticationTest::testServiceBindWithoutBindPasswordIsDirectoryError()
{
	Directory directory;
	auto params = dnParams();
	params.authMode = "ACCOUNT"s;
	params.bindUser = "cn=service,dc=example,dc=com"s;
	params.bindMode = "SERVICE"s;
	TestLDAPAuthAdminService service(params, &directory);

	bool fallbackLocal = true;
	try
	{
		service.authenticate("alice"s, "secret"s, fallbackLocal);
		fail("a service account without password must be reported as a directory error");
	}
	catch (Poco::IOException&)
	{
	}
	assertTrue (directory.boundDNs.empty());
	assertFalse (service.hasUser("alice"s));
}


void LDAPAuthenticationTest::testRejectedServiceAccountIsDirectoryError()
{
	Directory directory;
	directory.bindResult = LDAPClient::Result::WRONG_CREDENTIALS;
	auto params = dnParams();
	params.authMode = "ACCOUNT"s;
	params.bindUser = "cn=service,dc=example,dc=com"s;
	params.bindPassword = "wrong service secret"s;
	TestLDAPAuthAdminService service(params, &directory);

	bool fallbackLocal = true;
	try
	{
		service.authenticate("alice"s, "secret"s, fallbackLocal);
		fail("a rejected service account must be reported as a directory error");
	}
	catch (Poco::IOException&)
	{
	}
	assertFalse (service.hasUser("alice"s));
}


void LDAPAuthenticationTest::testWrongPasswordIsRejected()
{
	Directory directory;
	directory.bindResult = LDAPClient::Result::WRONG_CREDENTIALS;
	TestLDAPAuthAdminService service(dnParams(), &directory);

	bool fallbackLocal = true;
	assertFalse (service.authenticate("alice"s, "wrong"s, fallbackLocal));
	assertFalse (fallbackLocal);
	assertFalse (service.hasUser("alice"s));
}


void LDAPAuthenticationTest::testRefusedUserBindDoesNotFallBack()
{
	Directory directory;
	directory.bindResult = LDAPClient::Result::ERROR_RESULT;
	auto params = dnParams();
	params.localFallback = true;
	TestLDAPAuthAdminService service(params, &directory);
	service.setUserAttribute("alice"s, AbstractLDAPAuthAdminService::USER_ATTR_SUCCESS_TIME,
		std::to_string(Poco::Timestamp().epochTime()));

	bool fallbackLocal = true;
	assertFalse (service.authenticate("alice"s, "secret"s, fallbackLocal));
	assertFalse (fallbackLocal);
	assertFalse (service.hasUser("alice"s));
}


void LDAPAuthenticationTest::testAcceptedUserIsStored()
{
	Directory directory;
	directory.fullName = "Alice Example"s;
	directory.permissions = {"read"s};
	directory.attributes = {{"mail"s, "alice@example.com"s}};
	TestLDAPAuthAdminService service(dnParams(), &directory);

	bool fallbackLocal = true;
	assertTrue (service.authenticate("alice"s, "secret"s, fallbackLocal));
	assertFalse (fallbackLocal);
	assertTrue (service.hasUser("alice"s));
	assertEqual ("Alice Example"s, service.userAttribute("alice"s, AbstractLDAPAuthAdminService::USER_ATTR_FULL_NAME));
	assertEqual ("alice@example.com"s, service.userAttribute("alice"s, "mail"s));
	assertTrue (service.userPermissions("alice"s) == LDAPClient::Permissions{"read"s});
}


void LDAPAuthenticationTest::testBindDNIsBuiltFromBindDN()
{
	Directory directory;
	auto params = dnParams();
	params.searchBase = "ou=staff,dc=example,dc=com"s;
	TestLDAPAuthAdminService service(params, &directory);

	bool fallbackLocal = false;
	assertTrue (service.authenticate("bob"s, "secret"s, fallbackLocal));
	assertTrue (service.authenticate("a,b"s, "secret"s, fallbackLocal));
	assertEqual (2, directory.boundDNs.size());
	assertEqual ("uid=bob,ou=people,dc=example,dc=com"s, directory.boundDNs[0]);
	assertEqual ("uid=a\\,b,ou=people,dc=example,dc=com"s, directory.boundDNs[1]);
}


void LDAPAuthenticationTest::testBindDNFallsBackToSearchBase()
{
	Directory directory;
	auto params = dnParams();
	params.bindDN.clear();
	params.searchBase = "uid=%s,ou=staff,dc=example,dc=com"s;
	TestLDAPAuthAdminService service(params, &directory);

	bool fallbackLocal = false;
	assertTrue (service.authenticate("bob"s, "secret"s, fallbackLocal));
	assertEqual (1, directory.boundDNs.size());
	assertEqual ("uid=bob,ou=staff,dc=example,dc=com"s, directory.boundDNs[0]);
}


void LDAPAuthenticationTest::testServiceBindModeVerifiesFoundUserDN()
{
	Directory directory;
	auto params = dnParams();
	params.authMode = "ACCOUNT"s;
	params.bindUser = "cn=service,dc=example,dc=com"s;
	params.bindPassword = "service secret"s;
	params.bindMode = "SERVICE"s;
	TestLDAPAuthAdminService service(params, &directory);

	bool fallbackLocal = false;
	assertTrue (service.authenticate("alice"s, "secret"s, fallbackLocal));
	assertEqual (2, directory.initializeCount);
	assertTrue (directory.boundDNs ==
		std::vector<std::string>({"cn=service,dc=example,dc=com"s, directory.foundUserDN}));
}


void LDAPAuthenticationTest::testUnknownUserInServiceBindModeIsRejected()
{
	Directory directory;
	directory.findUserSucceeds = false;
	auto params = dnParams();
	params.authMode = "ACCOUNT"s;
	params.bindUser = "cn=service,dc=example,dc=com"s;
	params.bindPassword = "service secret"s;
	TestLDAPAuthAdminService service(params, &directory);

	bool fallbackLocal = true;
	assertFalse (service.authenticate("alice"s, "secret"s, fallbackLocal));
	assertFalse (fallbackLocal);
	assertTrue (directory.boundDNs == std::vector<std::string>({"cn=service,dc=example,dc=com"s}));
	assertFalse (service.hasUser("alice"s));
}


void LDAPAuthenticationTest::testUserBindModeBindsWithAccountName()
{
	Directory directory;
	directory.userPrincipalName = "alice@example.com"s;
	auto params = dnParams();
	params.authMode = "ACCOUNT"s;
	TestLDAPAuthAdminService service(params, &directory);

	bool fallbackLocal = false;
	assertTrue (service.authenticate("alice@example.com"s, "secret"s, fallbackLocal));
	assertEqual (1, directory.initializeCount);
	assertTrue (directory.boundDNs == std::vector<std::string>({"alice@example.com"s}));
}


void LDAPAuthenticationTest::testServiceBindModeReadsAccountNameBeforeUserBind()
{
	Directory directory;
	directory.accountName = "alice"s;
	auto params = dnParams();
	params.authMode = "ACCOUNT"s;
	params.bindUser = "cn=service,dc=example,dc=com"s;
	params.bindPassword = "service secret"s;
	TestLDAPAuthAdminService service(params, &directory);

	TestLDAPAuthAdminService::LDAPUser user;
	assertTrue (service.verify("alice@example.com"s, "secret"s, user) ==
		TestLDAPAuthAdminService::LDAPResult::ACCEPTED);
	assertEqual ("alice"s, user.accountName);
	assertTrue (service.checkedAccounts() == std::vector<std::string>({"alice"s}));
}


void LDAPAuthenticationTest::testServiceBindModeKeepsAccountNameOfSearch()
{
	Directory directory;
	directory.accountName = "Alice"s;
	directory.fetchReadsAccountName = false;
	auto params = dnParams();
	params.authMode = "ACCOUNT"s;
	params.bindUser = "cn=service,dc=example,dc=com"s;
	params.bindPassword = "service secret"s;
	TestLDAPAuthAdminService service(params, &directory);

	TestLDAPAuthAdminService::LDAPUser user;
	assertTrue (service.verify("alice@example.com"s, "secret"s, user) ==
		TestLDAPAuthAdminService::LDAPResult::ACCEPTED);
	assertEqual ("Alice"s, user.accountName);
	bool fallbackLocal = false;
	assertTrue (service.authenticate("alice@example.com"s, "secret"s, fallbackLocal));
	assertTrue (service.hasUser("alice"s));
	assertFalse (service.hasUser("alice@example.com"s));
}


void LDAPAuthenticationTest::testLockedOutAccountIsNotBoundInServiceBindMode()
{
	Directory directory;
	directory.accountName = "alice"s;
	auto params = dnParams();
	params.authMode = "ACCOUNT"s;
	params.bindUser = "cn=service,dc=example,dc=com"s;
	params.bindPassword = "service secret"s;
	params.localFallback = true;
	TestLDAPAuthAdminService service(params, &directory);
	service.lockOutAccount("alice"s);

	TestLDAPAuthAdminService::LDAPUser user;
	assertTrue (service.verify("alice@example.com"s, "secret"s, user) ==
		TestLDAPAuthAdminService::LDAPResult::LOCKED_OUT);
	assertFalse (user.passwordVerified);
	bool fallbackLocal = true;
	assertFalse (service.authenticate("alice@example.com"s, "secret"s, fallbackLocal));
	assertFalse (fallbackLocal);
	assertTrue (directory.boundDNs ==
		std::vector<std::string>({"cn=service,dc=example,dc=com"s, "cn=service,dc=example,dc=com"s}));
	assertFalse (service.hasUser("alice@example.com"s));
}


void LDAPAuthenticationTest::testUserBindModeReadsAccountNameAfterBind()
{
	Directory directory;
	directory.accountName = "alice"s;
	directory.userPrincipalName = "alice@example.com"s;
	directory.principalName = "COMPANY\\alice"s;
	auto params = dnParams();
	params.authMode = "ACCOUNT"s;
	TestLDAPAuthAdminService service(params, &directory);

	for (const auto& name: {"Alice@Example.com"s, "company\\ALICE"s, "Alice"s})
	{
		TestLDAPAuthAdminService::LDAPUser user;
		assertTrue (service.verify(name, "secret"s, user) == TestLDAPAuthAdminService::LDAPResult::ACCEPTED);
		assertEqual ("alice"s, user.accountName);
	}
	assertTrue (service.checkedAccounts() == std::vector<std::string>({"alice"s, "alice"s, "alice"s}));
}


void LDAPAuthenticationTest::testUserBindModeRefusesNameThatEntryDoesNotHave()
{
	Directory directory;
	directory.accountName = "alice"s;
	directory.userPrincipalName = "alice@example.com"s;
	directory.principalName = "COMPANY\\alice"s;
	directory.permissions = {"admin"s};
	auto params = dnParams();
	params.authMode = "ACCOUNT"s;
	TestLDAPAuthAdminService service(params, &directory);

	// The server may accept these names for other users than alice, whose
	// entry the search by name finds.
	for (const auto& name: {"OTHER\\alice"s, "alice@other.example.com"s})
	{
		TestLDAPAuthAdminService::LDAPUser user;
		assertTrue (service.verify(name, "secret"s, user) == TestLDAPAuthAdminService::LDAPResult::REFUSED);
		assertTrue (user.accountName.empty());
		assertTrue (user.permissions.empty());
		bool fallbackLocal = false;
		assertFalse (service.authenticate(name, "secret"s, fallbackLocal));
		assertFalse (service.hasUser(name));
	}

	// Without msDS-PrincipalName, a name with a domain is not a name of the entry.
	directory.principalName.clear();
	TestLDAPAuthAdminService::LDAPUser user;
	assertTrue (service.verify("COMPANY\\alice"s, "secret"s, user) == TestLDAPAuthAdminService::LDAPResult::REFUSED);
	assertTrue (service.checkedAccounts().empty());
	assertFalse (service.hasUser("alice"s));

	// A name without a domain is a name of the entry only if it is its account
	// name, which an entry found by another attribute, such as the user
	// principal name, may not have.
	for (const auto& accountName: {"bob"s, ""s})
	{
		directory.accountName = accountName;
		TestLDAPAuthAdminService::LDAPUser plainUser;
		assertTrue (service.verify("alice"s, "secret"s, plainUser) == TestLDAPAuthAdminService::LDAPResult::REFUSED);
		assertTrue (plainUser.accountName.empty());
		assertTrue (plainUser.permissions.empty());
		bool fallbackLocal = false;
		assertFalse (service.authenticate("alice"s, "secret"s, fallbackLocal));
		assertFalse (service.hasUser("alice"s));
		assertFalse (service.hasUser("bob"s));
	}
	assertTrue (service.checkedAccounts().empty());
}


void LDAPAuthenticationTest::testLockedOutAccountIsRefusedAfterBindInUserBindMode()
{
	Directory directory;
	directory.accountName = "alice"s;
	directory.userPrincipalName = "alice@example.com"s;
	auto params = dnParams();
	params.authMode = "ACCOUNT"s;
	TestLDAPAuthAdminService service(params, &directory);
	service.lockOutAccount("alice"s);

	TestLDAPAuthAdminService::LDAPUser user;
	assertTrue (service.verify("alice@example.com"s, "secret"s, user) ==
		TestLDAPAuthAdminService::LDAPResult::LOCKED_OUT);
	assertTrue (user.passwordVerified);
	bool fallbackLocal = true;
	assertFalse (service.authenticate("alice@example.com"s, "secret"s, fallbackLocal));
	assertFalse (fallbackLocal);
	assertFalse (service.hasUser("alice@example.com"s));
	assertFalse (service.hasUser("alice"s));
}


void LDAPAuthenticationTest::testAccountWithoutAccountNameIsCheckedUnderGivenName()
{
	Directory directory;
	auto serviceParams = dnParams();
	serviceParams.authMode = "ACCOUNT"s;
	serviceParams.bindUser = "cn=service,dc=example,dc=com"s;
	serviceParams.bindPassword = "service secret"s;
	TestLDAPAuthAdminService serviceBind(serviceParams, &directory);
	TestLDAPAuthAdminService dnMode(dnParams(), &directory);

	TestLDAPAuthAdminService::LDAPUser user;
	assertTrue (serviceBind.verify("Alice@Example.com"s, "secret"s, user) ==
		TestLDAPAuthAdminService::LDAPResult::ACCEPTED);
	assertTrue (serviceBind.checkedAccounts() == std::vector<std::string>({"alice@example.com"s}));
	assertTrue (dnMode.verify("Bob"s, "secret"s, user) == TestLDAPAuthAdminService::LDAPResult::ACCEPTED);
	assertTrue (dnMode.checkedAccounts() == std::vector<std::string>({"bob"s}));
}


void LDAPAuthenticationTest::testAcceptedUserIsStoredUnderAccountName()
{
	Directory directory;
	directory.accountName = "Alice"s;
	auto params = dnParams();
	params.authMode = "ACCOUNT"s;
	params.bindUser = "cn=service,dc=example,dc=com"s;
	params.bindPassword = "service secret"s;
	TestLDAPAuthAdminService service(params, &directory);

	bool fallbackLocal = false;
	assertTrue (service.authenticate("Alice@Example.com"s, "secret"s, fallbackLocal));
	assertTrue (service.hasUser("alice"s));
	assertFalse (service.hasUser("Alice@Example.com"s));

	directory.accountName.clear();
	assertTrue (service.authenticate("Bob@Example.com"s, "secret"s, fallbackLocal));
	assertTrue (service.hasUser("Bob@Example.com"s));
}


void LDAPAuthenticationTest::testRoleFilterSelectsImportedRoles()
{
	Directory directory;
	directory.roles = {"dev-team"s, "sales"s};
	directory.groupPermissions = {{"dev-team"s, {"build"s}}, {"sales"s, {"orders"s}}};
	auto params = dnParams();
	params.roleFilter = "dev.*"s;
	TestLDAPAuthAdminService service(params, &directory);

	bool fallbackLocal = false;
	assertTrue (service.authenticate("alice"s, "secret"s, fallbackLocal));
	assertTrue (service.userRoles("alice"s) == LDAPClient::Roles{"dev-team"s});
	assertTrue (service.rolePermissions("dev-team"s) == LDAPClient::Permissions{"build"s});
	assertFalse (service.roleExists("sales"s));
}


void LDAPAuthenticationTest::testLocalRolesAreNotImported()
{
	Directory directory;
	directory.roles = {"admin"s, "dev"s};
	directory.groupPermissions = {{"admin"s, {"all"s}}, {"dev"s, {"build"s}}};
	auto params = dnParams();
	params.localRoles = "admin|ops"s;
	TestLDAPAuthAdminService service(params, &directory);
	service.addRole("admin"s, {"admin.local"s});

	bool fallbackLocal = false;
	assertTrue (service.authenticate("alice"s, "secret"s, fallbackLocal));
	assertTrue (service.userRoles("alice"s) == LDAPClient::Roles{"dev"s});
	assertTrue (service.rolePermissions("admin"s) == LDAPClient::Permissions{"admin.local"s});
	assertTrue (service.rolePermissions("dev"s) == LDAPClient::Permissions{"build"s});
}


void LDAPAuthenticationTest::testLocalRoleIsNotOverwritten()
{
	Directory directory;
	directory.roles = {"operator"s};
	directory.groupPermissions = {{"operator"s, {"all"s}}};
	TestLDAPAuthAdminService service(dnParams(), &directory);
	service.addLocalRole("operator"s, {"operator.local"s});

	bool fallbackLocal = false;
	assertTrue (service.authenticate("alice"s, "secret"s, fallbackLocal));
	assertTrue (service.userRoles("alice"s).empty());
	assertTrue (service.rolePermissions("operator"s) == LDAPClient::Permissions{"operator.local"s});
}


void LDAPAuthenticationTest::testUnreadableUserEntryAfterBindThrows()
{
	Directory directory;
	directory.fetchSucceeds = false;
	TestLDAPAuthAdminService service(dnParams(), &directory);

	bool fallbackLocal = false;
	try
	{
		service.authenticate("alice"s, "secret"s, fallbackLocal);
		fail("a directory error after the bind must throw");
	}
	catch (Poco::IOException&)
	{
	}
}


void LDAPAuthenticationTest::testUnreadableGroupAfterBindThrows()
{
	Directory directory;
	directory.roles = {"dev"s};
	TestLDAPAuthAdminService service(dnParams(), &directory);

	bool fallbackLocal = false;
	try
	{
		service.authenticate("alice"s, "secret"s, fallbackLocal);
		fail("a directory error after the bind must throw");
	}
	catch (Poco::IOException&)
	{
	}
}


void LDAPAuthenticationTest::testInaccessibleServerFallsBackWithinLifetime()
{
	const Poco::Timestamp::TimeDiff DAY = Poco::Timespan::DAYS;
	struct Case
	{
		bool localFallback;
		int lifetimeDays;
		Poco::Timestamp::TimeDiff successAge;
		bool expectedFallback;
	};
	const std::vector<Case> cases =
	{
		{true, 7, DAY, true},
		{true, 7, 8*DAY, false},
		{false, 7, DAY, false},
		{true, 30000, DAY, true}
	};

	for (const auto& c: cases)
	{
		Directory directory;
		directory.bindResult = LDAPClient::Result::SERVER_INACCESSIBLE;
		auto params = dnParams();
		params.localFallback = c.localFallback;
		params.uNameLifeDaysDays = c.lifetimeDays;
		TestLDAPAuthAdminService service(params, &directory);
		const Poco::Timestamp successTime = Poco::Timestamp() - c.successAge;
		service.setUserAttribute("alice"s, AbstractLDAPAuthAdminService::USER_ATTR_SUCCESS_TIME,
			std::to_string(successTime.epochTime()));

		bool fallbackLocal = !c.expectedFallback;
		assertFalse (service.authenticate("alice"s, "secret"s, fallbackLocal));
		assertEqual (c.expectedFallback, fallbackLocal);
	}
}


void LDAPAuthenticationTest::testUnreachableServerFallsBack()
{
	Directory directory;
	directory.initializeSucceeds = false;
	auto params = dnParams();
	params.localFallback = true;
	TestLDAPAuthAdminService service(params, &directory);
	service.setUserAttribute("alice"s, AbstractLDAPAuthAdminService::USER_ATTR_SUCCESS_TIME,
		std::to_string(Poco::Timestamp().epochTime()));

	bool fallbackLocal = false;
	assertFalse (service.authenticate("alice"s, "secret"s, fallbackLocal));
	assertTrue (fallbackLocal);
	assertTrue (directory.boundDNs.empty());
}


void LDAPAuthenticationTest::testUnansweredServerFallsBackToLocalCredentials()
{
	Poco::Net::ServerSocket silentServer(Poco::Net::SocketAddress("127.0.0.1"s, 0));
	auto params = dnParams();
	params.uri = "ldap://127.0.0.1:"s + std::to_string(silentServer.address().port());
	params.timeout = 1;
	params.localFallback = true;
	TestLDAPAuthAdminService service(params, nullptr);
	service.setUserAttribute("alice"s, AbstractLDAPAuthAdminService::USER_ATTR_SUCCESS_TIME,
		std::to_string(Poco::Timestamp().epochTime()));

	bool fallbackLocal = false;
	assertFalse (service.authenticate("alice"s, "secret"s, fallbackLocal));
	assertTrue (fallbackLocal);
}


void LDAPAuthenticationTest::testTimeoutOfZeroIsReplacedByDefault()
{
	Directory directory;
	auto params = dnParams();
	params.timeout = 0;
	TestLDAPAuthAdminService service(params, &directory);

	bool fallbackLocal = false;
	assertTrue (service.authenticate("alice"s, "secret"s, fallbackLocal));
	assertTrue (directory.timeouts ==
		std::vector<Poco::Timespan>({Poco::Timespan(AbstractLDAPAuthAdminService::LDAPParams::DEFAULT_TIMEOUT, 0)}));
}


void LDAPAuthenticationTest::setUp()
{
}


void LDAPAuthenticationTest::tearDown()
{
}


CppUnit::Test* LDAPAuthenticationTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("LDAPAuthenticationTest");

	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testEmptyPasswordIsRejectedWithoutBind);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testNulCharacterIsRejectedWithoutBind);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testServiceBindWithoutBindPasswordIsDirectoryError);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testRejectedServiceAccountIsDirectoryError);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testWrongPasswordIsRejected);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testRefusedUserBindDoesNotFallBack);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testAcceptedUserIsStored);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testBindDNIsBuiltFromBindDN);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testBindDNFallsBackToSearchBase);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testServiceBindModeVerifiesFoundUserDN);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testUnknownUserInServiceBindModeIsRejected);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testUserBindModeBindsWithAccountName);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testServiceBindModeReadsAccountNameBeforeUserBind);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testServiceBindModeKeepsAccountNameOfSearch);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testLockedOutAccountIsNotBoundInServiceBindMode);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testUserBindModeReadsAccountNameAfterBind);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testUserBindModeRefusesNameThatEntryDoesNotHave);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testLockedOutAccountIsRefusedAfterBindInUserBindMode);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testAccountWithoutAccountNameIsCheckedUnderGivenName);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testAcceptedUserIsStoredUnderAccountName);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testRoleFilterSelectsImportedRoles);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testLocalRolesAreNotImported);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testLocalRoleIsNotOverwritten);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testUnreadableUserEntryAfterBindThrows);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testUnreadableGroupAfterBindThrows);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testInaccessibleServerFallsBackWithinLifetime);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testUnreachableServerFallsBack);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testUnansweredServerFallsBackToLocalCredentials);
	CppUnit_addTest(pSuite, LDAPAuthenticationTest, testTimeoutOfZeroIsReplacedByDefault);

	return pSuite;
}


#endif // POCO_OSP_AUTH_ENABLE_LDAP
