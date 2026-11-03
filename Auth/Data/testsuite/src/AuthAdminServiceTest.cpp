//
// AuthAdminServiceTest.cpp
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "AuthAdminServiceTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/TestSuite.h"
#include "Poco/Data/Binding.h"
#include "Poco/Data/DataException.h"
#include "Poco/Data/Extraction.h"
#include "Poco/Exception.h"
#include "Poco/Data/Session.h"
#include "Poco/Data/Statement.h"
#include "Poco/Data/SQLite/Connector.h"
#include "Poco/OSP/Auth/PooledAuthAdminService.h"
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
#include "Poco/OSP/Auth/LDAPClient.h"
#include "Poco/Event.h"
#include "Poco/Semaphore.h"
#include "Poco/Timespan.h"
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <thread>
#endif
#include "Poco/NotificationCenter.h"
#include "Poco/NObserver.h"
#include "Poco/Delegate.h"
#include "Poco/String.h"
#include "Poco/Format.h"
#include "Poco/File.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/Environment.h"
#include "Poco/FileStream.h"
#include "Poco/StringTokenizer.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Channel.h"
#include "Poco/Message.h"
#include "Poco/Mutex.h"
#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <new>
#include <vector>


using namespace Poco::OSP::Auth;
using namespace Poco::OSP::Auth::Data;
using namespace Poco::Data;
using namespace Poco::Data::Keywords;
using namespace std::string_literals;
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
using Poco::OSP::Auth::LDAPClient;
#endif


namespace
{
	constexpr int LOCK_OUT_SECONDS = 600;


	class CapturingChannel: public Poco::Channel
		/// Keeps the messages logged to it.
	{
	public:
		using Ptr = Poco::AutoPtr<CapturingChannel>;

		CapturingChannel() = default;
		CapturingChannel(const CapturingChannel&) = delete;
		CapturingChannel(CapturingChannel&&) = delete;
		CapturingChannel& operator = (const CapturingChannel&) = delete;
		CapturingChannel& operator = (CapturingChannel&&) = delete;

		void log(const Poco::Message& msg) override
		{
			Poco::FastMutex::ScopedLock lock(_mutex);
			_messages.push_back(msg);
		}

		[[nodiscard]] std::vector<Poco::Message> messages() const
		{
			Poco::FastMutex::ScopedLock lock(_mutex);
			return _messages;
		}

	protected:
		~CapturingChannel() override = default;

	private:
		std::vector<Poco::Message> _messages;
		mutable Poco::FastMutex _mutex;
	};


	AuthAdminServiceImpl::AuthParams backendAuthParams()
		/// Returns the parameters of the backends of a pool, which use auth.db.
	{
		constexpr int PBKDF2_ITERATIONS = 1000;
		AuthAdminServiceImpl::AuthParams authParams;
		authParams.dbConnector = "SQLite";
		authParams.dbConnectionString = "auth.db";
		authParams.hashVersion = 2;
		authParams.pbkdf2Iterations = PBKDF2_ITERATIONS;
		return authParams;
	}


	std::vector<AuthAdminService::Ptr> createBackends(const Poco::Util::AbstractConfiguration& properties,
		std::size_t count)
	{
		const AuthAdminServiceImpl::AuthParams authParams = backendAuthParams();
		AuthAdminServiceImpl::LDAPParams ldapParams;
		std::vector<AuthAdminService::Ptr> backends;
		for (std::size_t i = 0; i < count; i++)
		{
			backends.push_back(new AuthAdminServiceImpl(properties, Poco::Logger::root(), authParams, ldapParams));
		}
		return backends;
	}


	void createShippedDatabase(const std::string& path)
		/// Creates the SQLite database at path with Auth/sql/createdb.sql.
	{
		Poco::Data::Session session("SQLite", path);
		Poco::FileInputStream sqlStream(Poco::Environment::get("POCO_BASE") + "/Auth/sql/createdb.sql");
		std::string sql;
		std::string line;
		while (std::getline(sqlStream, line))
		{
			if (line.compare(0, 2, "--") != 0) sql += line + "\n";
		}
		Poco::StringTokenizer statements(sql, ";",
			Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
		for (const auto& statement: statements)
		{
			session << statement, now;
		}
	}


	class RoutingPool: public PooledAuthAdminService
		/// Makes the selection of the backend visible.
	{
	public:
		using Ptr = Poco::AutoPtr<RoutingPool>;

		explicit RoutingPool(const std::vector<AuthAdminService::Ptr>& backends):
			PooledAuthAdminService(backends)
		{
		}

		RoutingPool() = delete;
		~RoutingPool() override = default;

		using PooledAuthAdminService::forUser;
	};


	class AccountNameAuthAdminService: public AuthAdminServiceImpl
		/// Accepts the password "secret" for every user name as a login of the
		/// account "alice", and keeps the user names it is asked for.
	{
	public:
		using Ptr = Poco::AutoPtr<AccountNameAuthAdminService>;

		AccountNameAuthAdminService(const Poco::Util::AbstractConfiguration& properties, const AuthParams& authParams):
			AuthAdminServiceImpl(properties, Poco::Logger::root(), authParams, LDAPParams())
		{
		}

		AccountNameAuthAdminService() = delete;
		AccountNameAuthAdminService(const AccountNameAuthAdminService&) = delete;
		AccountNameAuthAdminService(AccountNameAuthAdminService&&) = delete;
		AccountNameAuthAdminService& operator = (const AccountNameAuthAdminService&) = delete;
		AccountNameAuthAdminService& operator = (AccountNameAuthAdminService&&) = delete;
		~AccountNameAuthAdminService() override = default;

		bool authenticateAccount(const std::string& userName, const std::string& credentials,
			std::string& accountName) const override
		{
			_userNames.push_back(userName);
			if (credentials != "secret") return false;
			accountName = "alice";
			return true;
		}

		[[nodiscard]] const std::vector<std::string>& userNames() const
		{
			return _userNames;
		}

	private:
		mutable std::vector<std::string> _userNames;
	};


	class ValidateUserRecorder
		/// Records the sender of the validateUser events.
	{
	public:
		ValidateUserRecorder() = default;
		~ValidateUserRecorder() = default;

		void onValidateUser(const void* pSender, AuthAdminService::ValidateUserEvent&)
		{
			_senders.push_back(pSender);
		}

		[[nodiscard]] const std::vector<const void*>& senders() const
		{
			return _senders;
		}

	private:
		std::vector<const void*> _senders;
	};


	class RemovedUserAuthAdminService: public AuthAdminServiceImpl
		/// Behaves as if each user is removed right after the login.
	{
	public:
		using Ptr = Poco::AutoPtr<RemovedUserAuthAdminService>;

		RemovedUserAuthAdminService(const Poco::Util::AbstractConfiguration& properties, const AuthParams& authParams,
			const LDAPParams& ldapParams):
			AuthAdminServiceImpl(properties, Poco::Logger::root(), authParams, ldapParams)
		{
		}

		RemovedUserAuthAdminService() = delete;
		RemovedUserAuthAdminService(const RemovedUserAuthAdminService&) = delete;
		RemovedUserAuthAdminService(RemovedUserAuthAdminService&&) = delete;
		RemovedUserAuthAdminService& operator = (const RemovedUserAuthAdminService&) = delete;
		RemovedUserAuthAdminService& operator = (RemovedUserAuthAdminService&&) = delete;
		~RemovedUserAuthAdminService() override = default;

		std::string getUserAttribute(const std::string& username, const std::string&, const std::string&) const override
		{
			throw Poco::NotFoundException(username);
		}
	};


#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	constexpr std::chrono::seconds EVENT_WAIT{5};
		/// The time a test waits for an event signalled by another thread.


	template <typename Waitable>
	[[nodiscard]] bool tryWaitForEvent(Waitable& waitable)
		/// Waits for the given event or semaphore for at most EVENT_WAIT.
	{
		return waitable.tryWait(static_cast<long>(std::chrono::milliseconds(EVENT_WAIT).count()));
	}


	struct Directory
		/// Scripted answers of the LDAP server, shared by the clients of a test.
	{
		Poco::FastMutex mutex;
		bool reachable = true;
		std::map<std::string, std::string> passwords;
			/// The bind DN and the password of each user the server accepts.
		std::string fullName;
		Poco::OSP::Auth::Permissions permissions;
		Poco::OSP::Auth::Roles roles;
		std::map<std::string, Poco::OSP::Auth::Permissions> groupPermissions;
		bool holdBinds = false;
			/// A bind signals bindStarted and waits for bindReleased.
		std::string heldBindName;
			/// If not empty, holdBinds holds only the binds with this name.
		Poco::Semaphore bindStarted{0, MAX_BINDS};
		Poco::Event bindReleased{Poco::Event::EVENT_MANUALRESET};
		std::function<void()> onBind;
			/// Called at the start of each bind.
		int binds = 0;
		bool searchFails = false;
		LDAPClient::Result bindResult = LDAPClient::Result::SUCCESS_RESULT;
			/// If not SUCCESS_RESULT, the result of every bind.
		bool readFails = false;
			/// The entry of a user cannot be read after a successful bind.
		std::string foundUserDN;
			/// The result of the search for the DN of a user; empty if none is found.
		std::string accountName;
			/// The account name of the entry of a user, read in the ACCOUNT mode.
		std::string userPrincipalName;
			/// The user principal name of that entry.
		std::string principalName;
			/// The msDS-PrincipalName of that entry.
		bool severalAccountNames = false;
			/// The entry of a user has more than one account name.

		static constexpr int MAX_BINDS = 16;
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
			Poco::FastMutex::ScopedLock lock(_directory.mutex);
			return _directory.reachable;
		}

		bool setTimeLimit(int) override
		{
			return true;
		}

		bool setTimeout(const Poco::Timespan&) override
		{
			return true;
		}

		Result bindDirect(const std::string& bindName, const std::string& credentials) override
		{
			bool hold = false;
			std::function<void()> onBind;
			{
				Poco::FastMutex::ScopedLock lock(_directory.mutex);
				hold = _directory.holdBinds && (_directory.heldBindName.empty() || _directory.heldBindName == bindName);
				onBind = _directory.onBind;
				++_directory.binds;
			}
			if (onBind) onBind();
			if (hold)
			{
				_directory.bindStarted.set();
				_directory.bindReleased.wait(static_cast<long>(std::chrono::milliseconds(HOLD_TIMEOUT).count()));
			}
			Poco::FastMutex::ScopedLock lock(_directory.mutex);
			if (_directory.bindResult != Result::SUCCESS_RESULT) return _directory.bindResult;
			const auto it = _directory.passwords.find(bindName);
			return (it != _directory.passwords.end() && it->second == credentials) ?
				Result::SUCCESS_RESULT : Result::WRONG_CREDENTIALS;
		}

		void unbind() override
		{
		}

		bool findUserDN(const std::string& username, std::string& userDN) override
		{
			Poco::FastMutex::ScopedLock lock(_directory.mutex);
			if (_directory.searchFails) throw Poco::IOException("The LDAP search for the user failed", username);
			readAccountName(username);
			userDN = _directory.foundUserDN;
			return !userDN.empty();
		}

		bool fetchOwnProperties() override
		{
			return fetchOwnProperties(""s);
		}

		bool fetchOwnProperties(const std::string& userDN) override
		{
			Poco::FastMutex::ScopedLock lock(_directory.mutex);
			if (_directory.readFails) return false;
			readAccountName(userDN);
			setOwnProperties(_directory.fullName, _directory.roles, _directory.permissions, {});
			return true;
		}

		bool fetchGroupPermissions(const std::string& groupDN, Permissions& permissions) override
		{
			Poco::FastMutex::ScopedLock lock(_directory.mutex);
			const auto it = _directory.groupPermissions.find(groupDN);
			if (it == _directory.groupPermissions.end()) return false;
			permissions = it->second;
			return true;
		}

	private:
		void readAccountName(const std::string& name)
			/// Reads the account name of the entry as LDAPClient does, only in
			/// the ACCOUNT mode. The directory must be locked.
		{
			if (mode() != Mode::ACCOUNT_NAME) return;
			if (_directory.severalAccountNames)
				throw Poco::IOException("The LDAP entry of the user has more than one account name", name);
			setAccountName(_directory.accountName);
			setUserPrincipalName(_directory.userPrincipalName);
			setPrincipalName(_directory.principalName);
		}

		static constexpr std::chrono::seconds HOLD_TIMEOUT{10};

		Directory& _directory;
	};


	class LDAPAuthAdminService: public AuthAdminServiceImpl
		/// Talks to a ScriptedLDAPClient instead of an LDAP server.
	{
	public:
		using Ptr = Poco::AutoPtr<LDAPAuthAdminService>;

		LDAPAuthAdminService(const Poco::Util::AbstractConfiguration& properties, const AuthParams& authParams,
			const LDAPParams& ldapParams, Directory& directory):
			AuthAdminServiceImpl(properties, Poco::Logger::get("AuthAdminServiceTest.LDAP"s), authParams, ldapParams),
			_directory(directory)
		{
		}

		LDAPAuthAdminService() = delete;
		LDAPAuthAdminService(const LDAPAuthAdminService&) = delete;
		LDAPAuthAdminService(LDAPAuthAdminService&&) = delete;
		LDAPAuthAdminService& operator = (const LDAPAuthAdminService&) = delete;
		LDAPAuthAdminService& operator = (LDAPAuthAdminService&&) = delete;
		~LDAPAuthAdminService() override = default;

		[[nodiscard]] bool waitForWaitingLogins(int count) const
			/// Waits until count logins wait for the LDAP round trip of another
			/// login, or for at most EVENT_WAIT. Returns true if they do.
		{
			const auto deadline = std::chrono::steady_clock::now() + EVENT_WAIT;
			while (loginsWaitingForLDAP() < count)
			{
				if (std::chrono::steady_clock::now() >= deadline) return false;
				std::this_thread::sleep_for(POLL_INTERVAL);
			}
			return true;
		}

	protected:
		std::unique_ptr<LDAPClient> createLDAPClient() const override
		{
			return std::make_unique<ScriptedLDAPClient>(_logger, _directory);
		}

	private:
		static constexpr std::chrono::milliseconds POLL_INTERVAL{1};

		Directory& _directory;
	};


	class UnstorableLDAPAuthAdminService: public LDAPAuthAdminService
		/// Fails with a database error when the full name of a user is stored.
	{
	public:
		using Ptr = Poco::AutoPtr<UnstorableLDAPAuthAdminService>;

		UnstorableLDAPAuthAdminService(const Poco::Util::AbstractConfiguration& properties,
			const AuthParams& authParams, const LDAPParams& ldapParams, Directory& directory):
			LDAPAuthAdminService(properties, authParams, ldapParams, directory)
		{
		}

		UnstorableLDAPAuthAdminService() = delete;
		UnstorableLDAPAuthAdminService(const UnstorableLDAPAuthAdminService&) = delete;
		UnstorableLDAPAuthAdminService(UnstorableLDAPAuthAdminService&&) = delete;
		UnstorableLDAPAuthAdminService& operator = (const UnstorableLDAPAuthAdminService&) = delete;
		UnstorableLDAPAuthAdminService& operator = (UnstorableLDAPAuthAdminService&&) = delete;
		~UnstorableLDAPAuthAdminService() override = default;

	protected:
		void replaceUserAttributeImpl(const std::string& username, const std::string& attribute,
			const std::string& value) const override
		{
			if (attribute == USER_ATTR_FULL_NAME)
				throw Poco::Data::DataException("cannot store the attribute", attribute);
			LDAPAuthAdminService::replaceUserAttributeImpl(username, attribute, value);
		}
	};


	class UncheckableLDAPAuthAdminService: public LDAPAuthAdminService
		/// Fails with a database error when an account is checked before the
		/// bind of the user.
	{
	public:
		using Ptr = Poco::AutoPtr<UncheckableLDAPAuthAdminService>;

		UncheckableLDAPAuthAdminService(const Poco::Util::AbstractConfiguration& properties,
			const AuthParams& authParams, const LDAPParams& ldapParams, Directory& directory):
			LDAPAuthAdminService(properties, authParams, ldapParams, directory)
		{
		}

		UncheckableLDAPAuthAdminService() = delete;
		UncheckableLDAPAuthAdminService(const UncheckableLDAPAuthAdminService&) = delete;
		UncheckableLDAPAuthAdminService(UncheckableLDAPAuthAdminService&&) = delete;
		UncheckableLDAPAuthAdminService& operator = (const UncheckableLDAPAuthAdminService&) = delete;
		UncheckableLDAPAuthAdminService& operator = (UncheckableLDAPAuthAdminService&&) = delete;
		~UncheckableLDAPAuthAdminService() override = default;

	protected:
		bool acceptLDAPAccount(const std::string&, const std::string& accountName) const override
		{
			throw Poco::Data::DataException("cannot read the account", accountName);
		}
	};


	AuthAdminServiceImpl::AuthParams ldapTestAuthParams()
	{
		AuthAdminServiceImpl::AuthParams authParams;
		authParams.dbConnector = "SQLite";
		authParams.dbConnectionString = "auth.db";
		authParams.hashVersion = 2;
		authParams.pbkdf2Iterations = 1000;
		return authParams;
	}


	AuthAdminServiceImpl::LDAPParams ldapTestParams()
	{
		AuthAdminServiceImpl::LDAPParams params;
		params.uri = "ldap://127.0.0.1";
		params.authMode = "DN";
		params.bindDN = "uid=%s,ou=people,dc=example,dc=com";
		params.searchBase = params.bindDN;
		params.userObjectClass = "inetOrgPerson";
		params.groupObjectClass = "groupOfNames";
		params.permissionAttribute = "permission";
		params.roleAttribute = "memberOf";
		params.rolePermissionAttribute = "permission";
		params.localFallback = true;
		return params;
	}


	AuthAdminServiceImpl::LDAPParams accountModeParams(bool serviceBind)
		/// Returns the LDAP parameters of the ACCOUNT mode with the SERVICE bind
		/// mode, whose service account the directory accepts with "service
		/// secret", if serviceBind is true, otherwise with the USER bind mode.
	{
		auto params = ldapTestParams();
		params.authMode = "ACCOUNT";
		if (serviceBind)
		{
			params.bindUser = "cn=service,dc=example,dc=com";
			params.bindPassword = "service secret";
		}
		return params;
	}


	std::string userDN(const std::string& username)
	{
		return "uid=" + username + ",ou=people,dc=example,dc=com";
	}


	void addServiceAccountAndAlice(Directory& directory)
		/// Lets the directory accept the service account of accountModeParams()
		/// and find the entry of alice, with the password "A" and the account
		/// name "Alice", for every name.
	{
		directory.passwords["cn=service,dc=example,dc=com"] = "service secret";
		directory.passwords[userDN("alice")] = "A";
		directory.foundUserDN = userDN("alice");
		directory.accountName = "Alice";
	}


	std::vector<std::pair<AuthAdminServiceImpl::LDAPParams, bool>> directoryErrorBeforeUserBindCases(bool localFallback)
		/// Returns the LDAP parameters of the SERVICE bind mode with which the
		/// LDAP directory cannot be used before the user bind, each with whether
		/// the search for the user fails: the service account has no password,
		/// the service account is rejected, the search for the user fails.
		/// The directory accepts the service account with "service secret".
	{
		auto serviceParams = ldapTestParams();
		serviceParams.localFallback = localFallback;
		serviceParams.authMode = "ACCOUNT";
		serviceParams.bindUser = "cn=service,dc=example,dc=com";
		serviceParams.bindPassword = "service secret";
		auto noPasswordParams = serviceParams;
		noPasswordParams.bindPassword.clear();
		auto wrongPasswordParams = serviceParams;
		wrongPasswordParams.bindPassword = "wrong";
		return {{noPasswordParams, false}, {wrongPasswordParams, false}, {serviceParams, true}};
	}
#endif
}


bool AuthAdminServiceTest::_firstRun(true);


AuthAdminServiceTest::AuthAdminServiceTest(const std::string& name, bool testInlineAttributes):
	CppUnit::TestCase(name),
	_testName(name),
	_testInlineAttributes(testInlineAttributes)
{
}


AuthAdminServiceTest::~AuthAdminServiceTest() = default;


void AuthAdminServiceTest::testUsers()
{
	_pAAS->addUser("user1", "user");
	_pAAS->addUser("user2", "resu");
	_pAAS->addUser("admin1", "admin");

	std::set<std::string> users;
	_pAAS->users(users);
	assert (users.size() == 3);
	assert (users.find("user1") != users.end());
	assert (users.find("user2") != users.end());
	assert (users.find("admin1") != users.end());

	_pAAS->removeUser("admin1");
	_pAAS->users(users);
	assert (users.size() == 2);
	assert (users.find("user1") != users.end());
	assert (users.find("user2") != users.end());
}


void AuthAdminServiceTest::testManyUsers()
{
	for (int i = 0; i < 100; i++)
	{
		_pAAS->addUser(Poco::format("user%02d", i), "s3cr3t");
	}

	std::set<std::string> users;
	_pAAS->users(users, 0, 10);
	assert (users.size() == 10);
	assert (users.find("user00") != users.end());
	assert (users.find("user09") != users.end());

	_pAAS->users(users, 10, 10);
	assert (users.size() == 10);
	assert (users.find("user10") != users.end());
	assert (users.find("user19") != users.end());

	_pAAS->users(users, 90, 10);
	assert (users.size() == 10);
	assert (users.find("user90") != users.end());
	assert (users.find("user99") != users.end());

	_pAAS->users(users, 100, 10);
	assert (users.size() == 0);
}


void AuthAdminServiceTest::testUsersWithNameMatchesSpecialCharacters()
{
	const std::set<std::string> names = {"a_b", "a%c", "axb", "a!b", "a\\b"};
	for (const auto& name: names)
	{
		_pAAS->addUser(name, "s3cr3t");
	}

	std::set<std::string> users;
	_pAAS->usersWithName(users, "a*");
	assertTrue (users == names);

	for (const auto& name: names)
	{
		_pAAS->usersWithName(users, name);
		assertTrue (users == std::set<std::string>{name});
	}

	_pAAS->usersWithName(users, "a?b");
	assertTrue (users == std::set<std::string>({"a_b", "axb", "a!b", "a\\b"}));
	_pAAS->usersWithName(users, "a_*");
	assertTrue (users == std::set<std::string>{"a_b"});
}


void AuthAdminServiceTest::testUsersWithPermission()
{
	_pAAS->addUser("user1", "s3cr3t");
	_pAAS->addUser("user2", "s3cr3t");
	_pAAS->addUser("user3", "s3cr3t");
	_pAAS->addUser("user4", "s3cr3t");

	std::set<std::string> perms1;
	perms1.insert("perm1");
	std::set<std::string> perms2;
	perms2.insert("perm2");

	_pAAS->grantPermissionsToUser("user1", perms1);
	_pAAS->grantPermissionsToUser("user2", perms2);
	_pAAS->grantPermissionsToUser("user3", perms1);
	_pAAS->grantPermissionsToUser("user4", perms2);

	std::set<std::string> users;
	_pAAS->usersWithPermission(users, "perm1");
	assert (users.size() == 2);
	assert (users.find("user1") != users.end());
	assert (users.find("user3") != users.end());

	users.clear();
	_pAAS->usersWithPermission(users, "perm2");
	assert (users.size() == 2);
	assert (users.find("user2") != users.end());
	assert (users.find("user4") != users.end());
}


void AuthAdminServiceTest::testUsersWithRole()
{
	_pAAS->addUser("user1", "s3cr3t");
	_pAAS->addUser("user2", "s3cr3t");
	_pAAS->addUser("user3", "s3cr3t");
	_pAAS->addUser("user4", "s3cr3t");

	_pAAS->addRole("role1");
	_pAAS->addRole("role2");

	_pAAS->assignRoleToUser("user1", "role1");
	_pAAS->assignRoleToUser("user2", "role2");
	_pAAS->assignRoleToUser("user3", "role1");
	_pAAS->assignRoleToUser("user4", "role2");

	std::set<std::string> users;
	_pAAS->usersWithRole(users, "role1");
	assert (users.size() == 2);
	assert (users.find("user1") != users.end());
	assert (users.find("user3") != users.end());

	users.clear();
	_pAAS->usersWithRole(users, "role2");
	assert (users.size() == 2);
	assert (users.find("user2") != users.end());
	assert (users.find("user4") != users.end());
}


void AuthAdminServiceTest::testUsersWithAttribute()
{
	_pAAS->addUser("user1", "s3cr3t");
	_pAAS->addUser("user2", "s3cr3t");
	_pAAS->addUser("user3", "s3cr3t");
	_pAAS->addUser("user4", "s3cr3t");

	_pAAS->setUserAttribute("user1", "attr", "value1");
	_pAAS->setUserAttribute("user2", "attr", "value2");
	_pAAS->setUserAttribute("user3", "attr", "value1");
	_pAAS->setUserAttribute("user4", "attr", "value2");

	std::set<std::string> users;
	_pAAS->usersWithAttribute(users, "attr", "value1");
	assert (users.size() == 2);
	assert (users.find("user1") != users.end());
	assert (users.find("user3") != users.end());

	users.clear();
	_pAAS->usersWithAttribute(users, "attr", "value2");
	assert (users.size() == 2);
	assert (users.find("user2") != users.end());
	assert (users.find("user4") != users.end());
}


void AuthAdminServiceTest::testRoles()
{
	_pAAS->addRole("user");
	_pAAS->addRole("admin");

	std::set<std::string> roles;
	_pAAS->roles(roles);
	assert (roles.size() == 2);
	assert (roles.find("user") != roles.end());
	assert (roles.find("admin") != roles.end());

	_pAAS->removeRole("admin");
	roles.clear();
	_pAAS->roles(roles);
	assert (roles.size() == 1);
	assert (roles.find("user") != roles.end());
}


void AuthAdminServiceTest::testPermissions()
{
	std::set<std::string> perms;

	_pAAS->addUser("user1", "pass1");
	_pAAS->addUser("user2", "pass2");
	_pAAS->addUser("admin1", "pass3");

	_pAAS->addRole("user");
	_pAAS->addRole("admin");

	perms.clear();
	perms.insert("perm1");
	perms.insert("perm2");
	_pAAS->grantPermissionsToRole("user", perms);

	perms.clear();
	perms.insert("admin");
	_pAAS->grantPermissionsToRole("admin", perms);

	perms.clear();
	perms.insert("perm3");
	_pAAS->grantPermissionsToUser("user1", perms);

	_pAAS->assignRoleToUser("user1", "user");
	_pAAS->assignRoleToUser("user2", "user");
	_pAAS->assignRoleToUser("admin1", "admin");
	_pAAS->assignRoleToUser("admin1", "user");

	std::set<std::string> roles;
	_pAAS->rolesForUser("user1", roles);
	assert (roles.size() == 1);
	assert (roles.find("user") != roles.end());

	roles.clear();
	_pAAS->rolesForUser("admin1", roles);
	assert (roles.size() == 2);
	assert (roles.find("user") != roles.end());
	assert (roles.find("admin") != roles.end());

	perms.clear();
	_pAAS->effectivePermissionsForUser("user1", perms);
	assert (perms.size() == 3);
	assert (perms.find("perm1") != perms.end());
	assert (perms.find("perm2") != perms.end());
	assert (perms.find("perm3") != perms.end());

	perms.clear();
	_pAAS->effectivePermissionsForUser("user2", perms);
	assert (perms.size() == 2);
	assert (perms.find("perm1") != perms.end());
	assert (perms.find("perm2") != perms.end());

	perms.clear();
	_pAAS->effectivePermissionsForUser("admin1", perms);
	assert (perms.size() == 3);
	assert (perms.find("perm1") != perms.end());
	assert (perms.find("perm2") != perms.end());
	assert (perms.find("admin") != perms.end());

	_pAAS->removeRoleFromUser("admin1", "user");
	roles.clear();
	_pAAS->rolesForUser("admin1", roles);
	assert (roles.size() == 1);
	assert (roles.find("admin") != roles.end());

	perms.clear();
	_pAAS->effectivePermissionsForUser("admin1", perms);
	assert (perms.size() == 1);
	assert (perms.find("admin") != perms.end());
}


void AuthAdminServiceTest::testAuthenticate()
{
	_pAAS->addUser("user1", "pass1");
	_pAAS->addUser("user2", "pass2");
	_pAAS->addUser("admin1", "pass3");

	assert (_pAAS->authenticate("user1", "pass1"));
	assert (_pAAS->authenticate("user2", "pass2"));
	assert (_pAAS->authenticate("admin1", "pass3"));
	assert (_pAAS->verifyCredentials("user1", "pass1"));

	assert (!_pAAS->authenticate("admin1", "pass1"));
	assert (!_pAAS->authenticate("admin2", "pass3"));
	assert (!_pAAS->verifyCredentials("admin1", "pass1"));
}


void AuthAdminServiceTest::testAuthenticateNotification()
{
	_pAAS->addUser("userNOT", "passNOT");

	auto& nc = Poco::NotificationCenter::defaultCenter();

	nc.addObserver(
		Poco::NObserver<AuthAdminServiceTest, Poco::OSP::Auth::AuthNotification>(*this, &AuthAdminServiceTest::handleNotification)
	);

	assert (_pAAS->authenticate("userNOT", "passNOT"));
	assert (!_pAAS->authenticate("userNOT", "passWRONG"));

	assert(_notificationCounter == 1);

	nc.removeObserver(
		Poco::NObserver<AuthAdminServiceTest, Poco::OSP::Auth::AuthNotification>(*this, &AuthAdminServiceTest::handleNotification)
	);
}


void AuthAdminServiceTest::testChangePasswordOfUnknownUserThrows()
{
	try
	{
		_pAAS->changePassword("nobody", "pass1");
		fail("changing the password of an unknown user must throw");
	}
	catch (Poco::NotFoundException&)
	{
	}
	assertFalse (_pAAS->authenticate("nobody", "pass1"));
}


void AuthAdminServiceTest::testEmptyPasswordIsRejected()
{
	try
	{
		_pAAS->addUser("user1", "");
		fail("adding a user with an empty password must throw");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
	assertFalse (_pAAS->userExists("user1"));

	_pAAS->addUser("user1", "pass1");
	try
	{
		_pAAS->changePassword("user1", "");
		fail("changing to an empty password must throw");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
	_pAAS->uncacheUser("user1");
	assertFalse (_pAAS->authenticate("user1", ""));
	assertTrue (_pAAS->authenticate("user1", "pass1"));
}


void AuthAdminServiceTest::testPasswordChangedBehindCacheIsAccepted()
{
	_pAAS->addUser("user1", "pass1");
	assertTrue (_pAAS->authenticate("user1", "pass1"));

	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.hashVersion = 4;
	authParams.pbkdf2Iterations = 50000;
	authParams.credentialsSalt = "poco";
	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pOther = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams,
		ldapParams);
	pOther->changePassword("user1", "pass2");

	assertTrue (_pAAS->authenticate("user1", "pass2"));
	assertFalse (_pAAS->authenticate("user1", "pass1"));
}


void AuthAdminServiceTest::testLoginOfUserRemovedBeforeNotificationSucceeds()
{
	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.hashVersion = 2;
	authParams.pbkdf2Iterations = 1000;
	AuthAdminServiceImpl::LDAPParams ldapParams;
	RemovedUserAuthAdminService::Ptr pAAS = new RemovedUserAuthAdminService(*_pProperties, authParams, ldapParams);
	pAAS->addUser("user1", "pass1");

	auto& nc = Poco::NotificationCenter::defaultCenter();
	const Poco::NObserver<AuthAdminServiceTest, Poco::OSP::Auth::AuthNotification> observer(*this,
		&AuthAdminServiceTest::handleNotification);
	nc.addObserver(observer);
	bool authenticated = false;
	try
	{
		authenticated = pAAS->authenticate("user1", "pass1");
	}
	catch (Poco::Exception&)
	{
		nc.removeObserver(observer);
		throw;
	}
	nc.removeObserver(observer);

	assertTrue (authenticated);
	assertEqual (0, _notificationCounter);
}


void AuthAdminServiceTest::testAuthenticateV1()
{
	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.cacheSize = 100;
	authParams.cacheTimeout = 3600;
	authParams.hashVersion = 1;
	authParams.saltLength = 8;
	authParams.pbkdf2Iterations = 10000;
	authParams.credentialsSalt = "poco";

	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	pAAS->addUser("user1", "pass1");
	pAAS->addUser("user2", "pass2");
	pAAS->addUser("admin1", "pass3");

	assert (pAAS->authenticate("user1", "pass1"));
	assert (pAAS->authenticate("user2", "pass2"));
	assert (pAAS->authenticate("admin1", "pass3"));

	assert (!pAAS->authenticate("admin1", "pass1"));
	assert (!pAAS->authenticate("admin2", "pass3"));
}


void AuthAdminServiceTest::testAuthenticateUpgradeFromV1ToV2()
{
	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.cacheSize = 100;
	authParams.cacheTimeout = 3600;
	authParams.hashVersion = 1;
	authParams.saltLength = 8;
	authParams.pbkdf2Iterations = 10000;
	authParams.credentialsSalt = "poco";

	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS1 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	pAAS1->addUser("user1", "pass1");
	pAAS1->addUser("user2", "pass2");
	pAAS1->addUser("admin1", "pass3");

	assert (pAAS1->authenticate("user1", "pass1"));
	assert (pAAS1->authenticate("user2", "pass2"));
	assert (pAAS1->authenticate("admin1", "pass3"));

	assert (!pAAS1->authenticate("admin1", "pass1"));
	assert (!pAAS1->authenticate("admin2", "pass3"));

	// check hashes are stored as V1 (MD5)
	Poco::Data::Session session("SQLite", "auth.db");
	std::string hash;
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assert (hash == "49ecb36e877c78513407a5d36d9459a5");

	authParams.hashVersion = 2;
	AuthAdminServiceImpl::Ptr pAAS2 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	assert (pAAS2->authenticate("user1", "pass1"));
	assert (pAAS2->authenticate("user2", "pass2"));
	assert (pAAS2->authenticate("admin1", "pass3"));

	assert (!pAAS2->authenticate("admin1", "pass1"));
	assert (!pAAS2->authenticate("admin2", "pass3"));

	// check hashes have been upgraded to V2
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assert (hash.compare(0, 4, "v=2;") == 0);
}


void AuthAdminServiceTest::testAuthenticateUpgradeFromV1ToV3()
{
	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.cacheSize = 100;
	authParams.cacheTimeout = 3600;
	authParams.hashVersion = 1;
	authParams.saltLength = 8;
	authParams.pbkdf2Iterations = 10000;
	authParams.credentialsSalt = "poco";

	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS1 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	pAAS1->addUser("user1", "pass1");
	pAAS1->addUser("user2", "pass2");
	pAAS1->addUser("admin1", "pass3");

	assert (pAAS1->authenticate("user1", "pass1"));
	assert (pAAS1->authenticate("user2", "pass2"));
	assert (pAAS1->authenticate("admin1", "pass3"));

	assert (!pAAS1->authenticate("admin1", "pass1"));
	assert (!pAAS1->authenticate("admin2", "pass3"));

	// check hashes are stored as V1 (MD5)
	Poco::Data::Session session("SQLite", "auth.db");
	std::string hash;
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assert (hash == "49ecb36e877c78513407a5d36d9459a5");

	authParams.hashVersion = 3;
	AuthAdminServiceImpl::Ptr pAAS3 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	assert (pAAS3->authenticate("user1", "pass1"));
	assert (pAAS3->authenticate("user2", "pass2"));
	assert (pAAS3->authenticate("admin1", "pass3"));

	assert (!pAAS3->authenticate("admin1", "pass1"));
	assert (!pAAS3->authenticate("admin2", "pass3"));

	// check hashes have been upgraded to V3
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assert (hash.compare(0, 4, "v=3;") == 0);

	assert (pAAS3->authenticate("user1", "pass1"));
	assert (pAAS3->authenticate("user2", "pass2"));
	assert (pAAS3->authenticate("admin1", "pass3"));

	assert (!pAAS3->authenticate("admin1", "pass1"));
	assert (!pAAS3->authenticate("admin2", "pass3"));
}


void AuthAdminServiceTest::testAuthenticateUpgradeFromV2ToV3()
{
	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.cacheSize = 100;
	authParams.cacheTimeout = 3600;
	authParams.hashVersion = 2;
	authParams.saltLength = 8;
	authParams.pbkdf2Iterations = 10000;
	authParams.credentialsSalt = "poco";

	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS2 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	pAAS2->addUser("user1", "pass1");
	pAAS2->addUser("user2", "pass2");
	pAAS2->addUser("admin1", "pass3");

	assert (pAAS2->authenticate("user1", "pass1"));
	assert (pAAS2->authenticate("user2", "pass2"));
	assert (pAAS2->authenticate("admin1", "pass3"));

	assert (!pAAS2->authenticate("admin1", "pass1"));
	assert (!pAAS2->authenticate("admin2", "pass3"));

	// check hashes are stored as V2
	Poco::Data::Session session("SQLite", "auth.db");

	std::string hash;
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assert (hash.compare(0, 4, "v=2;") == 0);

	authParams.hashVersion = 3;
	AuthAdminServiceImpl::Ptr pAAS3 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	assert (pAAS3->authenticate("user1", "pass1"));
	assert (pAAS3->authenticate("user2", "pass2"));
	assert (pAAS3->authenticate("admin1", "pass3"));

	assert (!pAAS3->authenticate("admin1", "pass1"));
	assert (!pAAS3->authenticate("admin2", "pass3"));

	// check hashes have been upgraded to V3
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assert (hash.compare(0, 4, "v=3;") == 0);

	assert (pAAS3->authenticate("user1", "pass1"));
	assert (pAAS3->authenticate("user2", "pass2"));
	assert (pAAS3->authenticate("admin1", "pass3"));

	assert (!pAAS3->authenticate("admin1", "pass1"));
	assert (!pAAS3->authenticate("admin2", "pass3"));
}


void AuthAdminServiceTest::testAuthenticateUpgradeFromV3ToV4()
{
	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.cacheSize = 100;
	authParams.cacheTimeout = 3600;
	authParams.hashVersion = 3;
	authParams.saltLength = 8;
	authParams.pbkdf2Iterations = 10000;
	authParams.credentialsSalt = "poco";

	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS3 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	pAAS3->addUser("user1", "pass1");
	pAAS3->addUser("user2", "pass2");
	pAAS3->addUser("admin1", "pass3");

	assert (pAAS3->authenticate("user1", "pass1"));
	assert (pAAS3->authenticate("user2", "pass2"));
	assert (pAAS3->authenticate("admin1", "pass3"));

	assert (!pAAS3->authenticate("admin1", "pass1"));
	assert (!pAAS3->authenticate("admin2", "pass3"));

	// check hashes are stored as V3
	Poco::Data::Session session("SQLite", "auth.db");

	std::string hash;
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assert (hash.compare(0, 4, "v=3;") == 0);

	assert (_pAAS->authenticate("user1", "pass1"));
	assert (_pAAS->authenticate("user2", "pass2"));
	assert (_pAAS->authenticate("admin1", "pass3"));

	assert (!_pAAS->authenticate("admin1", "pass1"));
	assert (!_pAAS->authenticate("admin2", "pass3"));

	// check hashes have been upgraded to V4
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assert (hash.compare(0, 4, "v=4;") == 0);

	authParams.hashVersion = 4;
	AuthAdminServiceImpl::Ptr pAAS4 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	assert (pAAS4->authenticate("user1", "pass1"));
	assert (pAAS4->authenticate("user2", "pass2"));
	assert (pAAS4->authenticate("admin1", "pass3"));

	assert (!pAAS4->authenticate("admin1", "pass1"));
	assert (!pAAS4->authenticate("admin2", "pass3"));
}


void AuthAdminServiceTest::testAuthenticateUpgradeFromV2ToV4()
{
	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.hashVersion = 2;
	authParams.saltLength = 8;
	authParams.pbkdf2Iterations = 10000;
	authParams.credentialsSalt = "poco";

	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS2 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams,
		ldapParams);
	pAAS2->addUser("user1", "pass1");

	Poco::Data::Session session("SQLite", "auth.db");
	std::string hash;
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assertTrue (hash.compare(0, 4, "v=2;") == 0);

	authParams.hashVersion = 4;
	AuthAdminServiceImpl::Ptr pAAS4 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams,
		ldapParams);
	assertTrue (pAAS4->authenticate("user1", "pass1"));

	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assertTrue (hash.compare(0, 4, "v=4;") == 0);
	pAAS4->uncacheUser("user1");
	assertTrue (pAAS4->authenticate("user1", "pass1"));
	assertFalse (pAAS4->authenticate("user1", "pass2"));
}


void AuthAdminServiceTest::testAuthenticateRaisesIterationCount()
{
	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.hashVersion = 4;
	authParams.saltLength = 8;
	authParams.pbkdf2Iterations = 50000;
	authParams.credentialsSalt = "poco";

	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS1 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams,
		ldapParams);
	pAAS1->addUser("user1", "pass1");
	assertEqual ("50000", pAAS1->getUserAttribute("user1", "$iterations"));

	authParams.pbkdf2Iterations = 60000;
	AuthAdminServiceImpl::Ptr pAAS2 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams,
		ldapParams);
	assertTrue (pAAS2->authenticate("user1", "pass1"));
	pAAS2->uncacheUser("user1");
	assertEqual ("60000", pAAS2->getUserAttribute("user1", "$iterations"));
	assertTrue (pAAS2->authenticate("user1", "pass1"));

	authParams.pbkdf2Iterations = 50000;
	AuthAdminServiceImpl::Ptr pAAS3 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams,
		ldapParams);
	assertTrue (pAAS3->authenticate("user1", "pass1"));
	pAAS3->uncacheUser("user1");
	assertEqual ("60000", pAAS3->getUserAttribute("user1", "$iterations"));
}


void AuthAdminServiceTest::testIterationsOfEmptyPasswordHashAreZero()
{
	_pAAS->addUser("user1", "pass1");
	Poco::Data::Session session("SQLite", "auth.db");
	session << "UPDATE auth_user SET password = '' WHERE username = 'user1'", now;
	_pAAS->uncacheUser("user1");

	assertEqual ("0", _pAAS->getUserAttribute("user1", "$iterations"));
	assertEqual ("1", _pAAS->getUserAttribute("user1", "$version"));
}


void AuthAdminServiceTest::testRandomSaltsKeepWhitespaceBytes()
{
	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.hashVersion = 3;
	authParams.saltLength = 8;
	authParams.pbkdf2Iterations = 1000;
	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams,
		ldapParams);

	// Six of the 256 byte values are whitespace, so 100 random salts of 8 bytes
	// contain none with a probability of less than 1e-8.
	constexpr int USER_COUNT = 100;
	bool whitespaceFound = false;
	for (int i = 0; i < USER_COUNT && !whitespaceFound; i++)
	{
		const std::string username = "user" + std::to_string(i);
		pAAS->addUser(username, "pass");
		const std::string salt = pAAS->getUserAttribute(username, "$salt");
		assertEqual (authParams.saltLength, static_cast<int>(salt.size()));
		whitespaceFound = salt.find_first_of(" \t\n\v\f\r") != std::string::npos;
	}
	assertTrue (whitespaceFound);
}


void AuthAdminServiceTest::testAuthorize()
{
	std::set<std::string> perms;

	_pAAS->addUser("user1", "pass1");
	_pAAS->addUser("user2", "pass2");
	_pAAS->addUser("admin1", "pass3");

	_pAAS->addRole("user");
	_pAAS->addRole("admin");

	perms.clear();
	perms.insert("perm1");
	perms.insert("perm2");
	_pAAS->grantPermissionsToRole("user", perms);

	perms.clear();
	perms.insert("admin");
	_pAAS->grantPermissionsToRole("admin", perms);

	perms.clear();
	perms.insert("perm3");
	_pAAS->grantPermissionsToUser("user1", perms);

	_pAAS->assignRoleToUser("user1", "user");
	_pAAS->assignRoleToUser("user2", "user");
	_pAAS->assignRoleToUser("admin1", "admin");
	_pAAS->assignRoleToUser("admin1", "user");

	assert (_pAAS->authorize("user1", "perm1"));
	assert (_pAAS->authorize("user1", "perm2"));
	assert (_pAAS->authorize("user1", "perm3"));

	assert (!_pAAS->authorize("user1", "admin"));
	assert (!_pAAS->authorize("user1", "perm4"));

	assert (_pAAS->authorize("user2", "perm1"));
	assert (_pAAS->authorize("user2", "perm2"));
	assert (!_pAAS->authorize("user2", "perm3"));
	assert (!_pAAS->authorize("user2", "admin"));

	assert (_pAAS->authorize("admin1", "perm1"));
	assert (_pAAS->authorize("admin1", "perm2"));
	assert (_pAAS->authorize("admin1", "admin"));
	assert (!_pAAS->authorize("admin1", "perm4"));
}


void AuthAdminServiceTest::testLongCacheTimeoutKeepsCachedPermissions()
{
	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.hashVersion = 2;
	authParams.pbkdf2Iterations = 1000;
	// More milliseconds than an int can hold. The overflow is undefined behaviour:
	// GCC wraps it, and UBSan reports it, but clang computes the product in 64 bits
	// when it optimises, so that this test cannot show the overflow there.
	authParams.cacheTimeout = 30*24*3600;
	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams,
		ldapParams);
	pAAS->addUser("user1", "pass1");
	pAAS->grantPermissionsToUser("user1", {"perm1"});
	assertTrue (pAAS->authorize("user1", "perm1"));

	Poco::Data::Session session("SQLite", "auth.db");
	session << "DELETE FROM auth_user_permissions WHERE username = 'user1'", now;
	assertTrue (pAAS->authorize("user1", "perm1"));
}


void AuthAdminServiceTest::testAttributes()
{
	_pAAS->addUser("user1", "pass1");
	_pAAS->addUser("user2", "pass2");

	std::set<std::string> attrs;
	_pAAS->attributesForUser("user1", attrs);

	assert (attrs.empty());

	_pAAS->setUserAttribute("user1", "attr1", "value1");
	_pAAS->setUserAttribute("user1", "attr2", "value2");

	_pAAS->attributesForUser("user1", attrs);
	assert (attrs.size() == 2);
	assert (attrs.find("attr1") != attrs.end());
	assert (attrs.find("attr2") != attrs.end());

	_pAAS->setUserAttribute("user1", "attr3", "value3");

	_pAAS->attributesForUser("user1", attrs);
	assert (attrs.size() == 3);
	assert (attrs.find("attr1") != attrs.end());
	assert (attrs.find("attr2") != attrs.end());
	assert (attrs.find("attr3") != attrs.end());

	assert (_pAAS->getUserAttribute("user1", "attr1") == "value1");
	assert (_pAAS->getUserAttribute("user1", "attr2") == "value2");
	assert (_pAAS->getUserAttribute("user1", "attr3") == "value3");

	_pAAS->removeUserAttribute("user1", "attr3");

	assert (_pAAS->getUserAttribute("user1", "attr1") == "value1");
	assert (_pAAS->getUserAttribute("user1", "attr2") == "value2");
	assert (_pAAS->getUserAttribute("user1", "attr3", "undefined") == "undefined");

	_pAAS->removeUserAttribute("user1", "attr2");
	assert (_pAAS->getUserAttribute("user1", "attr2", "undefined") == "undefined");

	attrs.clear();
	_pAAS->attributesForUser("user1", attrs);
	assert (attrs.size() == 1);
	assert (attrs.find("attr1") != attrs.end());

	_pAAS->setUserAttribute("user2", "attr1", "value3");
	attrs.clear();
	_pAAS->attributesForUser("user2", attrs);
	assert (attrs.size() == 1);
	assert (attrs.find("attr1") != attrs.end());

	assert (_pAAS->getUserAttribute("user2", "attr1") == "value3");
	assert (_pAAS->getUserAttribute("user1", "attr1") == "value1");

	std::vector<std::string> users;
	users = _pAAS->findUsersByAttribute("attr1", "value1");
	assert (users.size() == 1 && users[0] == "user1");
	users = _pAAS->findUsersByAttribute("attr1", "value3");
	assert (users.size() == 1 && users[0] == "user2");
	users = _pAAS->findUsersByAttribute("attr1", "valueX");
	assert (users.empty());
}


void AuthAdminServiceTest::testAttributeWithAuthPrefixWithoutUnderscoreIsUserAttribute()
{
	_pAAS->addUser("user1", "pass1");
	_pAAS->setUserAttribute("user1", "authority", "ca1");

	std::set<std::string> names;
	_pAAS->attributesForUser("user1", names);
	assertTrue (names.find("authority") != names.end());

	_pAAS->replaceUserAttributes("user1", {{"attr1", "value1"}});
	assertEqual ("", _pAAS->getUserAttribute("user1", "authority"));
}


void AuthAdminServiceTest::testAuthAttributesAreChangedOnlyThroughAuthInterface()
{
	_pAAS->addUser("user1", "pass1");
	for (const std::string attribute: {"auth_lockout", "auth_failures", "auth_ldap", "auth_tfa_secret",
		"AUTH_tfa_secret", "Auth_lockout"})
	{
		try
		{
			_pAAS->setUserAttribute("user1", attribute, "x");
			fail("setUserAttribute() must reject an authentication attribute");
		}
		catch (Poco::InvalidArgumentException&)
		{
		}
		try
		{
			_pAAS->removeUserAttribute("user1", attribute);
			fail("removeUserAttribute() must reject an authentication attribute");
		}
		catch (Poco::InvalidArgumentException&)
		{
		}
	}

	_pAAS->setAuthAttribute("user1", "auth_tfa_secret", "secret");
	assertEqual ("secret", _pAAS->getUserAttribute("user1", "auth_tfa_secret"));
	_pAAS->removeAuthAttribute("user1", "auth_tfa_secret");
	assertEqual ("", _pAAS->getUserAttribute("user1", "auth_tfa_secret"));

	try
	{
		_pAAS->setAuthAttribute("user1", "mail", "user1@example.com");
		fail("setAuthAttribute() must reject a user attribute");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
	try
	{
		_pAAS->setAuthAttribute("user1", "AUTH_tfa_secret", "secret");
		fail("setAuthAttribute() must reject an authentication attribute that is not in lower case");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
	try
	{
		_pAAS->removeAuthAttribute("user1", "mail");
		fail("removeAuthAttribute() must reject a user attribute");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
}


void AuthAdminServiceTest::testAttributesForUser()
{
	_pAAS->addUser("user1", "pass1");
	_pAAS->addUser("user2", "pass2");

	_pAAS->setUserAttribute("user1", "attr1", "value1");
	_pAAS->setUserAttribute("user1", "attr2", "value2");

	_pAAS->setUserAttribute("user2", "attr1", "value3");

	std::map<std::string, std::string> attrs;
	_pAAS->attributesForUser("user1", attrs);
	assert (attrs.size() == 2);
	assert (attrs["attr1"] == "value1");
	assert (attrs["attr2"] == "value2");

	attrs.clear();
	_pAAS->attributesForUser("user2", attrs);
	assert (attrs.size() == 1);
	assert (attrs["attr1"] == "value3");
}


void AuthAdminServiceTest::testLockOut()
{
	_pAAS->addUser("user1", "user");

	assert (_pAAS->authenticate("user1", "user"));
	assert (!_pAAS->isUserLockedOut("user1"));

	std::map<std::string, std::string> attrs;
	int flags = _pAAS->attributesForUser("user1", attrs);
	assert ((flags & AuthAdminService::USER_LOCKED_OUT) == 0);

	_pAAS->lockOutUser("user1", Poco::DateTime() + Poco::Timespan(600, 0));

	assert (_pAAS->isUserLockedOut("user1"));

	flags = _pAAS->attributesForUser("user1", attrs);
	assert ((flags & AuthAdminService::USER_LOCKED_OUT) != 0);
	assert (!_pAAS->authenticate("user1", "user"));

	_pAAS->unlockOutUser("user1");

	assert (!_pAAS->isUserLockedOut("user1"));
	assert (_pAAS->authenticate("user1", "user"));
}


void AuthAdminServiceTest::testLockOutAfterLoginFailures()
{
	Poco::Data::Session session("SQLite", "auth.db");
	std::string hash;
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.cacheSize = 100;
	authParams.cacheTimeout = 3600;
	authParams.hashVersion = 2;
	authParams.saltLength = 8;
	authParams.pbkdf2Iterations = 10000;
	authParams.credentialsSalt = "poco";
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;

	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	pAAS->addUser("user1", "pass1");

	// Successful login attempt after two failures
	assert (!pAAS->authenticate("user1", "bad"));
	assert (!pAAS->isUserLockedOut("user1"));

	assert (!pAAS->authenticate("user1", "bad"));
	assert (!pAAS->isUserLockedOut("user1"));

	assert (pAAS->authenticate("user1", "pass1"));
	assert (!pAAS->isUserLockedOut("user1"));

	// Lock out after three failures
	assert (!pAAS->authenticate("user1", "bad"));
	assert (!pAAS->isUserLockedOut("user1"));

	assert (!pAAS->authenticate("user1", "bad"));
	assert (!pAAS->isUserLockedOut("user1"));

	assert (!pAAS->authenticate("user1", "bad"));
	assert (pAAS->isUserLockedOut("user1"));

	assert (!pAAS->authenticate("user1", "pass1"));

	// Let the lock-out expire.
	const std::string past = Poco::DateTimeFormatter::format(Poco::DateTime() - Poco::Timespan(60, 0),
		Poco::DateTimeFormat::ISO8601_FORMAT);
	session << "UPDATE auth_user_attributes SET value = ? WHERE username = 'user1' AND attribute = 'auth_lockout'",
		useRef(past), now;

	assert (!pAAS->isUserLockedOut("user1"));
	assert (pAAS->authenticate("user1", "pass1"));
}


void AuthAdminServiceTest::testCachedLoginResetsFailureCount()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.cacheTimeout = 3600;
	authParams.hashVersion = 2;
	authParams.saltLength = 8;
	authParams.pbkdf2Iterations = 1000;
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;

	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams,
		ldapParams);
	pAAS->addUser("user1", "pass1");
	assertTrue (pAAS->authenticate("user1", "pass1"));

	assertFalse (pAAS->authenticate("user1", "bad"));
	assertFalse (pAAS->authenticate("user1", "bad"));
	assertTrue (pAAS->authenticate("user1", "pass1"));
	assertFalse (pAAS->authenticate("user1", "bad"));
	assertFalse (pAAS->isUserLockedOut("user1"));

	assertFalse (pAAS->authenticate("user1", "bad"));
	assertFalse (pAAS->authenticate("user1", "bad"));
	assertTrue (pAAS->isUserLockedOut("user1"));
}


void AuthAdminServiceTest::testMalformedLockOutTimeIsLoggedAndIgnored()
{
	CapturingChannel::Ptr pChannel = new CapturingChannel;
	Poco::Logger& logger = Poco::Logger::get("AuthAdminServiceTest.lockOut");
	logger.setChannel(pChannel);
	logger.setLevel(Poco::Message::PRIO_WARNING);

	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.hashVersion = 2;
	authParams.pbkdf2Iterations = 1000;
	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS = new AuthAdminServiceImpl(*_pProperties, logger, authParams, ldapParams);
	pAAS->addUser("user1", "pass1");

	Poco::Data::Session session("SQLite", "auth.db");
	session << "INSERT INTO auth_user_attributes VALUES ('user1', 'auth_lockout', 'not a time')", now;

	assertTrue (pAAS->authenticate("user1", "pass1"));
	const auto messages = pChannel->messages();
	assertFalse (messages.empty());
	assertTrue (messages[0].getPriority() == Poco::Message::PRIO_WARNING);
	logger.setChannel(nullptr);
}


void AuthAdminServiceTest::testLockOutWithNullableFailureCount()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER", now;

	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.hashVersion = 2;
	authParams.pbkdf2Iterations = 1000;
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams,
		ldapParams);
	pAAS->addUser("user1", "pass1");

	assertFalse (pAAS->authenticate("user1", "bad"));
	assertFalse (pAAS->authenticate("user1", "bad"));
	assertFalse (pAAS->isUserLockedOut("user1"));
	assertFalse (pAAS->authenticate("user1", "bad"));
	assertTrue (pAAS->isUserLockedOut("user1"));
}


void AuthAdminServiceTest::testMissingFailureCountColumnIsLogged()
{
	CapturingChannel::Ptr pChannel = new CapturingChannel;
	Poco::Logger& logger = Poco::Logger::get("AuthAdminServiceTest.failures");
	logger.setChannel(pChannel);
	logger.setLevel(Poco::Message::PRIO_WARNING);

	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS = new AuthAdminServiceImpl(*_pProperties, logger, authParams, ldapParams);
	logger.setChannel(nullptr);

	const auto messages = pChannel->messages();
	assertEqual (std::size_t{1}, messages.size());
	assertTrue (messages[0].getPriority() == Poco::Message::PRIO_WARNING);
}


void AuthAdminServiceTest::testLockOutWithShippedSchema()
{
	Poco::TemporaryFile dbFile;
	createShippedDatabase(dbFile.path());

	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = dbFile.path();
	authParams.hashVersion = 2;
	authParams.pbkdf2Iterations = 1000;
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams,
		ldapParams);
	assertTrue (pAAS->userExists("admin"));

	for (int i = 0; i < authParams.maxFailures; i++)
	{
		assertFalse (pAAS->isUserLockedOut("admin"));
		assertFalse (pAAS->authenticate("admin", "bad"));
	}
	assertTrue (pAAS->isUserLockedOut("admin"));
}


void AuthAdminServiceTest::testPoolRevokePermissionClearsEveryBackend()
{
	constexpr std::size_t BACKEND_COUNT = 4;
	constexpr int REPETITIONS = 20;
	PooledAuthAdminService::Ptr pPool = new PooledAuthAdminService(createBackends(*_pProperties, BACKEND_COUNT));
	pPool->addUser("user1", "pass1");
	for (int i = 0; i < REPETITIONS; i++)
	{
		pPool->grantPermissionsToUser("user1", {"perm1"});
		assertTrue (pPool->authorize("user1", "perm1"));
		pPool->revokePermission("perm1");
		assertFalse (pPool->authorize("user1", "perm1"));
	}
}


void AuthAdminServiceTest::testPoolRoutesUserNamesIgnoringCase()
{
	// The hash of a name modulo a power of two does not depend on the case of ASCII letters.
	constexpr std::size_t BACKEND_COUNT = 3;
	RoutingPool::Ptr pPool = new RoutingPool(createBackends(*_pProperties, BACKEND_COUNT));
	for (const std::string name: {"Bob", "ALICE", "Carol.Example", "dAvE"})
	{
		assertTrue (pPool->forUser(name) == pPool->forUser(Poco::toLower(name)));
	}
}


void AuthAdminServiceTest::testPoolAuthenticatesAccountWithBackendOfUser()
{
	constexpr std::size_t BACKEND_COUNT = 3;
	std::vector<AccountNameAuthAdminService::Ptr> backends;
	std::vector<AuthAdminService::Ptr> poolBackends;
	for (std::size_t i = 0; i < BACKEND_COUNT; i++)
	{
		backends.push_back(new AccountNameAuthAdminService(*_pProperties, backendAuthParams()));
		poolBackends.push_back(backends.back());
	}
	RoutingPool::Ptr pPool = new RoutingPool(poolBackends);

	std::string accountName;
	assertTrue (pPool->authenticateAccount("Alice@Example.com", "secret", accountName));
	assertEqual ("alice", accountName);
	const AuthAdminService::Ptr pBackendOfUser = pPool->forUser("Alice@Example.com");
	for (const auto& pBackend: backends)
	{
		const std::size_t expectedCalls = pBackend.get() == pBackendOfUser.get() ? 1 : 0;
		assertEqual (expectedCalls, pBackend->userNames().size());
	}
}


void AuthAdminServiceTest::testPoolForwardsValidateUserWithSender()
{
	PooledAuthAdminService::Ptr pPool = new PooledAuthAdminService(createBackends(*_pProperties, 2));
	ValidateUserRecorder recorder;
	pPool->validateUser += Poco::delegate(&recorder, &ValidateUserRecorder::onValidateUser);
	pPool->addUser("user1", "pass1");
	assertTrue (pPool->authenticate("user1", "pass1"));
	pPool->validateUser -= Poco::delegate(&recorder, &ValidateUserRecorder::onValidateUser);

	assertEqual (std::size_t{1}, recorder.senders().size());
	assertTrue (recorder.senders()[0] == pPool.get());
}


void AuthAdminServiceTest::testNeverLockOutUsersAreNotLockedOut()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.hashVersion = 2;
	authParams.pbkdf2Iterations = 1000;
	authParams.maxFailures = 2;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	authParams.neverLockOutUsers = {"admin"};
	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams,
		ldapParams);
	pAAS->addUser("admin", "pass1");
	pAAS->addUser("user1", "pass1");

	for (int i = 0; i < authParams.maxFailures; i++)
	{
		assertFalse (pAAS->authenticate("admin", "bad"));
		assertFalse (pAAS->authenticate("user1", "bad"));
	}
	assertFalse (pAAS->isUserLockedOut("admin"));
	assertTrue (pAAS->isUserLockedOut("user1"));
	assertTrue (pAAS->authenticate("admin", "pass1"));
}


void AuthAdminServiceTest::testValidateUserCanDisableUser()
{
	_pAAS->addUser("user1", "pass1");
	_pAAS->addUser("user2", "pass2");
	void (*disableUser1)(const void*, AuthAdminService::ValidateUserEvent&) =
		[](const void*, AuthAdminService::ValidateUserEvent& ev)
	{
		if (ev.username == "user1") ev.valid = false;
	};
	_pAAS->validateUser += Poco::delegate(disableUser1);

	const bool user1Authenticated = _pAAS->authenticate("user1", "pass1");
	const bool user1Enabled = _pAAS->userEnabled("user1");
	const bool user2Authenticated = _pAAS->authenticate("user2", "pass2");
	const bool user2Enabled = _pAAS->userEnabled("user2");
	_pAAS->validateUser -= Poco::delegate(disableUser1);

	assertFalse (user1Authenticated);
	assertFalse (user1Enabled);
	assertTrue (user2Authenticated);
	assertTrue (user2Enabled);
	assertTrue (_pAAS->verifyCredentials("user1", "pass1"));
}


void AuthAdminServiceTest::testReset()
{
	_pAAS->addUser("user1", "user");
	_pAAS->addUser("user2", "resu");
	_pAAS->addUser("admin1", "admin");

	_pAAS->reset();

	std::set<std::string> users;
	_pAAS->users(users);
	assert (users.size() == 3);
	assert (users.find("user1") != users.end());
	assert (users.find("user2") != users.end());
	assert (users.find("admin1") != users.end());
}


#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
void AuthAdminServiceTest::testExternalRole()
{
	_pAAS->addRole("extRole");
	_pAAS->addRole("localRole");
	_pAAS->addRole("undefRole");

	_pAAS->setRoleAttribute("extRole", AuthAdminService::ROLE_ATTR_LDAP, "true");
	_pAAS->setRoleAttribute("localRole", AuthAdminService::ROLE_ATTR_LDAP, "false");

	assert(_pAAS->isExternalRole("extRole"));
	assert(!_pAAS->isExternalRole("localRole"));
	assert(!_pAAS->isExternalRole("undefRole"));
}
#endif


#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
void AuthAdminServiceTest::testAdminOfShippedDatabaseIsLocalUserWithLDAP()
{
	Poco::TemporaryFile dbFile;
	createShippedDatabase(dbFile.path());
	Directory directory;
	auto authParams = ldapTestAuthParams();
	authParams.dbConnectionString = dbFile.path();
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, ldapTestParams(), directory);

	assertFalse (pAAS->isExternalUser("admin"));
	assertFalse (pAAS->authenticate("admin", "wrong"));
	assertTrue (pAAS->authenticate("admin", "admin"));
	assertEqual (0, directory.binds);
}


void AuthAdminServiceTest::testLDAPPasswordChangeUpdatesStoredHash()
{
	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), ldapTestParams(),
		directory);
	assertTrue (pAAS->authenticate("alice", "A"));

	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.passwords[userDN("alice")] = "B";
	}
	assertTrue (pAAS->authenticate("alice", "B"));

	Poco::Data::Session session("SQLite", "auth.db");
	std::string storedHash;
	session << "SELECT password FROM auth_user WHERE username = 'alice'", into(storedHash), now;
	assertTrue (storedHash.find(";h=" + pAAS->getUserAttribute("alice", "$hash")) != std::string::npos);

	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.reachable = false;
	}
	pAAS->invalidateCaches(AuthAdminService::INVALIDATE_CACHED_PASSWORDS);
	assertTrue (pAAS->authenticate("alice", "B"));
	assertFalse (pAAS->authenticate("alice", "A"));
}


void AuthAdminServiceTest::testLocalUserWithCapitalLettersIsNotLDAPUser()
{
	Directory directory;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), ldapTestParams(),
		directory);
	pAAS->addUser("Operator", "pass1");

	assertFalse (pAAS->isExternalUser("Operator"));
	assertTrue (pAAS->authenticate("Operator", "pass1"));
	pAAS->changePassword("Operator", "pass2");
	assertTrue (pAAS->authenticate("Operator", "pass2"));
}


void AuthAdminServiceTest::testOtherCallsProceedDuringLDAPLogin()
{
	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	directory.holdBinds = true;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), ldapTestParams(),
		directory);
	pAAS->addUser("bob", "pass1");

	bool authenticated = false;
	std::thread login([&]() { authenticated = pAAS->authenticate("alice", "A"); });
	const bool bindStarted = tryWaitForEvent(directory.bindStarted);

	Poco::Event otherCallDone;
	bool bobExists = false;
	std::thread otherCall([&]() { bobExists = pAAS->userExists("bob"); otherCallDone.set(); });
	const bool otherCallReturned = tryWaitForEvent(otherCallDone);

	directory.bindReleased.set();
	login.join();
	otherCall.join();

	assertTrue (bindStarted);
	assertTrue (otherCallReturned);
	assertTrue (bobExists);
	assertTrue (authenticated);
}


void AuthAdminServiceTest::testConcurrentLDAPLoginsOfOneUser()
{
	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	directory.fullName = "Alice Example";
	directory.permissions = {"read"};
	directory.roles = {"dev"};
	directory.groupPermissions = {{"dev", {"build"}}};
	directory.holdBinds = true;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), ldapTestParams(),
		directory);

	std::vector<int> results(2, 0);
	std::thread first([&]() { results[0] = pAAS->authenticate("alice", "A") ? 1 : 0; });
	std::thread second([&]() { results[1] = pAAS->authenticate("Alice", "A") ? 1 : 0; });
	const bool firstBindStarted = tryWaitForEvent(directory.bindStarted);
	directory.bindReleased.set();
	first.join();
	second.join();

	assertTrue (firstBindStarted);
	assertEqual (1, results[0]);
	assertEqual (1, results[1]);
	// The second login waits for the first one and then uses the cached password.
	assertEqual (1, directory.binds);

	std::set<std::string> users;
	pAAS->users(users);
	assertTrue (users == std::set<std::string>{"alice"});
	assertEqual ("Alice Example", pAAS->getUserAttribute("alice", AuthAdminService::USER_ATTR_FULL_NAME));
	std::set<std::string> permissions;
	pAAS->effectivePermissionsForUser("alice", permissions);
	assertTrue (permissions == std::set<std::string>({"build", "read"}));
	assertTrue (pAAS->authorize("Alice", "build"));
	pAAS->uncacheUser("alice");
	assertTrue (pAAS->authenticate("alice", "A"));
}


void AuthAdminServiceTest::testLDAPRoundTripsOfOneUserAreSequential()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 5;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, ldapTestParams(), directory);
	assertTrue (pAAS->authenticate("alice", "A"));

	std::vector<int> failuresAtBind;
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.holdBinds = true;
		directory.binds = 0;
		directory.onBind = [&]()
		{
			Poco::Data::Session reader("SQLite", "auth.db");
			int failures = 0;
			reader << "SELECT auth_failures FROM auth_user WHERE username = 'alice'", into(failures), now;
			Poco::FastMutex::ScopedLock lock(directory.mutex);
			failuresAtBind.push_back(failures);
		};
	}

	std::thread first([&]() { static_cast<void>(pAAS->authenticate("alice", "wrong1")); });
	const bool firstBindStarted = tryWaitForEvent(directory.bindStarted);
	std::thread second([&]() { static_cast<void>(pAAS->authenticate("alice", "wrong2")); });
	const bool secondLoginWaits = pAAS->waitForWaitingLogins(1);
	int bindsWhileFirstHeld = 0;
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		bindsWhileFirstHeld = directory.binds;
	}
	directory.bindReleased.set();
	first.join();
	second.join();

	assertTrue (firstBindStarted);
	assertTrue (secondLoginWaits);
	assertEqual (1, bindsWhileFirstHeld);
	assertTrue (failuresAtBind == std::vector<int>({0, 1}));
}


void AuthAdminServiceTest::testConcurrentLDAPAttemptsAreLimitedByFailureCount()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, ldapTestParams(), directory);
	assertTrue (pAAS->authenticate("alice", "A"));
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.holdBinds = true;
		directory.binds = 0;
	}

	std::vector<std::thread> attempts;
	attempts.reserve(static_cast<std::size_t>(authParams.maxFailures) + 2);
	for (int i = 0; i < authParams.maxFailures + 2; i++)
	{
		attempts.emplace_back([&]() { static_cast<void>(pAAS->authenticate("alice", "wrong")); });
	}
	const bool bindStarted = tryWaitForEvent(directory.bindStarted);
	const bool otherAttemptsWait = pAAS->waitForWaitingLogins(authParams.maxFailures + 1);
	int bindsWhileFirstHeld = 0;
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		bindsWhileFirstHeld = directory.binds;
	}
	directory.bindReleased.set();
	for (auto& attempt: attempts)
	{
		attempt.join();
	}

	assertTrue (bindStarted);
	assertTrue (otherAttemptsWait);
	assertEqual (1, bindsWhileFirstHeld);
	assertEqual (authParams.maxFailures, directory.binds);
	assertTrue (pAAS->isUserLockedOut("alice"));
}


void AuthAdminServiceTest::testLDAPSuccessDoesNotOverwriteAccountCreatedMeanwhile()
{
	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), ldapTestParams(),
		directory);
	assertTrue (pAAS->authenticate("alice", "A"));
	pAAS->uncacheUser("alice");
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.holdBinds = true;
	}

	bool authenticated = true;
	std::thread login([&]() { authenticated = pAAS->authenticate("alice", "A"); });
	const bool bindStarted = tryWaitForEvent(directory.bindStarted);
	pAAS->removeUser("alice");
	pAAS->addUser("alice", "local");
	directory.bindReleased.set();
	login.join();

	assertTrue (bindStarted);
	assertFalse (authenticated);
	assertFalse (pAAS->isExternalUser("alice"));
	assertTrue (pAAS->authenticate("alice", "local"));
}


void AuthAdminServiceTest::testLDAPRejectionKeepsPasswordOfAccountCreatedMeanwhile()
{
	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), ldapTestParams(),
		directory);
	assertTrue (pAAS->authenticate("alice", "A"));
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.holdBinds = true;
	}

	bool authenticated = true;
	std::thread login([&]() { authenticated = pAAS->authenticate("alice", "wrong"); });
	const bool bindStarted = tryWaitForEvent(directory.bindStarted);
	pAAS->removeUser("alice");
	pAAS->addUser("alice", "local");
	directory.bindReleased.set();
	login.join();

	assertTrue (bindStarted);
	assertFalse (authenticated);
	assertTrue (pAAS->authenticate("alice", "local"));
}


void AuthAdminServiceTest::testLockOutDuringLDAPLoginDiscardsLDAPAnswer()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	directory.fullName = "Alice Example";
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, ldapTestParams(), directory);
	assertTrue (pAAS->authenticate("alice", "A"));
	const std::string storedHash = pAAS->getUserAttribute("alice", "$hash");
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.passwords[userDN("alice")] = "B";
		directory.fullName = "Alice Changed";
		directory.holdBinds = true;
	}

	// The new password would be stored, and a wrong one would clear the
	// stored password and count a failure, if the answer were used.
	for (const auto& password: {"B"s, "wrong"s})
	{
		directory.bindReleased.reset();
		bool authenticated = true;
		std::thread login([&]() { authenticated = pAAS->authenticate("alice", password); });
		const bool bindStarted = tryWaitForEvent(directory.bindStarted);
		pAAS->lockOutUser("alice", Poco::DateTime() + Poco::Timespan(LOCK_OUT_SECONDS, 0));
		directory.bindReleased.set();
		login.join();

		assertTrue (bindStarted);
		assertFalse (authenticated);
		assertEqual (storedHash, pAAS->getUserAttribute("alice", "$hash"));
		assertEqual ("Alice Example", pAAS->getUserAttribute("alice", AuthAdminService::USER_ATTR_FULL_NAME));
		int failures = -1;
		session << "SELECT auth_failures FROM auth_user WHERE username = 'alice'", into(failures), now;
		assertEqual (0, failures);
		pAAS->unlockOutUser("alice");
	}
}


void AuthAdminServiceTest::testLDAPRoundTripsOfDifferentUsersRunInParallel()
{
	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	directory.passwords[userDN("bob")] = "B";
	directory.holdBinds = true;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), ldapTestParams(),
		directory);

	std::vector<int> results(2, 0);
	std::thread alice([&]() { results[0] = pAAS->authenticate("alice", "A") ? 1 : 0; });
	std::thread bob([&]() { results[1] = pAAS->authenticate("bob", "B") ? 1 : 0; });
	const bool firstBindStarted = tryWaitForEvent(directory.bindStarted);
	const bool secondBindStarted = tryWaitForEvent(directory.bindStarted);
	directory.bindReleased.set();
	alice.join();
	bob.join();

	assertTrue (firstBindStarted);
	assertTrue (secondBindStarted);
	assertEqual (1, results[0]);
	assertEqual (1, results[1]);
}


void AuthAdminServiceTest::testLoginWaitingTooLongForOtherLDAPLoginIsRefused()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	CapturingChannel::Ptr pChannel = new CapturingChannel;
	Poco::Logger& logger = Poco::Logger::get("AuthAdminServiceTest.LDAP"s);
	logger.setChannel(pChannel);
	logger.setLevel(Poco::Message::PRIO_WARNING);

	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	auto ldapParams = ldapTestParams();
	ldapParams.timeout = 1;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, ldapParams, directory);
	assertTrue (pAAS->authenticate("alice", "A"));
	pAAS->uncacheUser("alice");
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.holdBinds = true;
	}

	std::thread first([&]() { static_cast<void>(pAAS->authenticate("alice", "A")); });
	const bool bindStarted = tryWaitForEvent(directory.bindStarted);
	const bool authenticated = pAAS->authenticate("alice", "wrong");
	directory.bindReleased.set();
	first.join();
	logger.setChannel(nullptr);

	assertTrue (bindStarted);
	assertFalse (authenticated);
	int failures = -1;
	session << "SELECT auth_failures FROM auth_user WHERE username = 'alice'", into(failures), now;
	assertEqual (0, failures);
	assertEqual (std::size_t{1}, pChannel->messages().size());
}


void AuthAdminServiceTest::testLDAPDirectoryProblemKeepsLocalPassword()
{
	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	directory.passwords["cn=service,dc=example,dc=com"] = "service secret";
	directory.foundUserDN = userDN("alice");
	LDAPAuthAdminService::Ptr pDN = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), ldapTestParams(),
		directory);
	assertTrue (pDN->authenticate("alice", "A"));

	for (const auto& [params, searchFails]: directoryErrorBeforeUserBindCases(true))
	{
		{
			Poco::FastMutex::ScopedLock lock(directory.mutex);
			directory.searchFails = searchFails;
		}
		LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), params,
			directory);
		assertFalse (pAAS->authenticate("alice", "wrong"));
		assertTrue (pAAS->authenticate("alice", "A"));
		pAAS->uncacheUser("alice");
	}
}


void AuthAdminServiceTest::testLDAPDirectoryErrorWithoutLocalFallbackRefusesStoredPassword()
{
	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	directory.passwords["cn=service,dc=example,dc=com"] = "service secret";
	directory.foundUserDN = userDN("alice");
	LDAPAuthAdminService::Ptr pDN = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), ldapTestParams(),
		directory);
	assertTrue (pDN->authenticate("alice", "A"));
	const std::string storedHash = pDN->getUserAttribute("alice", "$hash");

	for (const auto& [params, searchFails]: directoryErrorBeforeUserBindCases(false))
	{
		{
			Poco::FastMutex::ScopedLock lock(directory.mutex);
			directory.searchFails = searchFails;
		}
		LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), params,
			directory);
		assertFalse (pAAS->authenticate("alice", "A"));
		assertEqual (storedHash, pAAS->getUserAttribute("alice", "$hash"));
	}
}


void AuthAdminServiceTest::testLDAPDirectoryErrorBeforeUserBindCountsWrongPasswords()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	directory.passwords["cn=service,dc=example,dc=com"] = "service secret";
	directory.foundUserDN = userDN("alice");
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	LDAPAuthAdminService::Ptr pDN = new LDAPAuthAdminService(*_pProperties, authParams, ldapTestParams(), directory);
	assertTrue (pDN->authenticate("alice", "A"));

	for (const auto& [params, searchFails]: directoryErrorBeforeUserBindCases(true))
	{
		{
			Poco::FastMutex::ScopedLock lock(directory.mutex);
			directory.searchFails = searchFails;
		}
		LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, params, directory);
		for (int i = 0; i < authParams.maxFailures; i++)
		{
			assertFalse (pAAS->isUserLockedOut("alice"));
			assertFalse (pAAS->authenticate("alice", "wrong"));
		}
		assertTrue (pAAS->isUserLockedOut("alice"));
		pAAS->unlockOutUser("alice");
	}
}


void AuthAdminServiceTest::testLDAPDirectoryErrorAfterAcceptedPasswordCountsNoFailure()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, ldapTestParams(), directory);
	assertTrue (pAAS->authenticate("alice", "A"));

	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.passwords[userDN("alice")] = "B";
		directory.readFails = true;
	}
	for (int i = 0; i < authParams.maxFailures; i++)
	{
		assertFalse (pAAS->authenticate("alice", "B"));
	}

	assertFalse (pAAS->isUserLockedOut("alice"));
	int failures = -1;
	session << "SELECT auth_failures FROM auth_user WHERE username = 'alice'", into(failures), now;
	assertEqual (0, failures);
}


void AuthAdminServiceTest::testRefusedLDAPBindCountsFailureAndKeepsPassword()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, ldapTestParams(), directory);
	assertTrue (pAAS->authenticate("alice", "A"));
	const std::string storedHash = pAAS->getUserAttribute("alice", "$hash");

	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.bindResult = LDAPClient::Result::ERROR_RESULT;
	}
	// After a refused login, the cached password "A" is not accepted either.
	assertFalse (pAAS->authenticate("alice", "B"));
	for (int i = 1; i < authParams.maxFailures; i++)
	{
		assertFalse (pAAS->isUserLockedOut("alice"));
		assertFalse (pAAS->authenticate("alice", "A"));
	}

	assertTrue (pAAS->isUserLockedOut("alice"));
	assertEqual (storedHash, pAAS->getUserAttribute("alice", "$hash"));
}


void AuthAdminServiceTest::testInaccessibleLDAPServerKeepsStoredPassword()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, ldapTestParams(), directory);
	assertTrue (pAAS->authenticate("alice", "A"));
	const std::string storedHash = pAAS->getUserAttribute("alice", "$hash");

	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.bindResult = LDAPClient::Result::SERVER_INACCESSIBLE;
	}
	for (int i = 0; i < authParams.maxFailures; i++)
	{
		pAAS->uncacheUser("alice");
		assertTrue (pAAS->authenticate("alice", "A"));
	}

	assertFalse (pAAS->isUserLockedOut("alice"));
	assertEqual (storedHash, pAAS->getUserAttribute("alice", "$hash"));
}


void AuthAdminServiceTest::testDatabaseErrorWhileStoringLDAPUserRollsBack()
{
	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	directory.fullName = "Alice Example";
	UnstorableLDAPAuthAdminService::Ptr pAAS = new UnstorableLDAPAuthAdminService(*_pProperties, ldapTestAuthParams(),
		ldapTestParams(), directory);

	try
	{
		[[maybe_unused]] const bool authenticated = pAAS->authenticate("alice", "A");
		fail("a database error while storing the LDAP user must be reported");
	}
	catch (Poco::Data::DataException&)
	{
	}
	assertFalse (pAAS->userExists("alice"));

	LDAPAuthAdminService::Ptr pStoring = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), ldapTestParams(),
		directory);
	assertTrue (pStoring->authenticate("alice", "A"));
	const std::string storedHash = pAAS->getUserAttribute("alice", "$hash");
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.passwords[userDN("alice")] = "B";
	}
	try
	{
		[[maybe_unused]] const bool authenticated = pAAS->authenticate("alice", "B");
		fail("a database error while storing the new password of the LDAP user must be reported");
	}
	catch (Poco::Data::DataException&)
	{
	}
	assertEqual (storedHash, pAAS->getUserAttribute("alice", "$hash"));
}


void AuthAdminServiceTest::testLoginNamesOfDirectoryUserShareOneAccount()
{
	Directory directory;
	addServiceAccountAndAlice(directory);
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(),
		accountModeParams(true), directory);

	std::string accountName;
	assertTrue (pAAS->authenticateAccount("alice", "A", accountName));
	assertEqual ("alice", accountName);
	pAAS->setAuthAttribute("alice", "auth_tfa_secret", "secret");

	accountName.clear();
	assertTrue (pAAS->authenticateAccount("Alice@Example.com", "A", accountName));
	assertEqual ("alice", accountName);
	assertFalse (pAAS->userExists("alice@example.com"));
	assertEqual ("secret", pAAS->getUserAttribute(accountName, "auth_tfa_secret"));
}


void AuthAdminServiceTest::testFailedLoginsThroughEveryNameLockOutAccount()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	Directory directory;
	addServiceAccountAndAlice(directory);
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, accountModeParams(true),
		directory);
	assertTrue (pAAS->authenticate("alice", "A"));

	for (const std::string name: {"alice@example.com", "COMPANY\\alice", "ALICE"})
	{
		assertFalse (pAAS->isUserLockedOut("alice"));
		assertFalse (pAAS->authenticate(name, "wrong"));
	}
	assertTrue (pAAS->isUserLockedOut("alice"));
}


void AuthAdminServiceTest::testLockedOutAccountIsNotTriedThroughOtherName()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	Directory directory;
	addServiceAccountAndAlice(directory);
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, accountModeParams(true),
		directory);
	assertTrue (pAAS->authenticate("alice", "A"));
	assertFalse (pAAS->authenticate("alice", "wrong"));
	pAAS->lockOutUser("alice", Poco::DateTime() + Poco::Timespan(LOCK_OUT_SECONDS, 0));
	int binds = 0;
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		binds = directory.binds;
	}

	std::string accountName;
	assertFalse (pAAS->authenticateAccount("alice@example.com", "A", accountName));
	assertTrue (accountName.empty());
	{
		// Only the service account is bound.
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		assertEqual (binds + 1, directory.binds);
	}
	int failures = -1;
	session << "SELECT auth_failures FROM auth_user WHERE username = 'alice'", into(failures), now;
	assertEqual (1, failures);
}


void AuthAdminServiceTest::testLockOutDuringLoginThroughOtherNameDiscardsLDAPAnswer()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	Directory directory;
	addServiceAccountAndAlice(directory);
	directory.fullName = "Alice Example";
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, accountModeParams(true),
		directory);
	assertTrue (pAAS->authenticate("alice", "A"));
	assertFalse (pAAS->authenticate("alice", "wrong"));
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.fullName = "Alice Changed";
		directory.holdBinds = true;
		directory.heldBindName = userDN("alice");
	}

	// The account is locked out while the LDAP server checks the password.
	bool authenticated = true;
	std::string accountName;
	std::thread login([&]() { authenticated = pAAS->authenticateAccount("alice@example.com", "A", accountName); });
	const bool bindStarted = tryWaitForEvent(directory.bindStarted);
	pAAS->lockOutUser("alice", Poco::DateTime() + Poco::Timespan(LOCK_OUT_SECONDS, 0));
	directory.bindReleased.set();
	login.join();

	assertTrue (bindStarted);
	assertFalse (authenticated);
	assertTrue (accountName.empty());
	assertTrue (pAAS->isUserLockedOut("alice"));
	assertEqual ("Alice Example", pAAS->getUserAttribute("alice", AuthAdminService::USER_ATTR_FULL_NAME));
	int failures = -1;
	session << "SELECT auth_failures FROM auth_user WHERE username = 'alice'", into(failures), now;
	assertEqual (1, failures);
	assertFalse (pAAS->userExists("alice@example.com"));
}


void AuthAdminServiceTest::testUserBindModeStoresLoginUnderAccountName()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	Directory directory;
	directory.passwords["alice@example.com"] = "A";
	directory.accountName = "Alice";
	directory.userPrincipalName = "alice@example.com";
	directory.fullName = "Alice Example";
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, accountModeParams(false),
		directory);

	std::string accountName;
	assertTrue (pAAS->authenticateAccount("Alice@Example.com", "A", accountName));
	assertEqual ("alice", accountName);
	assertFalse (pAAS->userExists("alice@example.com"));
	assertFalse (pAAS->authenticate("alice", "wrong"));

	pAAS->lockOutUser("alice", Poco::DateTime() + Poco::Timespan(LOCK_OUT_SECONDS, 0));
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.fullName = "Changed";
	}
	accountName.clear();
	assertFalse (pAAS->authenticateAccount("Alice@Example.com", "A", accountName));
	assertTrue (accountName.empty());
	int failures = -1;
	session << "SELECT auth_failures FROM auth_user WHERE username = 'alice'", into(failures), now;
	assertEqual (1, failures);
	assertEqual ("Alice Example", pAAS->getUserAttribute("alice", AuthAdminService::USER_ATTR_FULL_NAME));
	assertFalse (pAAS->userExists("alice@example.com"));
}


void AuthAdminServiceTest::testUserBindModeRefusesNameThatEntryDoesNotHave()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	// The directory accepts these names for other users than alice, and the
	// search by name finds the entry of alice for each of them.
	Directory directory;
	directory.passwords["alice"] = "A";
	directory.passwords["other\\alice"] = "X";
	directory.passwords["alice@other.example.com"] = "X";
	directory.passwords["company\\alice"] = "A";
	directory.accountName = "alice";
	directory.userPrincipalName = "alice@example.com";
	directory.principalName = "COMPANY\\alice";
	directory.permissions = {"alice.perm"};
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, accountModeParams(false),
		directory);
	assertTrue (pAAS->authenticate("alice", "A"));
	assertFalse (pAAS->authenticate("alice", "wrong"));

	for (const std::string name: {"OTHER\\alice", "alice@other.example.com"})
	{
		std::string accountName;
		assertFalse (pAAS->authenticateAccount(name, "X", accountName));
		assertTrue (accountName.empty());
		assertFalse (pAAS->userExists(name));
		assertFalse (pAAS->authorize(name, "alice.perm"));
	}
	int failures = -1;
	session << "SELECT auth_failures FROM auth_user WHERE username = 'alice'", into(failures), now;
	assertEqual (1, failures);

	// Without msDS-PrincipalName, a name with a domain is not a name of the entry.
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.principalName.clear();
	}
	std::string accountName;
	assertFalse (pAAS->authenticateAccount("COMPANY\\alice", "A", accountName));
	assertTrue (accountName.empty());
	assertFalse (pAAS->userExists("company\\alice"));

	// A name without a domain is a name of the entry only if it is its account
	// name, which an entry found by another attribute, such as the user
	// principal name, may not have. The refused login counts for the local
	// account of the given name.
	const std::string fullName = pAAS->getUserAttribute("alice", AuthAdminService::USER_ATTR_FULL_NAME);
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.fullName = "Bob Example";
		directory.permissions = {"bob.perm"};
	}
	int expectedFailures = 1;
	for (const std::string entryAccountName: {"bob", ""})
	{
		{
			Poco::FastMutex::ScopedLock lock(directory.mutex);
			directory.accountName = entryAccountName;
		}
		accountName.clear();
		assertFalse (pAAS->authenticateAccount("alice", "A", accountName));
		assertTrue (accountName.empty());
		assertFalse (pAAS->userExists("bob"));
		assertFalse (pAAS->authorize("alice", "bob.perm"));
		assertEqual (fullName, pAAS->getUserAttribute("alice", AuthAdminService::USER_ATTR_FULL_NAME));
		session << "SELECT auth_failures FROM auth_user WHERE username = 'alice'", into(failures), now;
		++expectedFailures;
		assertEqual (expectedFailures, failures);
	}
}


void AuthAdminServiceTest::testUserBindModeAcceptsDomainNameOfEntry()
{
	Directory directory;
	directory.passwords["company\\alice"] = "A";
	directory.accountName = "Alice";
	directory.userPrincipalName = "alice@example.com";
	directory.principalName = "COMPANY\\alice";
	directory.permissions = {"alice.perm"};
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(),
		accountModeParams(false), directory);

	std::string accountName;
	assertTrue (pAAS->authenticateAccount("COMPANY\\alice", "A", accountName));
	assertEqual ("alice", accountName);
	assertFalse (pAAS->userExists("company\\alice"));
	assertTrue (pAAS->authorize("alice", "alice.perm"));
}


void AuthAdminServiceTest::testOutageAfterSearchChecksStoredPasswordOfAccount()
{
	Directory directory;
	addServiceAccountAndAlice(directory);
	directory.passwords[userDN("alice@example.com")] = "OLD";
	LDAPAuthAdminService::Ptr pDN = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), ldapTestParams(),
		directory);
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(),
		accountModeParams(true), directory);
	// An account stored under another name of alice, as by earlier versions.
	assertTrue (pDN->authenticate("alice@example.com", "OLD"));
	assertTrue (pAAS->authenticate("alice", "A"));

	// The service account binds and finds the entry, then the LDAP server
	// cannot be reached for the bind of the user.
	directory.onBind = [&directory]()
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.reachable = false;
	};
	const auto loginDuringOutage = [&](const std::string& password, std::string& accountName)
	{
		{
			Poco::FastMutex::ScopedLock lock(directory.mutex);
			directory.reachable = true;
		}
		return pAAS->authenticateAccount("alice@example.com", password, accountName);
	};
	std::string accountName;
	assertFalse (loginDuringOutage("OLD", accountName));
	assertTrue (accountName.empty());
	assertTrue (loginDuringOutage("A", accountName));
	assertEqual ("alice", accountName);
}


void AuthAdminServiceTest::testLoginWithoutAccountNameIsStoredUnderTypedName()
{
	Directory directory;
	addServiceAccountAndAlice(directory);
	directory.accountName.clear();
	directory.passwords[userDN("bob")] = "B";
	LDAPAuthAdminService::Ptr pService = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(),
		accountModeParams(true), directory);
	LDAPAuthAdminService::Ptr pDN = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), ldapTestParams(),
		directory);

	std::string accountName;
	assertTrue (pService->authenticateAccount("Alice@Example.com", "A", accountName));
	assertEqual ("alice@example.com", accountName);
	assertTrue (pService->userExists("alice@example.com"));

	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.accountName = "Alice";
	}
	assertTrue (pDN->authenticateAccount("Bob", "B", accountName));
	assertEqual ("bob", accountName);
	assertTrue (pDN->userExists("bob"));
}


void AuthAdminServiceTest::testAuthenticateRefusesOtherNameThanAccountName()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	Directory directory;
	addServiceAccountAndAlice(directory);
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, accountModeParams(true),
		directory);
	assertTrue (pAAS->authenticate("alice", "A"));
	assertFalse (pAAS->authenticate("alice", "wrong"));

	assertFalse (pAAS->authenticate("alice@example.com", "A"));
	assertFalse (pAAS->userExists("alice@example.com"));
	int failures = -1;
	session << "SELECT auth_failures FROM auth_user WHERE username = 'alice'", into(failures), now;
	assertEqual (0, failures);
	assertTrue (pAAS->authenticate("ALICE", "A"));

	// The account name and the given name differ only in the case of a letter
	// that is not ASCII: U+00C9 and U+00E9 (E with acute accent).
	directory.accountName = "\xC3\x89mile";
	assertTrue (pAAS->authenticate("\xC3\xA9mile", "A"));
}


void AuthAdminServiceTest::testVerifyCredentialsRefusesOtherNameThanAccountName()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	Directory directory;
	addServiceAccountAndAlice(directory);
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, accountModeParams(true),
		directory);
	assertTrue (pAAS->verifyCredentials("alice", "A"));
	assertFalse (pAAS->verifyCredentials("alice", "wrong"));

	assertFalse (pAAS->verifyCredentials("alice@example.com", "A"));
	assertFalse (pAAS->userExists("alice@example.com"));
	int failures = -1;
	session << "SELECT auth_failures FROM auth_user WHERE username = 'alice'", into(failures), now;
	assertEqual (0, failures);
	assertTrue (pAAS->verifyCredentials("ALICE", "A"));

	// The account name and the given name differ only in the case of a letter
	// that is not ASCII: U+00C9 and U+00E9 (E with acute accent).
	directory.accountName = "\xC3\x89mile";
	assertTrue (pAAS->verifyCredentials("\xC3\xA9mile", "A"));
}


void AuthAdminServiceTest::testDatabaseErrorWhileCheckingAccountIsReported()
{
	Directory directory;
	addServiceAccountAndAlice(directory);
	UncheckableLDAPAuthAdminService::Ptr pAAS = new UncheckableLDAPAuthAdminService(*_pProperties, ldapTestAuthParams(),
		accountModeParams(true), directory);

	try
	{
		[[maybe_unused]] const bool authenticated = pAAS->authenticate("alice@example.com", "A");
		fail("a database error while checking the account must be reported");
	}
	catch (Poco::Data::DataException&)
	{
	}
	assertFalse (pAAS->userExists("alice@example.com"));
}


void AuthAdminServiceTest::testAccountNameOfLocalUserIsRefused()
{
	Directory directory;
	addServiceAccountAndAlice(directory);
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(),
		accountModeParams(true), directory);
	pAAS->addUser("alice", "local");
	const std::string storedHash = pAAS->getUserAttribute("alice", "$hash");

	std::string accountName;
	assertFalse (pAAS->authenticateAccount("alice@example.com", "A", accountName));
	assertTrue (accountName.empty());
	assertEqual (storedHash, pAAS->getUserAttribute("alice", "$hash"));
	assertFalse (pAAS->userExists("alice@example.com"));
	assertTrue (pAAS->authenticate("alice", "local"));
}


void AuthAdminServiceTest::testLoginThroughOtherNameWaitsForLoginOfAccount()
{
	Directory directory;
	addServiceAccountAndAlice(directory);
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(),
		accountModeParams(true), directory);
	assertTrue (pAAS->authenticate("alice", "A"));
	pAAS->uncacheUser("alice");
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.holdBinds = true;
		directory.heldBindName = userDN("alice");
		directory.binds = 0;
	}

	std::vector<std::string> accountNames(2);
	std::vector<int> results(2, 0);
	std::thread first([&]() { results[0] = pAAS->authenticateAccount("alice", "A", accountNames[0]) ? 1 : 0; });
	const bool firstBindStarted = tryWaitForEvent(directory.bindStarted);
	std::thread second([&]()
		{ results[1] = pAAS->authenticateAccount("alice@example.com", "A", accountNames[1]) ? 1 : 0; });
	const bool secondLoginWaits = pAAS->waitForWaitingLogins(1);
	int bindsWhileFirstHeld = 0;
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		bindsWhileFirstHeld = directory.binds;
	}
	directory.bindReleased.set();
	first.join();
	second.join();

	assertTrue (firstBindStarted);
	assertTrue (secondLoginWaits);
	// The service account of both logins, and the user of the first one.
	assertEqual (3, bindsWhileFirstHeld);
	assertEqual (1, results[0]);
	assertEqual (1, results[1]);
	assertTrue (accountNames == std::vector<std::string>({"alice", "alice"}));
}


void AuthAdminServiceTest::testLoginThroughOtherNameWaitingTooLongIsRefused()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	CapturingChannel::Ptr pChannel = new CapturingChannel;
	Poco::Logger& logger = Poco::Logger::get("AuthAdminServiceTest.LDAP"s);
	logger.setChannel(pChannel);
	logger.setLevel(Poco::Message::PRIO_WARNING);

	Directory directory;
	addServiceAccountAndAlice(directory);
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	auto ldapParams = accountModeParams(true);
	ldapParams.timeout = 1;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, ldapParams, directory);
	assertTrue (pAAS->authenticate("alice", "A"));
	pAAS->uncacheUser("alice");
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.holdBinds = true;
		directory.heldBindName = userDN("alice");
		directory.binds = 0;
	}

	std::thread first([&]() { static_cast<void>(pAAS->authenticate("alice", "A")); });
	const bool bindStarted = tryWaitForEvent(directory.bindStarted);
	std::string accountName;
	const bool authenticated = pAAS->authenticateAccount("alice@example.com", "A", accountName);
	int bindsWhileFirstHeld = 0;
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		bindsWhileFirstHeld = directory.binds;
	}
	directory.bindReleased.set();
	first.join();
	logger.setChannel(nullptr);

	assertTrue (bindStarted);
	assertFalse (authenticated);
	assertTrue (accountName.empty());
	// The service account of both logins, and the user of the first one.
	assertEqual (3, bindsWhileFirstHeld);
	int failures = -1;
	session << "SELECT auth_failures FROM auth_user WHERE username = 'alice'", into(failures), now;
	assertEqual (0, failures);
	assertEqual (std::size_t{1}, pChannel->messages().size());
}


void AuthAdminServiceTest::testConcurrentLoginsThroughNamesOfAccountAreLimitedByFailureCount()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	Directory directory;
	addServiceAccountAndAlice(directory);
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 1;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, accountModeParams(true),
		directory);
	assertTrue (pAAS->authenticate("alice", "A"));
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.holdBinds = true;
		directory.heldBindName = userDN("alice");
		directory.binds = 0;
	}

	constexpr int LOGINS = 4;
	std::vector<std::thread> attempts;
	attempts.reserve(LOGINS);
	for (int i = 0; i < LOGINS; i++)
	{
		attempts.emplace_back([&pAAS, i]()
			{ static_cast<void>(pAAS->authenticate("alice@guess" + std::to_string(i) + ".example.com", "wrong")); });
	}
	const bool bindStarted = tryWaitForEvent(directory.bindStarted);
	const bool otherLoginsWait = pAAS->waitForWaitingLogins(LOGINS - 1);
	int bindsWhileFirstHeld = 0;
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		bindsWhileFirstHeld = directory.binds;
	}
	directory.bindReleased.set();
	for (auto& attempt: attempts)
	{
		attempt.join();
	}

	assertTrue (bindStarted);
	assertTrue (otherLoginsWait);
	// Every login binds the service account; only the first one binds as the user.
	assertEqual (LOGINS + 1, bindsWhileFirstHeld);
	assertEqual (LOGINS + 1, directory.binds);
	assertTrue (pAAS->isUserLockedOut("alice"));
	int failures = -1;
	session << "SELECT auth_failures FROM auth_user WHERE username = 'alice'", into(failures), now;
	assertEqual (1, failures);
}


void AuthAdminServiceTest::testEntryWithSeveralAccountNamesIsDirectoryError()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	Directory directory;
	addServiceAccountAndAlice(directory);
	directory.passwords["company\\alice"] = "A";
	directory.fullName = "Alice Example";
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	auto serviceParams = accountModeParams(true);
	serviceParams.localFallback = false;
	auto userParams = accountModeParams(false);
	userParams.localFallback = false;
	LDAPAuthAdminService::Ptr pService = new LDAPAuthAdminService(*_pProperties, authParams, serviceParams, directory);
	LDAPAuthAdminService::Ptr pUser = new LDAPAuthAdminService(*_pProperties, authParams, userParams, directory);
	assertTrue (pService->authenticate("alice", "A"));
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.severalAccountNames = true;
		directory.fullName = "Changed";
	}

	for (const auto& pAAS: {pService, pUser})
	{
		for (const std::string password: {"A", "wrong"})
		{
			std::string accountName;
			assertFalse (pAAS->authenticateAccount("COMPANY\\alice", password, accountName));
			assertTrue (accountName.empty());
		}
	}
	assertFalse (pService->userExists("company\\alice"));
	assertEqual ("Alice Example", pService->getUserAttribute("alice", AuthAdminService::USER_ATTR_FULL_NAME));
	int failures = -1;
	session << "SELECT auth_failures FROM auth_user WHERE username = 'alice'", into(failures), now;
	assertEqual (0, failures);
}


void AuthAdminServiceTest::testRejectedLDAPPasswordKeepsUserAndClearsLocalPassword()
{
	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	directory.fullName = "Alice Example";
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), ldapTestParams(),
		directory);
	assertTrue (pAAS->authenticate("alice", "A"));
	pAAS->setUserAttribute("alice", "mail", "alice@example.com");
	pAAS->setAuthAttribute("alice", "auth_tfa_secret", "secret");

	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.passwords[userDN("alice")] = "B";
	}
	pAAS->uncacheUser("alice");
	assertFalse (pAAS->authenticate("alice", "A"));

	assertTrue (pAAS->userExists("alice"));
	assertEqual ("Alice Example", pAAS->getUserAttribute("alice", AuthAdminService::USER_ATTR_FULL_NAME));
	assertEqual ("alice@example.com", pAAS->getUserAttribute("alice", "mail"));
	assertEqual ("secret", pAAS->getUserAttribute("alice", "auth_tfa_secret"));

	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.reachable = false;
	}
	assertFalse (pAAS->authenticate("alice", "A"));
}


void AuthAdminServiceTest::testRejectedLDAPLoginKeepsPasswordNotStoredByLDAP()
{
	_pAAS->addUser("bob", "local");
	Directory directory;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), ldapTestParams(),
		directory);
	assertTrue (pAAS->isExternalUser("bob"));
	const std::string storedHash = pAAS->getUserAttribute("bob", "$hash");

	assertFalse (pAAS->authenticate("bob", "local"));
	assertEqual (1, directory.binds);
	assertFalse (storedHash.empty());
	assertEqual (storedHash, pAAS->getUserAttribute("bob", "$hash"));
}


void AuthAdminServiceTest::testRejectedLDAPPasswordsLockOutUser()
{
	Poco::Data::Session session("SQLite", "auth.db");
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	auto authParams = ldapTestAuthParams();
	authParams.maxFailures = 3;
	authParams.lockOutTime = LOCK_OUT_SECONDS;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, authParams, ldapTestParams(), directory);
	assertTrue (pAAS->authenticate("alice", "A"));

	for (int i = 0; i < authParams.maxFailures; i++)
	{
		assertFalse (pAAS->isUserLockedOut("alice"));
		assertFalse (pAAS->authenticate("alice", "wrong"));
	}
	assertTrue (pAAS->isUserLockedOut("alice"));
}


void AuthAdminServiceTest::testUnreachableLDAPServerKeepsLocalPassword()
{
	Directory directory;
	directory.passwords[userDN("alice")] = "A";
	auto ldapParams = ldapTestParams();
	ldapParams.localFallback = false;
	LDAPAuthAdminService::Ptr pAAS = new LDAPAuthAdminService(*_pProperties, ldapTestAuthParams(), ldapParams,
		directory);
	assertTrue (pAAS->authenticate("alice", "A"));
	pAAS->uncacheUser("alice");

	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.reachable = false;
	}
	assertFalse (pAAS->authenticate("alice", "A"));
	assertTrue (pAAS->userExists("alice"));
	assertFalse (pAAS->getUserAttribute("alice", "$hash").empty());
}
#endif


void AuthAdminServiceTest::testRoleMappings()
{
	// Setup: create users, roles, and permissions
	_pAAS->addUser("ldapuser", "pass1");
	_pAAS->addUser("localuser", "pass2");

	_pAAS->addRole("cn=ldap_operators,dc=example,dc=com");
	_pAAS->addRole("cn=ldap_viewers,dc=example,dc=com");
	_pAAS->addRole("operators");
	_pAAS->addRole("cn=ldap_admins,dc=example,dc=com");
	_pAAS->addRole("admins");

	// Grant permissions to internal roles
	std::set<std::string> perms;
	perms.insert("app.devices.read");
	perms.insert("app.devices.write");
	perms.insert("hmi");
	_pAAS->grantPermissionsToRole("operators", perms);

	perms.clear();
	perms.insert("app.auth.admin");
	perms.insert("app.sql.admin");
	_pAAS->grantPermissionsToRole("admins", perms);

	// Grant an LDAP permission to the external role (simulates rolePermissionAttribute = cn)
	perms.clear();
	perms.insert("LDAP_Operators");
	_pAAS->grantPermissionsToRole("cn=ldap_operators,dc=example,dc=com", perms);

	// Assign LDAP role to user
	_pAAS->assignRoleToUser("ldapuser", "cn=ldap_operators,dc=example,dc=com");

	// --- User with no roles/mappings should have no permissions ---
	perms.clear();
	_pAAS->effectivePermissionsForUser("localuser", perms);
	assert (perms.size() == 0);

	// --- Without mapping: user only has the LDAP permission ---
	perms.clear();
	_pAAS->effectivePermissionsForUser("ldapuser", perms);
	assert (perms.size() == 1);
	assert (perms.find("LDAP_Operators") != perms.end());
	assert (perms.find("app.devices.read") == perms.end());

	// --- Test setRoleMapping: map external LDAP role to internal role ---
	_pAAS->setRoleMapping("cn=ldap_operators,dc=example,dc=com", "operators");

	// Verify mapping was stored
	std::string internalRole;
	_pAAS->roleMappingForExternal("cn=ldap_operators,dc=example,dc=com", internalRole);
	assert (internalRole == "operators");

	// With mapping: user gets both LDAP permission + mapped role permissions
	perms.clear();
	_pAAS->effectivePermissionsForUser("ldapuser", perms);
	assert (perms.size() == 4);
	assert (perms.find("LDAP_Operators") != perms.end());
	assert (perms.find("app.devices.read") != perms.end());
	assert (perms.find("app.devices.write") != perms.end());
	assert (perms.find("hmi") != perms.end());

	// --- Test authorize works through the mapping ---
	assert (_pAAS->authorize("ldapuser", "app.devices.read"));
	assert (_pAAS->authorize("ldapuser", "hmi"));
	assert (!_pAAS->authorize("ldapuser", "app.auth.admin"));

	// --- Test allRoleMappings ---
	std::map<std::string, std::string> mappings;
	_pAAS->allRoleMappings(mappings);
	assert (mappings.size() == 1);
	assert (mappings["cn=ldap_operators,dc=example,dc=com"] == "operators");

	// --- Test adding a second mapping ---
	_pAAS->setRoleMapping("cn=ldap_admins,dc=example,dc=com", "admins");
	_pAAS->assignRoleToUser("ldapuser", "cn=ldap_admins,dc=example,dc=com");

	perms.clear();
	_pAAS->effectivePermissionsForUser("ldapuser", perms);
	assert (perms.size() == 6); // LDAP_Operators + app.devices.read + app.devices.write + hmi + app.auth.admin + app.sql.admin
	assert (perms.find("app.auth.admin") != perms.end());
	assert (perms.find("app.sql.admin") != perms.end());

	mappings.clear();
	_pAAS->allRoleMappings(mappings);
	assert (mappings.size() == 2);

	// --- Test double-mapping to same internal role: no permission duplication ---
	_pAAS->setRoleMapping("cn=ldap_viewers,dc=example,dc=com", "operators");
	_pAAS->assignRoleToUser("ldapuser", "cn=ldap_viewers,dc=example,dc=com");

	perms.clear();
	_pAAS->effectivePermissionsForUser("ldapuser", perms);
	// operators perms (app.devices.read, app.devices.write, hmi) should appear once even though
	// two external roles map to operators. UNION in SQL deduplicates.
	assert (perms.size() == 6); // still 6, not 9
	assert (perms.find("app.devices.read") != perms.end());

	// --- Test overwrite mapping: change operators -> admins ---
	_pAAS->setRoleMapping("cn=ldap_operators,dc=example,dc=com", "admins");
	internalRole.clear();
	_pAAS->roleMappingForExternal("cn=ldap_operators,dc=example,dc=com", internalRole);
	assert (internalRole == "admins");

	// --- Test removeRoleMapping ---
	_pAAS->removeRoleMapping("cn=ldap_operators,dc=example,dc=com");
	internalRole.clear();
	_pAAS->roleMappingForExternal("cn=ldap_operators,dc=example,dc=com", internalRole);
	assert (internalRole.empty());

	mappings.clear();
	_pAAS->allRoleMappings(mappings);
	assert (mappings.size() == 2); // ldap_admins + ldap_viewers remain

	// --- Test that removing the internal role cascades to remove mappings ---
	_pAAS->removeRole("admins");
	mappings.clear();
	_pAAS->allRoleMappings(mappings);
	assert (mappings.size() == 1); // only ldap_viewers -> operators remains
	assert (mappings.find("cn=ldap_viewers,dc=example,dc=com") != mappings.end());

	// --- Test that removing the external role cascades to remove the mapping ---
	_pAAS->removeRole("cn=ldap_viewers,dc=example,dc=com");
	mappings.clear();
	_pAAS->allRoleMappings(mappings);
	assert (mappings.size() == 0);

	// --- Test query for non-existent mapping returns empty ---
	internalRole = "should-be-cleared";
	_pAAS->roleMappingForExternal("nonexistent", internalRole);
	assert (internalRole.empty());
}


void AuthAdminServiceTest::testInvalidateCachesClearsPermissionsByDefault()
{
	_pAAS->addUser("user1", "pass1");
	_pAAS->grantPermissionsToUser("user1", {"perm1"});
	assertTrue (_pAAS->authorize("user1", "perm1"));

	Poco::Data::Session session("SQLite", "auth.db");
	session << "DELETE FROM auth_user_permissions WHERE username = 'user1'", now;
	assertTrue (_pAAS->authorize("user1", "perm1"));

	_pAAS->invalidateCaches();
	assertFalse (_pAAS->authorize("user1", "perm1"));
}


void AuthAdminServiceTest::testValidateUserEventIsInitialized()
{
	using Event = AuthAdminService::ValidateUserEvent;

	// Construct the event in memory that holds no zero bytes, so that an
	// uninitialized member does not read as 0 or false by chance.
	alignas(Event) std::array<unsigned char, sizeof(Event)> buffer{};
	buffer.fill(0xFF);
	Event* pEvent = new (buffer.data()) Event;
	assertEqual (0, pEvent->flags);
	assertFalse (pEvent->valid);
	pEvent->~Event();
}


void AuthAdminServiceTest::handleNotification(const Poco::AutoPtr<Poco::OSP::Auth::AuthNotification>& n)
{
	if (n != nullptr)
	{
		++_notificationCounter;
	}
}


void AuthAdminServiceTest::setUp()
{
	if (_firstRun)
	{
		Poco::Data::SQLite::Connector::registerConnector();
		_firstRun = false;
	}

	Poco::File f("auth.db");
	if (f.exists())
	{
		f.remove();
	}

	Poco::Data::Session session("SQLite", "auth.db");
	if (_testInlineAttributes)
	{
		session <<
			"CREATE TABLE auth_user"
			"("
			"    username VARCHAR(256) PRIMARY KEY, "
			"    password VARCHAR(256) NOT NULL, "
			"    attr1    VARCHAR(256), "
			"    attr2    VARCHAR(256)"
			")",
			now;
	}
	else
	{
		session <<
			"CREATE TABLE auth_user"
			"("
			"    username VARCHAR(256) PRIMARY KEY, "
			"    password VARCHAR(256) NOT NULL"
			")",
			now;
	}
	session <<
		"CREATE TABLE auth_role"
		"("
		"    rolename VARCHAR(256) PRIMARY KEY"
		")",
		now;
	session <<
		"CREATE TABLE auth_user_roles"
		"("
		"    username VARCHAR(256) NOT NULL REFERENCES auth_user(username) ON DELETE CASCADE,"
		"    rolename VARCHAR(256) NOT NULL REFERENCES auth_role(rolename) ON DELETE CASCADE"
		")",
		now;
	session <<
		"CREATE TABLE auth_user_permissions"
		"("
		"    username VARCHAR(256) NOT NULL REFERENCES auth_user(username) ON DELETE CASCADE,"
		"    permission VARCHAR(32) NOT NULL"
		")",
		now;
	session <<
		"CREATE TABLE auth_role_permissions"
		"("
		"    rolename VARCHAR(256) NOT NULL REFERENCES auth_role(rolename) ON DELETE CASCADE,"
		"    permission VARCHAR(32) NOT NULL"
		")",
		now;
	session <<
		"CREATE TABLE auth_user_attributes"
		"("
		"    username VARCHAR(256), "
		"    attribute VARCHAR(256), "
		"    value VARCHAR(256)"
		")",
		now;
	session <<
		"CREATE TABLE auth_role_attributes"
		"("
		"    rolename VARCHAR(256), "
		"    attribute VARCHAR(256), "
		"    value VARCHAR(256)"
		")",
		now;
	session <<
		"CREATE TABLE auth_role_mappings"
		"("
		"    external_role VARCHAR(256) PRIMARY KEY REFERENCES auth_role(rolename) ON DELETE CASCADE, "
		"    internal_role VARCHAR(256) NOT NULL REFERENCES auth_role(rolename) ON DELETE CASCADE"
		")",
		now;

	Poco::Path propsPath(false);
	int upCount = 0;
	do
	{
		propsPath.clear();
		upCount++;
		for (int i = 0; i < upCount; i++)
		{
			propsPath.pushDirectory("..");
		}
		propsPath.pushDirectory("bundle");
		propsPath.setFileName("bundle.properties");
	}
	while (upCount < 5 && !Poco::File(propsPath.toString()).exists());

	_pProperties = new Poco::Util::PropertyFileConfiguration(propsPath.toString());

	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.cacheSize = 100;
	authParams.cacheTimeout = 3600;
	authParams.hashVersion = 4;
	authParams.saltLength = 16;
	authParams.pbkdf2Iterations = 50000;
	authParams.credentialsSalt = "poco";

	AuthAdminServiceImpl::LDAPParams ldapParams;
	if (_testName == "testExternalRole" || _testName == "testRoleMappings")
		ldapParams.uri = "ldap://example.com";	// Simulate LDAP enabled

	_pAAS = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);
}


void AuthAdminServiceTest::tearDown()
{
	_pAAS = nullptr;
}


CppUnit::Test* AuthAdminServiceTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("AuthAdminServiceTest");

	CppUnit_addTest(pSuite, AuthAdminServiceTest, testUsers);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testManyUsers);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testUsersWithNameMatchesSpecialCharacters);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testUsersWithPermission);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testUsersWithRole);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testUsersWithAttribute);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testRoles);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testPermissions);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthenticate);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthenticateNotification);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLoginOfUserRemovedBeforeNotificationSucceeds);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testPasswordChangedBehindCacheIsAccepted);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testChangePasswordOfUnknownUserThrows);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testEmptyPasswordIsRejected);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthenticateV1);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthenticateUpgradeFromV1ToV2);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthenticateUpgradeFromV1ToV3);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthenticateUpgradeFromV2ToV3);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthenticateUpgradeFromV3ToV4);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthenticateUpgradeFromV2ToV4);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthenticateRaisesIterationCount);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testIterationsOfEmptyPasswordHashAreZero);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testRandomSaltsKeepWhitespaceBytes);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthorize);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLongCacheTimeoutKeepsCachedPermissions);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAttributes);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthAttributesAreChangedOnlyThroughAuthInterface);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAttributesForUser);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAttributeWithAuthPrefixWithoutUnderscoreIsUserAttribute);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLockOut);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLockOutAfterLoginFailures);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testCachedLoginResetsFailureCount);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testNeverLockOutUsersAreNotLockedOut);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testValidateUserCanDisableUser);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testMalformedLockOutTimeIsLoggedAndIgnored);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLockOutWithNullableFailureCount);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testMissingFailureCountColumnIsLogged);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLockOutWithShippedSchema);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testPoolRevokePermissionClearsEveryBackend);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testPoolRoutesUserNamesIgnoringCase);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testPoolAuthenticatesAccountWithBackendOfUser);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testPoolForwardsValidateUserWithSender);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testReset);
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testExternalRole);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAdminOfShippedDatabaseIsLocalUserWithLDAP);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLDAPPasswordChangeUpdatesStoredHash);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLocalUserWithCapitalLettersIsNotLDAPUser);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testOtherCallsProceedDuringLDAPLogin);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testConcurrentLDAPLoginsOfOneUser);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLDAPRoundTripsOfOneUserAreSequential);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testConcurrentLDAPAttemptsAreLimitedByFailureCount);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLDAPSuccessDoesNotOverwriteAccountCreatedMeanwhile);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLDAPRejectionKeepsPasswordOfAccountCreatedMeanwhile);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLockOutDuringLDAPLoginDiscardsLDAPAnswer);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLDAPRoundTripsOfDifferentUsersRunInParallel);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLoginWaitingTooLongForOtherLDAPLoginIsRefused);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testRejectedLDAPPasswordKeepsUserAndClearsLocalPassword);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testRejectedLDAPLoginKeepsPasswordNotStoredByLDAP);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testRejectedLDAPPasswordsLockOutUser);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testUnreachableLDAPServerKeepsLocalPassword);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLDAPDirectoryProblemKeepsLocalPassword);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLDAPDirectoryErrorWithoutLocalFallbackRefusesStoredPassword);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLDAPDirectoryErrorBeforeUserBindCountsWrongPasswords);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLDAPDirectoryErrorAfterAcceptedPasswordCountsNoFailure);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testRefusedLDAPBindCountsFailureAndKeepsPassword);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testInaccessibleLDAPServerKeepsStoredPassword);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testDatabaseErrorWhileStoringLDAPUserRollsBack);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLoginNamesOfDirectoryUserShareOneAccount);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testFailedLoginsThroughEveryNameLockOutAccount);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLockedOutAccountIsNotTriedThroughOtherName);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLockOutDuringLoginThroughOtherNameDiscardsLDAPAnswer);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testUserBindModeStoresLoginUnderAccountName);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testUserBindModeRefusesNameThatEntryDoesNotHave);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testUserBindModeAcceptsDomainNameOfEntry);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testOutageAfterSearchChecksStoredPasswordOfAccount);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLoginWithoutAccountNameIsStoredUnderTypedName);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthenticateRefusesOtherNameThanAccountName);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testVerifyCredentialsRefusesOtherNameThanAccountName);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testDatabaseErrorWhileCheckingAccountIsReported);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAccountNameOfLocalUserIsRefused);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testEntryWithSeveralAccountNamesIsDirectoryError);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLoginThroughOtherNameWaitsForLoginOfAccount);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLoginThroughOtherNameWaitingTooLongIsRefused);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testConcurrentLoginsThroughNamesOfAccountAreLimitedByFailureCount);
#endif
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testRoleMappings);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testInvalidateCachesClearsPermissionsByDefault);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testValidateUserEventIsInitialized);

	return pSuite;
}


InlineAttributesAuthAdminServiceTest::InlineAttributesAuthAdminServiceTest(const std::string& name):
	AuthAdminServiceTest(name, true)
{
}


CppUnit::Test* InlineAttributesAuthAdminServiceTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("InlineAttributesAuthAdminServiceTest");

	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testUsers);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testManyUsers);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testUsersWithNameMatchesSpecialCharacters);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testUsersWithPermission);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testUsersWithRole);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testUsersWithAttribute);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testRoles);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testPermissions);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthenticate);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthenticateNotification);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testPasswordChangedBehindCacheIsAccepted);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthenticateV1);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthenticateUpgradeFromV1ToV2);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthenticateUpgradeFromV1ToV3);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthenticateUpgradeFromV2ToV3);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthenticateUpgradeFromV3ToV4);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthenticateUpgradeFromV2ToV4);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthenticateRaisesIterationCount);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthorize);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAttributes);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthAttributesAreChangedOnlyThroughAuthInterface);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAttributesForUser);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest,
		testAttributeWithAuthPrefixWithoutUnderscoreIsUserAttribute);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testLockOut);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testLockOutAfterLoginFailures);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testCachedLoginResetsFailureCount);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testNeverLockOutUsersAreNotLockedOut);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testValidateUserCanDisableUser);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testMalformedLockOutTimeIsLoggedAndIgnored);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testReset);
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testExternalRole);
#endif

	return pSuite;
}
