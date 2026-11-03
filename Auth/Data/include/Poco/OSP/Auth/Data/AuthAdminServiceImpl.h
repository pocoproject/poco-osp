//
// AuthAdminServiceImpl.h
//
// Library: OSP/Auth/Data
// Package: Auth
// Module:  AuthAdminServiceImpl
//
// Definition of the AuthAdminService service interface class.
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_Data_AuthAdminServiceImpl_INCLUDED
#define OSP_Auth_Data_AuthAdminServiceImpl_INCLUDED


#include "Poco/OSP/Auth/Data/Data.h"
#include "Poco/OSP/Auth/Data/StatementPlaceholderProvider.h"
#include "Poco/OSP/Auth/AbstractLDAPAuthAdminService.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/Data/Session.h"
#include "Poco/Timestamp.h"
#include "Poco/ExpireLRUCache.h"
#include "Poco/ExpireCache.h"
#include "Poco/RegularExpression.h"
#include "Poco/SharedPtr.h"
#include "Poco/Mutex.h"
#include "Poco/Condition.h"
#include <chrono>
#include <map>
#include <optional>
#include <unordered_map>
#include <set>


namespace Poco::OSP::Auth::Data {


class OSPAuthData_API AuthAdminServiceImpl: public AbstractLDAPAuthAdminService
	/// This class implements the AuthAdminService using a SQL database accessed
	/// via the POCO Data library, and optionally LDAP for password verification
	/// and permissions.
	///
	/// Upon successful authentication of a user, the class will cache the permissions
	/// for the user so that further permission checks are very quick.
	///
	/// This implementation supports multiple variants/versions of storing
	/// password hashes.
	///
	///   - Version 1 is the original mechanism, using MD5 with a global salt.
	///     This is no longer considered secure and should no longer be used.
	///   - Version 2 uses PBKDF2 with HMAC-SHA1 and per-user random salt.
	///   - Version 3 uses PBKDF2 with HMAC-SHA1 and per-user random salt with additional
	///     MD5 password pre-hashing, allowing implementation of secure challenge-response
	///     authentication mechanisms such as SCRAM-SHA1.
	///   - Version 4 uses PBKDF2 with HMAC-SHA256, SHA1 pre-hasing and a minimum of
	///     50.000 iterations, and a minimum salt length of 8 bytes.
	///
	/// Version 4 is recommended for new deployments. Stored passwords using a lower
	/// version than the one configured will automatically be upgraded as soon as
	/// a user successfully authenticates.
	/// A stored version 3 or 4 password hash with fewer PBKDF2 iterations than
	/// configured is replaced in the same way.
	///
	/// The login of a user without a stored password hash, such as an unknown
	/// user, computes a hash of the configured version, so that the time of
	/// the login does not show whether the user exists or has a password.
	///
	/// As of 2021, the recommended number of iterations for PBKDF2 with HMAC-SHA256
	/// is 310.000 for HMAC-SHA256 (version 4) and 720.000 for HMAC-SHA1 (version 3).
	/// See https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html
	/// for more information.
	///
	/// Versions 3 and 4 pass a hash of the password rather than the actual password to the
	/// PBKDF2 hashing function. This is to guard against certain timing attacks that would
	/// allow an attacker to get hints regarding the password length, and also to avoid
	/// excessive CPU usage in case of very long passwords.
	///
	/// Note that the getUserAttribute() method of this implementation supports the
	/// following special attributes:
	///
	///   - $salt: Returns the salt used for hashing the given user's password.
	///     For Version 1, this will be the configured global salt string.
	///     For Version 2, this will be a string consisting entirely of hexadecimal digits.
	///     For Versions 3 and 4, this will be a string containing binary data.
	///   - $iterations: Returns the number of PBKDF2 iterations used for
	///     hashing the given user's password.
	///   - $hash: Returns the password hash stored for the given user.
	///     For Versions 1 and 2, this will be a string consisting entirely of hexadecimal digits.
	///     For Versions 3 and 4, this will be a string containing binary data.
	///   - $version: Returns the version of the hash used for the given user.
{
public:
	using Ptr = Poco::AutoPtr<AuthAdminServiceImpl>;

	struct AuthParams
	{
		std::string dbConnector;
			/// The name of the database connector for the auth database.
		std::string dbConnectionString;
			/// The connection string for connecting to the auth database.
		int dbSessionTimeout = 0;
			/// The database session timeout. If the session has not been used
			/// for the given time (in seconds), it will be discarded and a new
			/// session created. Note: this is different from a connection
			/// timeout specified in the connection string. A timeout of 0
			/// disables the timeout mechanism.
		int cacheSize = 1024;
			/// Size of the permissions cache.
		int cacheTimeout = 60;
			/// The timeout for the credentials cache, in seconds.
		int hashVersion = 4;
			/// The version of the password has to use:
			///    - 1: MD5 with common salt (original, no longer considered secure)
			///    - 2: PBKDF2 with HMAC-SHA1 and per-user random salt
			///    - 3: PBKDF2 with HMAC-SHA1 and per-user random salt with additional
			///         MD5 password pre-hashing, allowing implementation of secure challenge-response
			///         authentication mechanisms such as SCRAM-SHA1 (recommended).
			///    - 4: Like version 3, but using PBKDF2 with HMAC-SHA256, SHA1 pre-hasing, a minimum
			///         of 50.000 iterations and minimum salt length of 8 bytes.
		int saltLength = 7;
			/// The length of the random per-user salt in bytes.
		int pbkdf2Iterations = 1024;
			/// The number of iterations in the PBKDF2 algorithm.
		std::string credentialsSalt;
			/// Salt for version 3 initial MD5 credentials hash (username:salt:password).
		int maxFailures = 0;
			/// Maximum login failures before lockout; set to 0 to disable
			/// automatic lockout on too many login failures.
		int lockOutTime = 0;
			/// Account lockout time in seconds after maxFailures failed login attempts.
			/// Set to 0 to disable lockout after too many login failures.
		std::set<std::string> neverLockOutUsers;
			/// A set of usernames that should never be locked out after
			/// too many login failures.
			/// This typically includes the main administrator account,
			/// in order to prevent the administrator from being locked
			/// out through a DoS attack.
	};

	AuthAdminServiceImpl(const Poco::Util::AbstractConfiguration& properties, Poco::Logger& logger, const AuthParams& authParams, const LDAPParams& ldapParams);
		/// Creates the AuthAdminServiceImpl using the given authentication and LDAP parameters.
		///
		/// Note: if ldapParams.uri is empty, LDAP authentication will be disabled.
		///
		/// See the Poco::Data::Session class for more information on connector names and
		/// connection strings.

	~AuthAdminServiceImpl() override;
		/// Destroys the AuthAdminServiceImpl.

	[[nodiscard]] static bool checkDatabaseConnectivity(const Poco::Util::AbstractConfiguration& properties,
		const AuthParams& authParams, Poco::Logger& logger);
		/// Checks whether the database connection is ok.

	// AuthAdminService
	[[nodiscard]] bool verifyCredentials(const std::string& username, const std::string& credentials) override;
		/// Returns true only if the account name of the login equals username
		/// ignoring case, as authenticate() does.
	void changePassword(const std::string& username, const std::string& password) override;
	void permissionsForUser(const std::string& username, std::set<std::string>& permissions) const override;
	void effectivePermissionsForUser(const std::string& username, std::set<std::string>& permissions) const override;
	void grantPermissionsToUser(const std::string& username, const std::set<std::string>& permissions) override;
	void replacePermissionsForUser(const std::string& username, const std::set<std::string>& permissions) override;
	void revokePermissionsFromUser(const std::string& username, const std::set<std::string>& permissions) override;
	int users(std::set<std::string>& users, int first = 0, int limit = 0) const override;
	int usersWithName(std::set<std::string>& users, const std::string& pattern, int first = 0, int limit = 0) const override;
	int usersWithPermission(std::set<std::string>& users, const std::string& permission, int first = 0, int limit = 0) const override;
	int usersWithRole(std::set<std::string>& users, const std::string& role, int first = 0, int limit = 0) const override;
	int usersWithAttribute(std::set<std::string>& users, const std::string& attribute, const std::string& value, int first = 0, int limit = 0) const override;
	[[nodiscard]] bool userEnabled(const std::string& username) const override;
	void addUser(const std::string& username, const std::string& password) override;
	void removeUser(const std::string& username) override;
	void permissionsForRole(const std::string& rolename, std::set<std::string>& permissions) const override;
	void grantPermissionsToRole(const std::string& rolename, const std::set<std::string>& permissions) override;
	void replacePermissionsForRole(const std::string& rolename, const std::set<std::string>& permissions) override;
	void revokePermissionsFromRole(const std::string& rolename, const std::set<std::string>& permissions) override;
	[[nodiscard]] bool roleExists(const std::string& rolename) const override;
	void roles(std::set<std::string>& roles) const override;
	void addRole(const std::string& rolename) override;
	void removeRole(const std::string& rolename) override;
	void rolesForUser(const std::string& username, std::set<std::string>& roles) const override;
	void assignRoleToUser(const std::string& username, const std::string& rolename) override;
	void removeRoleFromUser(const std::string& username, const std::string& rolename) override;
	void replaceRolesForUser(const std::string& username, const std::set<std::string>& permissions) override;
	void uncacheUser(const std::string& username) override;
	[[nodiscard]] std::string getUserAttribute(const std::string& username, const std::string& attribute,
		const std::string& deflt = std::string()) const override;
	void setUserAttribute(const std::string& username, const std::string& attribute, const std::string& value) override;
	void setRoleAttribute(const std::string& rolename, const std::string& attribute, const std::string& value);
	void removeUserAttribute(const std::string& username, const std::string& attribute) override;
	void setAuthAttribute(const std::string& username, const std::string& attribute, const std::string& value) override;
	void removeAuthAttribute(const std::string& username, const std::string& attribute) override;
	void replaceUserAttributes(const std::string& username, const std::map<std::string, std::string>& attributes) override;
	void updateUserAttributes(const std::string& username, const std::map<std::string, std::string>& attributes) override;
	void attributesForUser(const std::string& username, std::set<std::string>& attributes) const override;
	int attributesForUser(const std::string& username, std::map<std::string, std::string>& attributes) const override;
	[[nodiscard]] std::vector<std::string> findUsersByAttribute(const std::string& attribute,
		const std::string& value) const override;
	void revokePermission(const std::string& permission) override;
	void lockOutUser(const std::string& username, const Poco::DateTime& lockOutUntil) override;
	void unlockOutUser(const std::string& username) override;
	[[nodiscard]] bool isUserLockedOut(const std::string& username) const override;
	[[nodiscard]] bool isExternalUser(const std::string& username) const override;
	[[nodiscard]] bool isExternalRole(const std::string& rolename) const override;
	void allRoleMappings(std::map<std::string, std::string>& mappings) const override;
	void roleMappingForExternal(const std::string& externalRole, std::string& internalRole) const override;
	void setRoleMapping(const std::string& externalRole, const std::string& internalRole) override;
	void removeRoleMapping(const std::string& externalRole) override;
	void reset() override;
	void invalidateCaches(int what = INVALIDATE_ALL) override;

	// AuthService
	[[nodiscard]] bool authenticate(const std::string& username, const std::string& credentials) const override;
		/// Returns true only if the account name that authenticateAccount()
		/// returns for the login equals username ignoring case. A login with
		/// another name of an LDAP user is accepted as by authenticateAccount(),
		/// with a warning in the log, but returns false.
		///
		/// Throws a Poco::Data::DataException if the auth database cannot be used.
	[[nodiscard]] bool authenticateAccount(const std::string& username, const std::string& credentials,
		std::string& accountName) const override;
		/// The account name of an LDAP user is the sAMAccountName of the
		/// user's entry in the ACCOUNT mode, in lower case, if the LDAP server
		/// has given it; otherwise it is username, in lower case for an LDAP
		/// user. The local account, the failure count, the lock-out and the
		/// attributes of the user are kept under the account name.
		///
		/// Throws a Poco::Data::DataException if the auth database cannot be used.
	[[nodiscard]] bool authorize(const std::string& username, const std::string& permission) const override;
	[[nodiscard]] bool authorize(const std::string& userName, const std::string& roleOrScope,
		const std::string& permission) const override;
	[[nodiscard]] bool userExists(const std::string& username) const override;

	// Service
	[[nodiscard]] const std::type_info& type() const override;
	[[nodiscard]] bool isA(const std::type_info& otherType) const override;

	static const std::string USER_ATTR_FAILURES;
	static const std::string USER_ATTR_LOCKOUT;

protected:
	static constexpr int V4_MIN_SALT_LENGTH = 8;
	static constexpr int V4_MIN_ITERATIONS = 50000;

	static constexpr std::size_t SQL_CACHE_BUCKET_SIZE = 113; // next prime > 2*number of SQL statements

	[[nodiscard]] int loginsWaitingForLDAP() const;
		/// Returns the number of logins that wait for the LDAP round trip
		/// of another login of the same user, for example for a test that
		/// must know that a login waits.

	[[nodiscard]] Poco::Data::Session& session() const;
	static void setupSession(Poco::Data::Session& session, const Poco::Util::AbstractConfiguration& properties, const AuthParams& authParams, Poco::Logger& logger);
	[[nodiscard]] bool sessionExpired() const;
	[[nodiscard]] bool authenticateDB(Poco::Data::Session& session, const std::string& username,
		const std::string& credentials, bool isLogin) const;
		/// Returns true if the credentials match the stored password hash of
		/// the user. If isLogin is true and the user has no stored password
		/// hash, the credentials are hashed all the same (see the class
		/// documentation).
	void changePasswordImpl(Poco::Data::Session& session, const std::string& username, const std::string& hashedCredentials) const;
	[[nodiscard]] bool userExistsImpl(Poco::Data::Session& session, const std::string& username) const;
	[[nodiscard]] bool roleExistsImpl(Poco::Data::Session& session, const std::string& rolename) const;
	void addUserImpl(Poco::Data::Session& session, const std::string& username, const std::string& password) const;
	void removeUserImpl(Poco::Data::Session& session, const std::string& username) const;
	void uncacheUserImpl(const std::string& username) const;
	[[nodiscard]] int attributesForUserImpl(Poco::Data::Session& sess, const std::string& username,
		std::map<std::string, std::string>& attributes) const;
	void createOrUpdateRoleImpl(Poco::Data::Session& session, const std::string& rolename) const;
	void createOrUpdateUserImpl(Poco::Data::Session& session, const std::string& username, const std::string& credentials) const;
	void replaceUserAttributeImpl(Poco::Data::Session& session, const std::string& username, const std::string& attribute, const std::string& value) const;
	void replaceRoleAttributeImpl(Poco::Data::Session& session, const std::string& rolename, const std::string& attribute, const std::string& value) const;
	void replacePermissionsForUserImpl(Poco::Data::Session& session, const std::string& username, const std::set<std::string>& permissions) const;
	void replaceRolesForUserImpl(Poco::Data::Session& session, const std::string& username, const std::set<std::string>& permissions) const;
	void replacePermissionsForRoleImpl(Poco::Data::Session& sess, const std::string& rolename, const std::set<std::string>& permissions) const;
	void discoverInlineAttributes();
	[[nodiscard]] bool authorizeRole(const std::string& rolename, const std::string& permission) const;

	// AbstractLDAPAuthAdminService
	[[nodiscard]] bool acceptLDAPAccount(const std::string& userName, const std::string& accountName) const override;
		/// Returns false if the account is in its lock-out period.
		///
		/// If accountName differs from userName, the login waits while another
		/// login has reserved the account name, as it waits for the logins of
		/// its own name, and then reserves the account name until it ends. So
		/// concurrent logins through several names of a user get no more
		/// password guesses than the failure count allows. A login that still
		/// waits at the end of its wait time (see ldapLoginWaitTime()) is
		/// refused, and the refusal is not counted as a failed login.
		/// In the USER bind mode, the account name is known only after the bind
		/// of the user, so only the logins of one name wait for each other
		/// before the bind; the lock-out of the LDAP server limits the guesses
		/// through other names.
	void createOrUpdateRoleImpl(const std::string& rolename) const override;
	void createOrUpdateUserImpl(const std::string& username, const std::string& credentials) const override;
	void replaceUserAttributeImpl(const std::string& username, const std::string& attribute, const std::string& value) const override;
	void replaceRoleAttributeImpl(const std::string& rolename, const std::string& attribute, const std::string& value) const override;
	void replacePermissionsForUserImpl(const std::string& username, const std::set<std::string>& permissions) const override;
	void replaceRolesForUserImpl(const std::string& username, const std::set<std::string>& permissions) const override;
	void replacePermissionsForRoleImpl(const std::string& rolename, const std::set<std::string>& permissions) const override;
	[[nodiscard]] std::string getUserAttributeImpl(const std::string& username, const std::string& attribute,
		const std::string& deflt = std::string()) const override;
	[[nodiscard]] std::string getRoleAttributeImpl(const std::string& rolename, const std::string& attribute,
		const std::string& deflt = std::string()) const override;

private:
	struct AccountState
	{
		bool exists = false;
		bool ldapUser = false;
		std::string ldapAttr;
	};

	struct LDAPLogin
		/// A login whose LDAP round trip is in progress.
	{
		std::string userName;
			/// The name of the login.
		std::chrono::steady_clock::time_point deadline;
			/// The end of the time that the login waits for other logins.
	};

	class LDAPLoginGuard
		/// Ends the LDAP round trip of a user: removes the names that the
		/// login has reserved and wakes the logins that wait for them.
	{
	public:
		POCO_NODISCARD_CTOR LDAPLoginGuard(const AuthAdminServiceImpl& service, const std::string& username);
		~LDAPLoginGuard();

		LDAPLoginGuard() = delete;
		LDAPLoginGuard(const LDAPLoginGuard&) = delete;
		LDAPLoginGuard(LDAPLoginGuard&&) = delete;
		LDAPLoginGuard& operator = (const LDAPLoginGuard&) = delete;
		LDAPLoginGuard& operator = (LDAPLoginGuard&&) = delete;

	private:
		const AuthAdminServiceImpl& _service;
		const std::string _username;
	};

	static constexpr std::chrono::seconds LDAP_LOGIN_WAIT_MARGIN{5};

	using Permissions = std::set<std::string>;
	using PermissionsPtr = Poco::SharedPtr<Permissions>;
	using PermissionsCache = Poco::ExpireLRUCache<std::string, Permissions>;
	using PasswordCache = Poco::ExpireCache<std::string, std::string>;
	using SQLCache = std::unordered_map<std::string, std::string>;

	[[nodiscard]] bool authenticateImpl(std::string& username, const std::string& credentials, bool validate,
		bool& isLDAPUser) const;
		/// Verifies the credentials and, if validate is true, fires validateUser
		/// for an authenticated user. Holds the lock of the service except
		/// during the conversation with the LDAP server.
		///
		/// On return, username holds the name the user is stored under,
		/// which is the account name in lower case for an LDAP user.
	void postLoginNotification(const std::string& username, bool isLDAPUser) const;
		/// Posts the AuthNotification of a successful login of the user
		/// stored under username, if the notification center has observers.
	[[nodiscard]] std::optional<bool> authenticateWithoutLDAPImpl(Poco::Data::Session& sess,
		const std::string& username, const std::string& credentials, bool isLDAPUser) const;
		/// Returns the result of the authentication if it is decided without
		/// the LDAP server: for a locked-out user, a cached password or a
		/// local user. Returns no value for an LDAP user whose credentials
		/// the LDAP server must verify.
	[[nodiscard]] bool ldapUserNameImpl(Poco::Data::Session& sess, std::string& username, std::string& ldapAttr) const;
		/// Returns true for an LDAP user, whose name is then changed to lower case,
		/// the name the user is stored under.
	[[nodiscard]] AccountState accountStateImpl(Poco::Data::Session& sess, std::string& username) const;
		/// Returns the state of the account that decides how the user is
		/// authenticated. On return, username holds the name the user is
		/// stored under, which is lower case for an LDAP user.
	[[nodiscard]] std::chrono::milliseconds ldapLoginWaitTime() const;
		/// Returns how long a login waits for the LDAP round trip of
		/// another login of the same user.
	[[nodiscard]] bool waitForLDAPLoginImpl(const std::string& username,
		std::chrono::steady_clock::time_point deadline) const;
		/// Waits, with the lock of the service held, until a login that has
		/// reserved username ends or the deadline passes. Returns false, and
		/// logs that the login is refused, if the deadline has passed.
	[[nodiscard]] bool applyLDAPResultImpl(Poco::Data::Session& sess, const std::string& username,
		const std::string& credentials, std::optional<LDAPResult> ldapResult, const LDAPUser& ldapUser) const;
		/// Stores the result of verifyLDAP(), other than LDAPResult::LOCKED_OUT,
		/// and returns whether the user is authenticated. No result means a
		/// directory error: the stored credentials decide only where
		/// canUseLocalCredentials() allows it, and a mismatch counts as a
		/// failure unless the LDAP server had accepted the password
		/// (LDAPUser::passwordVerified) before the directory error.
		///
		/// Throws a Poco::Data::DataException if the user that the LDAP
		/// server has accepted cannot be stored in the database.
	[[nodiscard]] bool authenticateStoredImpl(Poco::Data::Session& sess, const std::string& username,
		const std::string& credentials) const;
		/// Verifies the credentials against the stored password hash and
		/// updates the caches and the failure count.
	void acceptStoredCredentialsImpl(Poco::Data::Session& sess, const std::string& username,
		const std::string& credentials) const;
		/// Caches the credentials and the permissions of a user whose
		/// credentials match the stored password hash, and clears the
		/// lock-out and the failure count.
	[[nodiscard]] bool validateUserImpl(Poco::Data::Session& sess, const std::string& username, bool valid) const;
	PermissionsPtr loadPermissions(Poco::Data::Session& sess, const std::string& username) const;
	void reloadPermissions(Poco::Data::Session& sess, const std::string& username) const;
	[[nodiscard]] bool isInlineAttribute(const std::string& attribute) const;
	[[nodiscard]] bool isAuthAttribute(const std::string& attribute) const;
	[[nodiscard]] bool isAuthAttributeName(const std::string& attribute) const;
		/// Returns true if the attribute name starts with the prefix auth_ in
		/// lower case, as the names of authentication attributes are written.
		/// isAuthAttribute() matches the prefix in any case, as the SQL
		/// statements do, so that such names are not used for user attributes.
	[[nodiscard]] std::string getUserAttributeImpl(Poco::Data::Session& sess, const std::string& username,
		const std::string& attribute, const std::string& deflt) const;
	[[nodiscard]] std::string getRoleAttributeImpl(Poco::Data::Session& sess, const std::string& rolename,
		const std::string& attribute, const std::string& deflt) const;
	void removeUserAttributeImpl(Poco::Data::Session& sess, const std::string& username, const std::string& attribute) const;
	void storeUserAttribute(const std::string& username, const std::string& attribute, const std::string& value);
	void deleteUserAttribute(const std::string& username, const std::string& attribute);
	void lockOutUserIfTooManyFailures(Poco::Data::Session& sess, const std::string& username) const;
	void resetFailureCountImpl(Poco::Data::Session& sess, const std::string& username) const;
	void lockOutUserImpl(Poco::Data::Session& sess, const std::string& username, const Poco::DateTime& lockOutUntil) const;
	void unlockOutUserImpl(Poco::Data::Session& sess, const std::string& username) const;
	[[nodiscard]] bool isUserLockedOutImpl(Poco::Data::Session& sess, const std::string& username) const;
	[[nodiscard]] bool isLockedOutLoginImpl(Poco::Data::Session& sess, const std::string& accountName,
		const std::string& username) const;
		/// Returns true if the account of a login with username is in its
		/// lock-out period, and logs the login attempt then.
	[[nodiscard]] bool isInLockOutPeriod(const std::string& username, const std::string& lockout) const;
	[[nodiscard]] bool isLDAPUserImpl(Poco::Data::Session& sess, const std::string& username,
		std::string& ldapAttr) const;
	[[nodiscard]] bool isLDAPRoleImpl(Poco::Data::Session& sess, const std::string& rolename,
		std::string& ldapAttr) const;
	[[nodiscard]] std::string hashPersistentCredentials(Poco::Data::Session& sess, const std::string& username,
		const std::string& credentials) const;
	std::string saltAndHashCredentials(const std::string& username, const std::string& credentials) const;
	[[nodiscard]] int pbkdf2Iterations() const;
		/// Returns the number of PBKDF2 iterations for a new hash of the configured version.
	[[nodiscard]] std::string hashCachedCredentials(const std::string& username, const std::string& credentials) const;
	[[nodiscard]] std::string sqlFor(const std::string& action) const;
	[[nodiscard]] std::string fixPlaceholders(const std::string& clause) const;
	[[nodiscard]] StatementPlaceholderProvider::Ptr statementPlaceholderProvider() const;
	[[nodiscard]] std::string hashForUser(Poco::Data::Session& sess, const std::string& username) const;
	std::string saltForUser(Poco::Data::Session& sess, const std::string& username, int& version, int& iterations) const;
	[[nodiscard]] static std::string md5(const std::string& salt, const std::string& credentials);
	[[nodiscard]] static std::string sha1(const std::string& salt, const std::string& credentials);
	[[nodiscard]] static std::string pbkdf2(const std::string& salt, const std::string& credentials, int version,
		int iterations);
	[[nodiscard]] static std::map<std::string, std::string> parseKeyValueMap(const std::string& s);
	[[nodiscard]] static std::string translatePattern(const std::string& pattern);
	[[nodiscard]] static std::string generateCacheSalt();

	const Poco::Util::AbstractConfiguration& _properties;
	mutable Poco::SharedPtr<Poco::Data::Session> _pSession;
	mutable Poco::Timestamp _sessionLastUsed;
	const AuthParams _authParams;
	Poco::SharedPtr<Poco::RegularExpression> _pLocalUsersExpr;
	std::set<std::string> _inlineAttributes;
	mutable SQLCache _sqlCache;
	mutable PermissionsCache _permissionsCache;
	mutable PasswordCache _passwordCache;
	mutable PasswordCache _passwordPersistentHashCache;
	std::string _cacheSalt;
	mutable Poco::FastMutex _mutex;
	mutable std::map<std::string, LDAPLogin> _ldapLogins;
		/// The names reserved by the logins whose LDAP round trip is in
		/// progress: the name of each login, and the account name that
		/// acceptLDAPAccount() has reserved for it.
	mutable Poco::Condition _ldapLoginEnded;
	mutable int _ldapLoginsWaiting = 0;
};


} // namespace Poco::OSP::Auth::Data


#endif // OSP_Auth_Data_AuthAdminServiceImpl_INCLUDED
