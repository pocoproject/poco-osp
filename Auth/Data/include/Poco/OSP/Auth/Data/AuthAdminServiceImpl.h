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
#include <unordered_map>
#include <set>


namespace Poco {
namespace OSP {
namespace Auth {
namespace Data {


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

	~AuthAdminServiceImpl();
		/// Destroys the AuthAdminServiceImpl.

	static bool checkDatabaseConnectivity(const Poco::Util::AbstractConfiguration& properties, const AuthParams& authParams, Poco::Logger& logger);
		/// Checks whether the database connection is ok.

	// AuthAdminService
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
	bool userEnabled(const std::string& username) const override;
	void addUser(const std::string& username, const std::string& password) override;
	void removeUser(const std::string& username) override;
	void permissionsForRole(const std::string& rolename, std::set<std::string>& permissions) const override;
	void grantPermissionsToRole(const std::string& rolename, const std::set<std::string>& permissions) override;
	void replacePermissionsForRole(const std::string& rolename, const std::set<std::string>& permissions) override;
	void revokePermissionsFromRole(const std::string& rolename, const std::set<std::string>& permissions) override;
	bool roleExists(const std::string& rolename) const override;
	void roles(std::set<std::string>& roles) const override;
	void addRole(const std::string& rolename) override;
	void removeRole(const std::string& rolename) override;
	void rolesForUser(const std::string& username, std::set<std::string>& roles) const override;
	void assignRoleToUser(const std::string& username, const std::string& rolename) override;
	void removeRoleFromUser(const std::string& username, const std::string& rolename) override;
	void replaceRolesForUser(const std::string& username, const std::set<std::string>& permissions) override;
	void uncacheUser(const std::string& username) override;
	std::string getUserAttribute(const std::string& username, const std::string& attribute, const std::string& deflt = std::string()) const override;
	void setUserAttribute(const std::string& username, const std::string& attribute, const std::string& value) override;
	void setRoleAttribute(const std::string& rolename, const std::string& attribute, const std::string& value);
	void removeUserAttribute(const std::string& username, const std::string& attribute) override;
	void replaceUserAttributes(const std::string& username, const std::map<std::string, std::string>& attributes) override;
	void updateUserAttributes(const std::string& username, const std::map<std::string, std::string>& attributes) override;
	void attributesForUser(const std::string& username, std::set<std::string>& attributes) const override;
	void attributesForUser(const std::string& username, std::map<std::string, std::string>& attributes) const override;
	std::vector<std::string> findUsersByAttribute(const std::string& attribute, const std::string& value) const override;
	void revokePermission(const std::string& permission) override;
	void lockOutUser(const std::string& username, const Poco::DateTime& lockOutUntil) override;
	void unlockOutUser(const std::string& username) override;
	bool isUserLockedOut(const std::string& username) const override;
	bool isExternalUser(const std::string& username) const override;
	bool isExternalRole(const std::string& rolename) const override;
	void allRoleMappings(std::map<std::string, std::string>& mappings) const override;
	void roleMappingForExternal(const std::string& externalRole, std::string& internalRole) const override;
	void setRoleMapping(const std::string& externalRole, const std::string& internalRole) override;
	void removeRoleMapping(const std::string& externalRole) override;
	void reset() override;
	void invalidateCaches(int what = INVALIDATE_ALL) override;

	// AuthService
	bool authenticate(const std::string& username, const std::string& credentials) const override;
	bool authorize(const std::string& username, const std::string& permission) const override;
	bool authorize(const std::string& userName, const std::string& roleOrScope, const std::string& permission) const override;
	bool userExists(const std::string& username) const override;

	// Service
	const std::type_info& type() const override;
	bool isA(const std::type_info& otherType) const override;

	static const std::string USER_ATTR_FAILURES;
	static const std::string USER_ATTR_LOCKOUT;

protected:
	enum
	{
		V4_MIN_SALT_LENGTH = 8,
		V4_MIN_ITERATIONS = 50000
	};

	enum
	{
		SQL_CACHE_BUCKET_SIZE = 113 // next prime > 2*number of SQL statements
	};

	Poco::Data::Session& session() const;
	static void setupSession(Poco::Data::Session& session, const Poco::Util::AbstractConfiguration& properties, const AuthParams& authParams, Poco::Logger& logger);
	bool sessionExpired() const;
	bool authenticateDB(Poco::Data::Session& session, const std::string& username, const std::string& credentials) const;
	void changePasswordImpl(Poco::Data::Session& session, const std::string& username, const std::string& hashedCredentials) const;
	bool userExistsImpl(Poco::Data::Session& session, const std::string& username) const;
	bool roleExistsImpl(Poco::Data::Session& session, const std::string& rolename) const;
	void addUserImpl(Poco::Data::Session& session, const std::string& username, const std::string& password) const;
	void removeUserImpl(Poco::Data::Session& session, const std::string& username) const;
	void uncacheUserImpl(const std::string& username) const;
	void createOrUpdateRoleImpl(Poco::Data::Session& session, const std::string& rolename) const;
	void createOrUpdateUserImpl(Poco::Data::Session& session, const std::string& username, const std::string& credentials) const;
	void replaceUserAttributeImpl(Poco::Data::Session& session, const std::string& username, const std::string& attribute, const std::string& value) const;
	void replaceRoleAttributeImpl(Poco::Data::Session& session, const std::string& rolename, const std::string& attribute, const std::string& value) const;
	void replacePermissionsForUserImpl(Poco::Data::Session& session, const std::string& username, const std::set<std::string>& permissions) const;
	void replaceRolesForUserImpl(Poco::Data::Session& session, const std::string& username, const std::set<std::string>& permissions) const;
	void replacePermissionsForRoleImpl(Poco::Data::Session& sess, const std::string& rolename, const std::set<std::string>& permissions) const;
	void discoverInlineAttributes();
	bool authorizeRole(const std::string& rolename, const std::string& permission) const;

	// AbstractLDAPAuthAdminService
	void createOrUpdateRoleImpl(const std::string& rolename) const override;
	void createOrUpdateUserImpl(const std::string& username, const std::string& credentials) const override;
	void replaceUserAttributeImpl(const std::string& username, const std::string& attribute, const std::string& value) const override;
	void replaceRoleAttributeImpl(const std::string& rolename, const std::string& attribute, const std::string& value) const override;
	void replacePermissionsForUserImpl(const std::string& username, const std::set<std::string>& permissions) const override;
	void replaceRolesForUserImpl(const std::string& username, const std::set<std::string>& permissions) const override;
	void replacePermissionsForRoleImpl(const std::string& rolename, const std::set<std::string>& permissions) const override;
	std::string getUserAttributeImpl(const std::string& username, const std::string& attribute, const std::string& deflt = ""s) const override;
	std::string getRoleAttributeImpl(const std::string& rolename, const std::string& attribute, const std::string& deflt = ""s) const override;

private:
	using Permissions = std::set<std::string>;
	using PermissionsPtr = Poco::SharedPtr<Permissions>;
	using PermissionsCache = Poco::ExpireLRUCache<std::string, Permissions>;
	using PasswordCache = Poco::ExpireCache<std::string, std::string>;
	using SQLCache = std::unordered_map<std::string, std::string>;

	bool authenticateImpl(const std::string& username, const std::string& credentials) const;
	bool validateUserImpl(const std::string& username, bool valid) const;
	PermissionsPtr loadPermissions(Poco::Data::Session& sess, const std::string& username) const;
	void reloadPermissions(Poco::Data::Session& sess, const std::string& username) const;
	bool isInlineAttribute(const std::string& attribute) const;
	bool isAuthAttribute(const std::string& attribute) const;
	std::string getUserAttributeImpl(Poco::Data::Session& sess, const std::string& username, const std::string& attribute, const std::string& deflt) const;
	std::string getRoleAttributeImpl(Poco::Data::Session& sess, const std::string& rolename, const std::string& attribute, const std::string& deflt) const;
	void removeUserAttributeImpl(Poco::Data::Session& sess, const std::string& username, const std::string& attribute) const;
	void lockOutUserIfTooManyFailures(Poco::Data::Session& sess, const std::string& username) const;
	void lockOutUserImpl(Poco::Data::Session& sess, const std::string& username, const Poco::DateTime& lockOutUntil) const;
	void unlockOutUserImpl(Poco::Data::Session& sess, const std::string& username) const;
	bool isUserLockedOutImpl(Poco::Data::Session& sess, const std::string& username) const;
	bool isLDAPUserImpl(Poco::Data::Session& sess, const std::string& username, std::string& ldapAttr) const;
	bool isLDAPRoleImpl(Poco::Data::Session& sess, const std::string& rolename, std::string& ldapAttr) const;
	std::string hashCredentials(Poco::Data::Session& sess, const std::string& username, const std::string& credentials) const;
	std::string saltAndHashCredentials(const std::string& username, const std::string& credentials) const;
	std::string sqlFor(const std::string& action) const;
	std::string fixPlaceholders(const std::string& clause) const;
	StatementPlaceholderProvider::Ptr statementPlaceholderProvider() const;
	std::string hashForUser(Poco::Data::Session& sess, const std::string& username) const;
	std::string saltForUser(Poco::Data::Session& sess, const std::string& username, int& version, int& iterations) const;
	static std::string md5(const std::string& salt, const std::string& credentials);
	static std::string sha1(const std::string& salt, const std::string& credentials);
	static std::string pbkdf2(const std::string& salt, const std::string& credentials, int version, int iterations);
	static std::map<std::string, std::string> parseKeyValueMap(const std::string& s);
	static std::string translatePattern(const std::string& pattern);

	const Poco::Util::AbstractConfiguration& _properties;
	mutable Poco::SharedPtr<Poco::Data::Session> _pSession;
	mutable Poco::Timestamp _sessionLastUsed;
	const AuthParams _authParams;
	Poco::SharedPtr<Poco::RegularExpression> _pLocalUsersExpr;
	Poco::SharedPtr<Poco::RegularExpression> _pLocalRolesExpr;
	std::set<std::string> _inlineAttributes;
	mutable SQLCache _sqlCache;
	mutable PermissionsCache _permissionsCache;
	mutable PasswordCache _passwordCache;
	mutable PasswordCache _passwordCacheLDAP;
	mutable Poco::FastMutex _mutex;
};


} } } } // namespace Poco::OSP::Auth::Data


#endif // OSP_Auth_Data_AuthAdminServiceImpl_INCLUDED
