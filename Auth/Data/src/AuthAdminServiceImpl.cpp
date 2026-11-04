//
// AuthAdminServiceImpl.cpp
//
// Library: OSP/Auth/Data
// Package: Auth
// Module:  AuthAdminServiceImpl
//
// Copyright (c) 2012-2025, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Auth/Data/AuthAdminServiceImpl.h"
#include "Poco/OSP/Auth/AuthNotification.h"
#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/Data/DataException.h"
#include "Poco/Data/Extraction.h"
#include "Poco/Data/Statement.h"
#include "Poco/Data/Transaction.h"
#include "Poco/Data/RecordSet.h"
#include "Poco/StringTokenizer.h"
#include "Poco/Format.h"
#include "Poco/DigestEngine.h"
#include "Poco/MD5Engine.h"
#include "Poco/SHA1Engine.h"
#include "Poco/SHA2Engine.h"
#include "Poco/HMACEngine.h"
#include "Poco/PBKDF2Engine.h"
#include "Poco/RandomStream.h"
#include "Poco/Base64Encoder.h"
#include "Poco/Base64Decoder.h"
#include "Poco/MemoryStream.h"
#include "Poco/NumberFormatter.h"
#include "Poco/NumberParser.h"
#include "Poco/NotificationCenter.h"
#include "Poco/StreamCopier.h"
#include "Poco/String.h"
#include "Poco/Timespan.h"
#include "Poco/Timestamp.h"
#include "Poco/DateTime.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/DateTimeParser.h"
#include "Poco/Nullable.h"
#include "Poco/Exception.h"
#include "Poco/Bugcheck.h"
#include "Poco/SharedPtr.h"
#include "Poco/ScopedUnlock.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <ios>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>


using namespace std::string_literals;
using namespace Poco::Data;
using namespace Poco::Data::Keywords;


namespace Poco::OSP::Auth::Data {


namespace
{
	const std::string STORED_PASSWORD_CHANGED_DURING_LOGIN("The stored password of user %s was set, removed or "
		"replaced while the login checked it, so the login is refused.");

	constexpr int MIN_HASH_VERSION = 1;
	constexpr int MAX_HASH_VERSION = 4;


	[[nodiscard]] std::string decodeBase64(const std::string& base64)
	{
		Poco::MemoryInputStream istr(base64.data(), base64.size());
		Poco::Base64Decoder decoder(istr);
		std::string result;
		Poco::StreamCopier::copyToString(decoder, result);
		return result;
	}

	std::string encodeBase64(const std::string& data)
	{
		std::ostringstream ostr;
		Poco::Base64Encoder encoder(ostr);
		encoder.rdbuf()->setLineLength(0);
		encoder << data;
		encoder.close();
		return ostr.str();
	}

	std::string digestToBinaryString(Poco::DigestEngine& engine)
	{
		Poco::DigestEngine::Digest d = engine.digest();
		return std::string(reinterpret_cast<const char*>(&d[0]), d.size());
	}

	std::string digestToHexString(Poco::DigestEngine& engine)
	{
		Poco::DigestEngine::Digest d = engine.digest();
		return Poco::DigestEngine::digestToHex(d);
	}

	std::string digestToBase64(Poco::DigestEngine& engine)
	{
		return encodeBase64(digestToBinaryString(engine));
	}

	[[nodiscard]] std::string randomBytes(std::size_t count)
	{
		// Read as a block: formatted extraction would skip whitespace bytes.
		std::string bytes(count, '\0');
		Poco::RandomInputStream rnd;
		rnd.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		return bytes;
	}

	[[nodiscard]] bool equalsInConstantTime(const std::string& hash1, const std::string& hash2)
	{
		return Poco::DigestEngine::constantTimeEquals(Poco::DigestEngine::Digest(hash1.begin(), hash1.end()),
			Poco::DigestEngine::Digest(hash2.begin(), hash2.end()));
	}
}


const std::string AuthAdminServiceImpl::USER_ATTR_FAILURES("auth_failures");
const std::string AuthAdminServiceImpl::USER_ATTR_LOCKOUT("auth_lockout");


AuthAdminServiceImpl::AuthAdminServiceImpl(const Poco::Util::AbstractConfiguration& properties, Poco::Logger& logger, const AuthParams& authParams, const LDAPParams& ldapParams):
	AbstractLDAPAuthAdminService(logger, ldapParams),
	_properties(properties),
	_authParams(authParams),
	_sqlCache(SQL_CACHE_BUCKET_SIZE),
	_permissionsCache(authParams.cacheSize, Poco::Timespan(authParams.cacheTimeout, 0).totalMilliseconds()),
	_passwordCache(Poco::Timespan(authParams.cacheTimeout, 0).totalMilliseconds()),
	_passwordPersistentHashCache(Poco::Timespan(authParams.cacheTimeout, 0).totalMilliseconds()),
	_cacheSalt(generateCacheSalt())
{
	if (_authParams.hashVersion < MIN_HASH_VERSION || _authParams.hashVersion > MAX_HASH_VERSION)
	{
		throw Poco::InvalidArgumentException("auth.hashVersion must be from 1 to 4"s,
			std::to_string(_authParams.hashVersion));
	}
	if (_authParams.pbkdf2Iterations <= 0)
	{
		throw Poco::InvalidArgumentException("auth.pbkdf2Rounds must be positive"s,
			std::to_string(_authParams.pbkdf2Iterations));
	}

	if (!ldapParams.localUsers.empty())
	{
		_pLocalUsersExpr = new Poco::RegularExpression(ldapParams.localUsers);
	}

	discoverInlineAttributes();

	if (_authParams.maxFailures > 0 && !isInlineAttribute(USER_ATTR_FAILURES))
	{
		_logger.warning("The user table has no column %s, so users are not locked out after auth.maxFailures failed "
			"logins. Add the column as in createdb.sql."s, USER_ATTR_FAILURES);
	}
}


AuthAdminServiceImpl::~AuthAdminServiceImpl() = default;


inline bool AuthAdminServiceImpl::isInlineAttribute(const std::string& attribute) const
{
	return _inlineAttributes.contains(attribute);
}


inline bool AuthAdminServiceImpl::isAuthAttributeName(const std::string& attribute) const
{
	return attribute.compare(0, AUTH_ATTRIBUTE_PREFIX.size(), AUTH_ATTRIBUTE_PREFIX) == 0;
}


bool AuthAdminServiceImpl::checkDatabaseConnectivity(const Poco::Util::AbstractConfiguration& properties, const AuthParams& authParams, Poco::Logger& logger)
{
	try
	{
		Poco::Data::Session sess(authParams.dbConnector, authParams.dbConnectionString);
		setupSession(sess, properties, authParams, logger);
		int result(-1);
		Poco::Data::Transaction xa(sess);
		sess << properties.getString("auth.sql.checkConnectivity"s), into(result), now;
		xa.commit();
		return result >= 0;
	}
	catch (Poco::Exception& exc)
	{
		logger.log(exc);
		return false;
	}
}


bool AuthAdminServiceImpl::verifyCredentials(const std::string& username, const std::string& credentials)
{
	std::string storedUsername(username);
	bool isLDAPUser = false;
	if (!authenticateImpl(storedUsername, credentials, false, isLDAPUser)) return false;

	if (Poco::icompare(storedUsername, username) != 0)
	{
		_logger.warning("The credentials of user %s were checked under a name other than the account name %s, which "
			"verifyCredentials() does not accept."s, username, storedUsername);
		return false;
	}
	return true;
}


void AuthAdminServiceImpl::changePassword(const std::string& username, const std::string& password)
{
	if (password.empty()) throw Poco::InvalidArgumentException("The password must not be empty"s, username);

	const std::string hashedCredentials = saltAndHashCredentials(username, password);
	const std::string cachedCredentials = hashCachedCredentials(username, password);

	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		std::string ldapAttr;
		if (!isLDAPUserImpl(sess, username, ldapAttr))
		{
			changePasswordImpl(sess, username, hashedCredentials);
		}
		else throw Poco::InvalidAccessException("cannot change password for LDAP user"s, username);
	}
	xa.commit();

	_passwordCache.update(username, cachedCredentials);
	_passwordPersistentHashCache.update(username, hashedCredentials);
}


bool AuthAdminServiceImpl::userExists(const std::string& username) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	bool result = false;
	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		result = userExistsImpl(sess, username);
	}
	xa.commit();
	return result;
}


bool AuthAdminServiceImpl::userEnabled(const std::string& username) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	bool enabled = false;
	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		enabled = validateUserImpl(sess, username, !isUserLockedOutImpl(sess, username));
	}
	xa.commit();
	return enabled;
}


bool AuthAdminServiceImpl::userExistsImpl(Poco::Data::Session& sess, const std::string& username) const
{
	int result = 0;
	sess << sqlFor("userExists"s), useRef(username), into(result), now;
	return result == 1;
}


void AuthAdminServiceImpl::permissionsForUser(const std::string& username, std::set<std::string>& permissions) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		sess << sqlFor("permissionsForUser"s), useRef(username), into(permissions), now;
	}
	xa.commit();
}


void AuthAdminServiceImpl::effectivePermissionsForUser(const std::string& username, std::set<std::string>& permissions) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		sess << sqlFor("effectivePermissionsForUser"s), useRef(username), useRef(username), useRef(username), into(permissions), now;
	}
	xa.commit();
}


void AuthAdminServiceImpl::grantPermissionsToUser(const std::string& username, const std::set<std::string>& permissions)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		std::string perm;
		Poco::Data::Statement statement = (sess << sqlFor("grantPermissionToUser"s), useRef(username), use(perm));
		for (std::set<std::string>::const_iterator it = permissions.begin(); it != permissions.end(); ++it)
		{
			perm = *it;
			statement.execute();
		}
	}
	xa.commit();

	_permissionsCache.remove(username);
}


void AuthAdminServiceImpl::revokePermissionsFromUser(const std::string& username, const std::set<std::string>& permissions)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		std::string perm;
		Poco::Data::Statement statement = (sess << sqlFor("revokePermissionFromUser"s), useRef(username), use(perm));
		for (std::set<std::string>::const_iterator it = permissions.begin(); it != permissions.end(); ++it)
		{
			perm = *it;
			statement.execute();
		}
	}
	xa.commit();

	_permissionsCache.remove(username);
}


void AuthAdminServiceImpl::replacePermissionsForUser(const std::string& username, const std::set<std::string>& permissions)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		replacePermissionsForUserImpl(sess, username, permissions);
	}
	xa.commit();

	_permissionsCache.remove(username);
}


void AuthAdminServiceImpl::replacePermissionsForUserImpl(Poco::Data::Session& sess, const std::string& username, const std::set<std::string>& permissions) const
{
	sess << sqlFor("removePermissionsFromUser"s), useRef(username), now;

	std::string perm;
	Poco::Data::Statement statement = (sess << sqlFor("grantPermissionToUser"s), useRef(username), use(perm));
	for (std::set<std::string>::const_iterator it = permissions.begin(); it != permissions.end(); ++it)
	{
		perm = *it;
		statement.execute();
	}
}


void AuthAdminServiceImpl::replacePermissionsForUserImpl(const std::string& username, const std::set<std::string>& permissions) const
{
	replacePermissionsForUserImpl(session(), username, permissions);
}


void AuthAdminServiceImpl::replacePermissionsForRoleImpl(Poco::Data::Session& sess, const std::string& rolename, const std::set<std::string>& permissions) const
{
	sess << sqlFor("removePermissionsFromRole"s), useRef(rolename), now;

	std::string perm;
	Poco::Data::Statement statement = (sess << sqlFor("grantPermissionToRole"s), useRef(rolename), use(perm));
	for (std::set<std::string>::const_iterator it = permissions.begin(); it != permissions.end(); ++it)
	{
		perm = *it;
		statement.execute();
	}
}


void AuthAdminServiceImpl::replacePermissionsForRoleImpl(const std::string& rolename, const std::set<std::string>& permissions) const
{
	replacePermissionsForRoleImpl(session(), rolename, permissions);
}


int AuthAdminServiceImpl::users(std::set<std::string>& users, int first, int limit) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	users.clear();
	int i = 0;
	int n = 0;
	std::string username;
	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		Poco::Data::Statement statement = (sess << sqlFor("users"s), into(username), range(0, 1));
		while (!statement.done())
		{
			statement.execute();
			if (statement.rowsExtracted() > 0)
			{
				if (i >= first && (limit == 0 || n < limit))
				{
					users.insert(username);
					n++;
				}
				i++;
			}
		}
	}
	xa.commit();
	return i;
}


int AuthAdminServiceImpl::usersWithName(std::set<std::string>& users, const std::string& pattern, int first, int limit) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	std::string sqlPattern = translatePattern(pattern);
	users.clear();
	int i = 0;
	int n = 0;
	std::string username;
	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		Poco::Data::Statement statement = (sess << sqlFor("usersWithName"s), into(username), use(sqlPattern), range(0, 1));
		while (!statement.done())
		{
			statement.execute();
			if (statement.rowsExtracted() > 0)
			{
				if (i >= first && (limit == 0 || n < limit))
				{
					users.insert(username);
					n++;
				}
				i++;
			}
		}
	}
	xa.commit();
	return i;
}


int AuthAdminServiceImpl::usersWithPermission(std::set<std::string>& users, const std::string& permission, int first, int limit) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	users.clear();
	int i = 0;
	int n = 0;
	std::string username;
	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		Poco::Data::Statement statement = (sess << sqlFor("usersWithPermission"s), into(username), useRef(permission), range(0, 1));
		while (!statement.done())
		{
			statement.execute();
			if (statement.rowsExtracted() > 0)
			{
				if (i >= first && (limit == 0 || n < limit))
				{
					users.insert(username);
					n++;
				}
				i++;
			}
		}
	}
	xa.commit();
	return i;
}


int AuthAdminServiceImpl::usersWithRole(std::set<std::string>& users, const std::string& role, int first, int limit) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	users.clear();
	int i = 0;
	int n = 0;
	std::string username;
	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		Poco::Data::Statement statement = (sess << sqlFor("usersWithRole"s), into(username), useRef(role), range(0, 1));
		while (!statement.done())
		{
			statement.execute();
			if (statement.rowsExtracted() > 0)
			{
				if (i >= first && (limit == 0 || n < limit))
				{
					users.insert(username);
					n++;
				}
				i++;
			}
		}
	}
	xa.commit();
	return i;
}


int AuthAdminServiceImpl::usersWithAttribute(std::set<std::string>& users, const std::string& attribute, const std::string& value, int first, int limit) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	users.clear();
	int i = 0;
	int n = 0;
	std::string username;
	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		Poco::Data::Statement statement = isInlineAttribute(attribute)
			? (sess << sqlFor("usersWithInlineAttribute"s), attribute, into(username), useRef(value), range(0, 1))
			: (sess << sqlFor("usersWithAttribute"s), into(username), useRef(attribute), useRef(value), range(0, 1));
		while (!statement.done())
		{
			statement.execute();
			if (statement.rowsExtracted() > 0)
			{
				if (i >= first && (limit == 0 || n < limit))
				{
					users.insert(username);
					n++;
				}
				i++;
			}
		}
	}
	xa.commit();
	return i;
}


void AuthAdminServiceImpl::addUser(const std::string& username, const std::string& password)
{
	if (password.empty()) throw Poco::InvalidArgumentException("The password must not be empty"s, username);

	const std::string hashedPassword = saltAndHashCredentials(username, password);
	{
		Poco::FastMutex::ScopedLock lock(_mutex);

		Poco::Data::Session& sess = session();
		Poco::Data::Transaction xa(sess, &_logger);
		{
			if (userExistsImpl(sess, username)) throw Poco::ExistsException(username);

			addUserImpl(sess, username, hashedPassword);

			if (!_ldapParams.uri.empty())
			{
				replaceUserAttributeImpl(sess, username, USER_ATTR_LDAP, "false"s);
			}
		}
		xa.commit();
	}

	userAdded(username);
}


void AuthAdminServiceImpl::addUserImpl(Poco::Data::Session& sess, const std::string& username,
	const std::string& hashedPassword) const
{
	sess << sqlFor("addUser"s), useRef(username), useRef(hashedPassword), now;
}


void AuthAdminServiceImpl::removeUser(const std::string& username)
{
	{
		Poco::FastMutex::ScopedLock lock(_mutex);

		Poco::Data::Session& sess = session();
		Poco::Data::Transaction xa(sess, &_logger);
		{
			if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

			removeUserImpl(sess, username);
		}
		xa.commit();

		uncacheUserImpl(username);
	}

	userRemoved(username);
}


void AuthAdminServiceImpl::removeUserImpl(Poco::Data::Session& sess, const std::string& username) const
{
	sess << sqlFor("removeUser"s), useRef(username), now;
	sess << sqlFor("removeUserPermissions"s), useRef(username), now;
	sess << sqlFor("removeUserRoles"s), useRef(username), now;
	sess << sqlFor("removeAllUserAttributes"s), useRef(username), now;
}


void AuthAdminServiceImpl::permissionsForRole(const std::string& rolename, std::set<std::string>& permissions) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!roleExistsImpl(sess, rolename)) throw Poco::NotFoundException(rolename);

		sess << sqlFor("permissionsForRole"s), useRef(rolename), into(permissions), now;
	}
	xa.commit();
}


void AuthAdminServiceImpl::grantPermissionsToRole(const std::string& rolename, const std::set<std::string>& permissions)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!roleExistsImpl(sess, rolename)) throw Poco::NotFoundException(rolename);

		std::string perm;
		Poco::Data::Statement statement = (sess << sqlFor("grantPermissionToRole"s), useRef(rolename), use(perm));
		for (std::set<std::string>::const_iterator it = permissions.begin(); it != permissions.end(); ++it)
		{
			perm = *it;
			statement.execute();
		}
	}
	xa.commit();

	_permissionsCache.clear();
}


void AuthAdminServiceImpl::revokePermissionsFromRole(const std::string& rolename, const std::set<std::string>& permissions)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!roleExistsImpl(sess, rolename)) throw Poco::NotFoundException(rolename);

		std::string perm;
		Poco::Data::Statement statement = (sess << sqlFor("revokePermissionFromRole"s), useRef(rolename), use(perm));
		for (std::set<std::string>::const_iterator it = permissions.begin(); it != permissions.end(); ++it)
		{
			perm = *it;
			statement.execute();
		}
	}
	xa.commit();

	_permissionsCache.clear();
}


void AuthAdminServiceImpl::replacePermissionsForRole(const std::string& rolename, const std::set<std::string>& permissions)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!roleExistsImpl(sess, rolename)) throw Poco::NotFoundException(rolename);

		replacePermissionsForRoleImpl(sess, rolename, permissions);
	}
	xa.commit();

	_permissionsCache.clear();
}


bool AuthAdminServiceImpl::roleExists(const std::string& rolename) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	bool result = false;
	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		result = roleExistsImpl(sess, rolename);
	}
	xa.commit();
	return result;
}


bool AuthAdminServiceImpl::roleExistsImpl(Poco::Data::Session& sess, const std::string& rolename) const
{
	int result = 0;
	sess << sqlFor("roleExists"s), useRef(rolename), into(result), now;
	return result == 1;
}


void AuthAdminServiceImpl::roles(std::set<std::string>& roles) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		sess << sqlFor("roles"s), into(roles), now;
	}
	xa.commit();
}


void AuthAdminServiceImpl::addRole(const std::string& rolename)
{
	{
		Poco::FastMutex::ScopedLock lock(_mutex);

		Poco::Data::Session& sess = session();
		Poco::Data::Transaction xa(sess, &_logger);
		{
			if (roleExistsImpl(sess, rolename)) throw Poco::ExistsException(rolename);

			sess << sqlFor("addRole"s), useRef(rolename), now;

			if (!_ldapParams.uri.empty())
			{
				replaceRoleAttributeImpl(sess, rolename, ROLE_ATTR_LDAP, "false"s);
			}
		}
		xa.commit();
	}

	roleAdded(rolename);
}


void AuthAdminServiceImpl::removeRole(const std::string& rolename)
{
	{
		Poco::FastMutex::ScopedLock lock(_mutex);

		Poco::Data::Session& sess = session();
		Poco::Data::Transaction xa(sess, &_logger);
		{
			if (!roleExistsImpl(sess, rolename)) throw Poco::NotFoundException(rolename);

			sess << sqlFor("removeRole"s), useRef(rolename), now;
			sess << sqlFor("removePermissionsFromRole"s), useRef(rolename), now;
		}
		xa.commit();

		_permissionsCache.clear();
	}

	roleRemoved(rolename);
}


bool AuthAdminServiceImpl::authorizeRole(const std::string& rolename, const std::string& permission) const
{
	int authorized = 0;

	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		sess << sqlFor("authorizeRole"s), useRef(rolename), useRef(permission), into(authorized), now;
	}
	xa.commit();
	
	return authorized > 0;
}


void AuthAdminServiceImpl::createOrUpdateRoleImpl(Poco::Data::Session& session, const std::string& rolename) const
{
	if (!roleExistsImpl(session, rolename))
	{
		session << sqlFor("addRole"s), useRef(rolename), now;
	}
}


void AuthAdminServiceImpl::createOrUpdateRoleImpl(const std::string& rolename) const
{
	createOrUpdateRoleImpl(session(), rolename);
}

void AuthAdminServiceImpl::rolesForUser(const std::string& username, std::set<std::string>& roles) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		sess << sqlFor("rolesForUser"s), useRef(username), into(roles), now;
	}
	xa.commit();
}


void AuthAdminServiceImpl::assignRoleToUser(const std::string& username, const std::string& rolename)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);
		if (!roleExistsImpl(sess, rolename)) throw Poco::InvalidArgumentException("Non-existing role", rolename);

		sess << sqlFor("assignRoleToUser"s), useRef(username), useRef(rolename), now;
	}
	xa.commit();

	_permissionsCache.clear();
}


void AuthAdminServiceImpl::removeRoleFromUser(const std::string& username, const std::string& rolename)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		sess << sqlFor("removeRoleFromUser"s), useRef(username), useRef(rolename), now;
	}
	xa.commit();

	_permissionsCache.clear();
}


void AuthAdminServiceImpl::replaceRolesForUser(const std::string& username, const std::set<std::string>& roles)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);
		for (std::set<std::string>::const_iterator it = roles.begin(); it != roles.end(); ++it)
		{
			if (!roleExistsImpl(sess, *it)) throw Poco::InvalidArgumentException("Non-existing role"s, *it);
		}

		replaceRolesForUserImpl(sess, username, roles);
	}
	xa.commit();

	_permissionsCache.clear();
}


void AuthAdminServiceImpl::replaceRolesForUserImpl(Poco::Data::Session& sess, const std::string& username, const std::set<std::string>& roles) const
{
	sess << sqlFor("removeRolesFromUser"s), useRef(username), now;

	std::string role;
	Poco::Data::Statement statement = (sess << sqlFor("assignRoleToUser"s), useRef(username), use(role));
	for (std::set<std::string>::const_iterator it = roles.begin(); it != roles.end(); ++it)
	{
		role = *it;
		statement.execute();
	}
}


void AuthAdminServiceImpl::replaceRolesForUserImpl(const std::string& username, const std::set<std::string>& roles) const
{
	replaceRolesForUserImpl(session(), username, roles);
}


bool AuthAdminServiceImpl::authenticate(const std::string& usernameInput, const std::string& credentials) const
{
	auto username { usernameInput };
	bool isLDAPUser = false;
	if (!authenticateImpl(username, credentials, true, isLDAPUser)) return false;

	if (Poco::icompare(username, usernameInput) != 0)
	{
		_logger.warning("User %s logged in with a name other than the account name %s, which authenticate() does not "
			"accept; the caller must use authenticateAccount()."s, usernameInput, username);
		return false;
	}
	postLoginNotification(username, isLDAPUser);
	return true;
}


bool AuthAdminServiceImpl::authenticateAccount(const std::string& username, const std::string& credentials,
	std::string& accountName) const
{
	auto storedName { username };
	bool isLDAPUser = false;
	if (!authenticateImpl(storedName, credentials, true, isLDAPUser)) return false;

	postLoginNotification(storedName, isLDAPUser);
	accountName = std::move(storedName);
	return true;
}


void AuthAdminServiceImpl::postLoginNotification(const std::string& username, bool isLDAPUser) const
{
	// Collecting the data of the notification takes several queries, so skip
	// it when no observer is registered.
	auto& notifCenter = Poco::NotificationCenter::defaultCenter();
	if (notifCenter.hasObservers())
	{
		try
		{
			const auto fullName = getUserAttribute(username, Poco::OSP::Auth::AuthAdminService::USER_ATTR_FULL_NAME);

			std::set<std::string> roles;
			rolesForUser(username, roles);

			std::set<std::string> permissions;
			permissionsForUser(username, permissions);

			std::map<std::string, std::string> attributes;
			attributesForUser(username, attributes);

			notifCenter.postNotification(
				new AuthNotification(isLDAPUser, username, fullName, roles, permissions, attributes)
			);

			_logger.debug("Posted the login notification for user %s."s, username);
		}
		catch (Poco::NotFoundException&)
		{
			_logger.debug("Auth: user %s was removed after the login, no notification is posted."s, username);
		}
	}
}


bool AuthAdminServiceImpl::authenticateImpl(std::string& username, const std::string& credentials, bool validate,
	bool& isLDAPUser) const
{
	const std::string requestedName(username);
	AccountState account;
	{
		Poco::FastMutex::ScopedLock lock(_mutex);

		// One LDAP round trip per user at a time, so that concurrent attempts
		// cannot get more password guesses than the failure count allows.
		const auto deadline = std::chrono::steady_clock::now() + ldapLoginWaitTime();
		std::optional<PasswordCheck> check;
		for (;;)
		{
			username = requestedName;
			std::string storedHash;
			{
				Poco::Data::Session& sess = session();
				Poco::Data::Transaction xa(sess, &_logger);
				std::string ldapAttr;
				isLDAPUser = ldapUserNameImpl(sess, username, ldapAttr);
				std::optional<bool> result = authenticateCachedImpl(sess, username, credentials);
				if (!result.has_value() && !isLDAPUser)
				{
					storedHash = storedHashImpl(sess, username);
					if (check.has_value() && check->storedHash == storedHash)
					{
						result = applyPasswordCheckImpl(sess, username, credentials, *check, true);
					}
				}
				if (result.has_value())
				{
					const bool authenticated = validate ? validateUserImpl(sess, username, *result) : *result;
					xa.commit();
					return authenticated;
				}
				account.ldapUser = isLDAPUser;
				account.ldapAttr = ldapAttr;
				account.exists = userExistsImpl(sess, username);
				xa.commit();
			}

			if (!isLDAPUser)
			{
				if (check.has_value())
				{
					_logger.warning(STORED_PASSWORD_CHANGED_DURING_LOGIN, username);
					return false;
				}
				// The other calls of the service proceed while the hash is
				// computed; the next pass checks that the stored hash is unchanged.
				const Poco::ScopedUnlock<Poco::FastMutex> unlock(_mutex);
				check = checkPassword(username, credentials, storedHash, false);
				continue;
			}
			if (!_ldapLogins.contains(username)) break;
			if (!waitForLDAPLoginImpl(username, deadline)) return false;
		}
		_ldapLogins.emplace(username, LDAPLogin{username, deadline, std::nullopt});
	}
	const LDAPLoginGuard guard(*this, username);

	// The conversation with the LDAP server runs without the lock, so that
	// a slow server does not block the other calls of the service.
	LDAPUser ldapUser;
	std::optional<LDAPResult> ldapResult;
	try
	{
		ldapResult = verifyLDAP(username, credentials, ldapUser);
	}
	catch (Poco::Data::DataException&)
	{
		// From acceptLDAPAccount(), which reads the database.
		throw;
	}
	catch (Poco::Exception& exc)
	{
		_logger.warning("The LDAP directory cannot be used for the login of user %s: %s"s, username, exc.displayText());
	}
	if (ldapResult.has_value() && *ldapResult == LDAPResult::LOCKED_OUT)
	{
		// acceptLDAPAccount() has refused the login and logged why; nothing
		// is stored or counted.
		return false;
	}

	Poco::FastMutex::ScopedLock lock(_mutex);

	const std::string loginName(username);
	const bool accepted = ldapResult.has_value() && *ldapResult == LDAPResult::ACCEPTED;
	const bool checkNeeded = accepted || !ldapResult.has_value() || *ldapResult == LDAPResult::INACCESSIBLE;
	std::optional<PasswordCheck> check;
	for (;;)
	{
		username = loginName;
		std::string storedHash;
		{
			// By value: the login holds its session until its transaction ends.
			Poco::Data::Session sess = session();
			Poco::Data::Transaction xa(sess, &_logger);
			std::string currentName(requestedName);
			const AccountState currentAccount = accountStateImpl(sess, currentName);
			if (currentAccount.exists != account.exists || currentAccount.ldapUser != account.ldapUser ||
				currentAccount.ldapAttr != account.ldapAttr || isUserLockedOutImpl(sess, username))
			{
				_logger.warning("The account of user %s changed during the login with the LDAP server, so the answer "
					"of the LDAP server is not used."s, username);
				xa.commit();
				return false;
			}
			if (!ldapUser.accountName.empty())
			{
				const std::string accountName = Poco::toLower(ldapUser.accountName);
				if (accountName != username)
				{
					std::string ldapAttr;
					if (!isLDAPUserImpl(sess, accountName, ldapAttr))
					{
						_logger.warning("The LDAP entry of user %s has the account name %s, which is the name of a "
							"local user or matches auth.ldap.localUsers, so the login is refused."s, username,
							accountName);
						xa.commit();
						return false;
					}
					if (isLockedOutLoginImpl(sess, accountName, username))
					{
						xa.commit();
						return false;
					}
					username = accountName;
				}
			}
			if (checkNeeded)
			{
				storedHash = storedHashImpl(sess, username);
			}
			if (!checkNeeded || (check.has_value() && check->storedHash == storedHash))
			{
				const LoginSessionScope loginSession(*this, sess);
				bool authenticated = applyLDAPResultImpl(sess, username, credentials, ldapResult, ldapUser,
					check.value_or(PasswordCheck()));
				if (validate)
				{
					authenticated = validateUserImpl(sess, username, authenticated);
				}
				xa.commit();
				return authenticated;
			}
			xa.commit();
		}

		if (check.has_value())
		{
			_logger.warning(STORED_PASSWORD_CHANGED_DURING_LOGIN, username);
			return false;
		}
		// The other calls of the service proceed while the hash is computed;
		// the next pass checks that the account and the stored hash are unchanged.
		const Poco::ScopedUnlock<Poco::FastMutex> unlock(_mutex);
		check = checkPassword(username, credentials, storedHash, accepted);
	}
}


bool AuthAdminServiceImpl::ldapUserNameImpl(Poco::Data::Session& sess, std::string& username,
	std::string& ldapAttr) const
{
	const bool isLDAPUser = isLDAPUserImpl(sess, username, ldapAttr);
	if (isLDAPUser)
	{
		// Always use lower case username for LDAP, because it is case insensitive.
		// The password cache is keyed by that name, so it must be known before the lookup.
		Poco::toLowerInPlace(username);
	}
	return isLDAPUser;
}


AuthAdminServiceImpl::AccountState AuthAdminServiceImpl::accountStateImpl(Poco::Data::Session& sess,
	std::string& username) const
{
	AccountState account;
	account.ldapUser = ldapUserNameImpl(sess, username, account.ldapAttr);
	account.exists = userExistsImpl(sess, username);
	return account;
}


int AuthAdminServiceImpl::loginsWaitingForLDAP() const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	return _ldapLoginsWaiting;
}


std::chrono::milliseconds AuthAdminServiceImpl::ldapLoginWaitTime() const
{
	return std::chrono::seconds(_ldapParams.timeout) + LDAP_LOGIN_WAIT_MARGIN;
}


bool AuthAdminServiceImpl::waitForLDAPLoginImpl(const std::string& username,
	std::chrono::steady_clock::time_point deadline) const
{
	const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
	if (remaining.count() <= 0)
	{
		_logger.warning("Another login of user %s still waits for the LDAP server, so this login is refused."s,
			username);
		return false;
	}
	++_ldapLoginsWaiting;
	_ldapLoginEnded.tryWait(_mutex, static_cast<long>(remaining.count()));
	--_ldapLoginsWaiting;
	return true;
}


AuthAdminServiceImpl::LDAPLoginGuard::LDAPLoginGuard(const AuthAdminServiceImpl& service, const std::string& username):
	_service(service),
	_username(username)
{
}


AuthAdminServiceImpl::LDAPLoginGuard::~LDAPLoginGuard()
{
	try
	{
		Poco::FastMutex::ScopedLock lock(_service._mutex);
		for (auto it = _service._ldapLogins.begin(); it != _service._ldapLogins.end();)
		{
			if (it->second.userName == _username)
			{
				it = _service._ldapLogins.erase(it);
			}
			else
			{
				++it;
			}
		}
		_service._ldapLoginEnded.broadcast();
	}
	catch (...)
	{
		poco_unexpected();
	}
}


AuthAdminServiceImpl::LoginSessionScope::LoginSessionScope(const AuthAdminServiceImpl& service,
	const Poco::Data::Session& session):
	_service(service)
{
	_service._loginSession = session;
}


AuthAdminServiceImpl::LoginSessionScope::~LoginSessionScope()
{
	_service._loginSession.reset();
}


std::optional<bool> AuthAdminServiceImpl::authenticateCachedImpl(Poco::Data::Session& sess,
	const std::string& username, const std::string& credentials) const
{
	if (isUserLockedOutImpl(sess, username))
	{
		_logger.notice("Login attempt by locked-out user %s."s, username);
		return false;
	}

	Poco::SharedPtr<std::string> pCachedPass = _passwordCache.get(username);
	if (pCachedPass != nullptr)
	{
		_logger.debug("Have password in cache."s);
		std::string hashedCreds = hashCachedCredentials(username, credentials);
		if (equalsInConstantTime(*pCachedPass, hashedCreds))
		{
			resetFailureCountImpl(sess, username);
			return true;
		}
		// The password may have been changed elsewhere, so the stored
		// credentials decide.
	}
	return std::nullopt;
}


bool AuthAdminServiceImpl::applyLDAPResultImpl(Poco::Data::Session& sess, const std::string& username,
	const std::string& credentials, std::optional<LDAPResult> ldapResult, const LDAPUser& ldapUser,
	const PasswordCheck& check) const
{
	if (ldapResult.has_value() && *ldapResult == LDAPResult::ACCEPTED)
	{
		const auto login = _ldapLogins.find(username);
		poco_assert (login != _ldapLogins.end());
		login->second.passwordCheck = check;
		try
		{
			storeLDAPUser(username, credentials, ldapUser);
		}
		catch (Poco::Data::DataException& exc)
		{
			// Rethrown, so that the transaction of the login is rolled back
			// and no partly stored user remains.
			_logger.error("The LDAP login of user %s cannot be stored in the database, so the login fails and nothing "
				"of it is stored: %s"s, username, exc.displayText());
			// The caches may hold the password hash that the rolled-back store has written.
			uncacheUserImpl(username);
			throw;
		}
		catch (Poco::Exception& exc)
		{
			_logger.warning("The LDAP directory cannot be used for the login of user %s: %s"s, username,
				exc.displayText());
			ldapResult.reset();
		}
	}
	if (!ldapResult.has_value())
	{
		if (canUseLocalCredentials(username))
		{
			// The LDAP server has not checked the password, so a mismatch
			// with the stored copy counts as a failure.
			if (!ldapUser.passwordVerified)
			{
				return applyPasswordCheckImpl(sess, username, credentials, check, true);
			}
			// The LDAP server has accepted the password, so a mismatch with
			// the stored copy is not counted as a failure.
			if (applyPasswordCheckImpl(sess, username, credentials, check, false))
			{
				return true;
			}
		}
		_logger.warning("User %s is not authenticated: the LDAP directory cannot be used, and the stored password may "
			"not be used or does not match."s, username);
		return false;
	}

	switch (*ldapResult)
	{
	case LDAPResult::ACCEPTED:
		if (std::any_of(ldapUser.roles.begin(), ldapUser.roles.end(),
			[](const auto& role) { return role.second.has_value(); }))
		{
			// storeLDAPUser() may have replaced the permissions of roles
			// that other users have as well.
			_permissionsCache.clear();
		}
		reloadPermissions(sess, username);
		_passwordCache.update(username, hashCachedCredentials(username, credentials));
		unlockOutUserImpl(sess, username);
		if (getUserAttributeImpl(sess, username, USER_ATTR_LDAP, ""s).empty())
		{
			replaceUserAttributeImpl(sess, username, USER_ATTR_LDAP, "true"s);
		}
		replaceUserAttributeImpl(sess, username, USER_ATTR_SUCCESS_TIME, std::to_string(Poco::Timestamp().epochTime()));
		return true;

	case LDAPResult::INACCESSIBLE:
		if (canUseLocalCredentials(username))
		{
			return applyPasswordCheckImpl(sess, username, credentials, check, true);
		}
		_logger.warning("The LDAP server is not accessible, so user %s is not authenticated."s, username);
		return false;

	case LDAPResult::REJECTED:
		_logger.warning("The LDAP login of user %s is rejected: the LDAP server rejected the password, the user has no "
			"LDAP entry, or the user name or the password is empty or contains a NUL character."s, username);
		if (userExistsImpl(sess, username))
		{
			// The user and its attributes stay; the stored copy of the password
			// must not be accepted while the LDAP server cannot be reached.
			// Only a password that an LDAP login has stored is cleared; a user
			// without the auth_ldap attribute can have a local password, such
			// as an admin user created with SQL.
			if (getUserAttributeImpl(sess, username, USER_ATTR_LDAP, ""s) == "true"s)
			{
				changePasswordImpl(sess, username, ""s);
			}
			uncacheUserImpl(username);
			lockOutUserIfTooManyFailures(sess, username);
		}
		return false;

	case LDAPResult::REFUSED:
		// The LDAP server has not said whether the password is correct, so the
		// stored copy is kept, but neither it nor a cached password is accepted.
		if (userExistsImpl(sess, username))
		{
			uncacheUserImpl(username);
			lockOutUserIfTooManyFailures(sess, username);
		}
		return false;

	case LDAPResult::LOCKED_OUT:
		// authenticateImpl() does not pass it: acceptLDAPAccount() has decided the login.
	default:
		poco_bugcheck();
	}
}


bool AuthAdminServiceImpl::acceptLDAPAccount(const std::string& userName, const std::string& accountName) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	const bool otherName = accountName != userName;
	std::chrono::steady_clock::time_point deadline;
	if (otherName)
	{
		const auto login = _ldapLogins.find(userName);
		poco_assert (login != _ldapLogins.end());
		deadline = login->second.deadline;
		while (_ldapLogins.contains(accountName))
		{
			if (!waitForLDAPLoginImpl(accountName, deadline)) return false;
		}
	}

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	const bool lockedOut = isLockedOutLoginImpl(sess, accountName, userName);
	xa.commit();
	if (lockedOut) return false;
	if (otherName)
	{
		_ldapLogins.emplace(accountName, LDAPLogin{userName, deadline, std::nullopt});
	}
	return true;
}


std::string AuthAdminServiceImpl::storedHashImpl(Poco::Data::Session& sess, const std::string& username) const
{
	std::string storedHash;
	sess << sqlFor("passwordHashForUser"s), useRef(username), into(storedHash), now;
	return storedHash;
}


bool AuthAdminServiceImpl::applyPasswordCheckImpl(Poco::Data::Session& sess, const std::string& username,
	const std::string& credentials, const PasswordCheck& check, bool countFailure) const
{
	if (!check.matches)
	{
		if (countFailure)
		{
			lockOutUserIfTooManyFailures(sess, username);
		}
		return false;
	}

	_passwordPersistentHashCache.update(username, check.storedHash);
	if (!check.newHash.empty())
	{
		_logger.information("Upgrading the v%d password hash of user %s to v%d."s, check.storedVersion, username,
			_authParams.hashVersion);
		changePasswordImpl(sess, username, check.newHash);
		_passwordPersistentHashCache.update(username, check.newHash);
	}
	acceptStoredCredentialsImpl(sess, username, credentials);
	return true;
}


void AuthAdminServiceImpl::storeCredentialsImpl(Poco::Data::Session& sess, const std::string& username,
	const PasswordCheck& check) const
{
	if (!userExistsImpl(sess, username))
	{
		addUserImpl(sess, username, check.newHash);
	}
	else if (!check.newHash.empty())
	{
		changePasswordImpl(sess, username, check.newHash);
		_passwordPersistentHashCache.update(username, check.newHash);
	}
}


void AuthAdminServiceImpl::acceptStoredCredentialsImpl(Poco::Data::Session& sess, const std::string& username,
	const std::string& credentials) const
{
	reloadPermissions(sess, username);
	_passwordCache.update(username, hashCachedCredentials(username, credentials));
	unlockOutUserImpl(sess, username);
}


void AuthAdminServiceImpl::allRoleMappings(std::map<std::string, std::string>& mappings) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		std::string extRole;
		std::string intRole;
		Poco::Data::Statement stmt = (sess << sqlFor("allRoleMappings"s), into(extRole), into(intRole), range(0, 1));
		while (!stmt.done())
		{
			if (stmt.execute() > 0)
			{
				mappings[extRole] = intRole;
			}
		}
	}
	xa.commit();
}


void AuthAdminServiceImpl::roleMappingForExternal(const std::string& externalRole, std::string& internalRole) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		internalRole.clear();
		sess << sqlFor("roleMappingForExternal"s), useRef(externalRole), into(internalRole), now;
	}
	xa.commit();
}


void AuthAdminServiceImpl::setRoleMapping(const std::string& externalRole, const std::string& internalRole)
{
	{
		Poco::FastMutex::ScopedLock lock(_mutex);

		Poco::Data::Session& sess = session();
		Poco::Data::Transaction xa(sess, &_logger);
		{
			sess << sqlFor("removeRoleMapping"s), useRef(externalRole), now;
			sess << sqlFor("addRoleMapping"s), useRef(externalRole), useRef(internalRole), now;
		}
		xa.commit();

		_permissionsCache.clear();
	}
}


void AuthAdminServiceImpl::removeRoleMapping(const std::string& externalRole)
{
	{
		Poco::FastMutex::ScopedLock lock(_mutex);

		Poco::Data::Session& sess = session();
		Poco::Data::Transaction xa(sess, &_logger);
		{
			sess << sqlFor("removeRoleMapping"s), useRef(externalRole), now;
		}
		xa.commit();

		_permissionsCache.clear();
	}
}


void AuthAdminServiceImpl::revokePermission(const std::string& permission)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		sess << sqlFor("revokePermissionFromUsers"s), useRef(permission), now;
		sess << sqlFor("revokePermissionFromRoles"s), useRef(permission), now;
	}
	xa.commit();

	_permissionsCache.clear();
}


void AuthAdminServiceImpl::lockOutUser(const std::string& username, const Poco::DateTime& untilDateTime)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		lockOutUserImpl(sess, username, untilDateTime);
	}
	xa.commit();
}


void AuthAdminServiceImpl::unlockOutUser(const std::string& username)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		unlockOutUserImpl(sess, username);
	}
	xa.commit();
}


bool AuthAdminServiceImpl::isUserLockedOut(const std::string& username) const
{
	bool result = false;
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		result = isUserLockedOutImpl(sess, username);
	}
	xa.commit();

	return result;
}


bool AuthAdminServiceImpl::isExternalUser([[maybe_unused]] const std::string& username) const
{
	bool result = false;

#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	Poco::FastMutex::ScopedLock lock(_mutex);

	if (!_ldapParams.uri.empty())
	{
		Poco::Data::Session& sess = session();
		Poco::Data::Transaction xa(sess, &_logger);
		{
			std::string ldapAttr;
			result = isLDAPUserImpl(sess, username, ldapAttr);
		}
		xa.commit();
	}
#endif

	return result;
}


bool AuthAdminServiceImpl::isExternalRole(const std::string& rolename) const
{
	bool result = false;
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		std::string ldapAttr;
		result = isLDAPRoleImpl(sess, rolename, ldapAttr);
	}
	xa.commit();

	return result;
}


bool AuthAdminServiceImpl::isLDAPUserImpl([[maybe_unused]] Poco::Data::Session& sess,
	[[maybe_unused]] const std::string& username, [[maybe_unused]] std::string& ldapAttr) const
{
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	if (!_ldapParams.uri.empty())
	{
		// A local user is stored under the name as given, an LDAP user in lower case.
		const std::string storedName = userExistsImpl(sess, username) ? username : Poco::toLower(username);

		ldapAttr = getUserAttributeImpl(sess, storedName, USER_ATTR_LDAP, ""s);
		if (ldapAttr == "false")
		{
			return false;
		}
		return ldapAttr == "true" || _pLocalUsersExpr == nullptr || !_pLocalUsersExpr->match(storedName);
	}
#endif
	return false;
}


bool AuthAdminServiceImpl::isLDAPRoleImpl([[maybe_unused]] Poco::Data::Session& sess,
	[[maybe_unused]] const std::string& rolename, [[maybe_unused]] std::string& ldapAttr) const
{
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	if (!_ldapParams.uri.empty())
	{
		ldapAttr = getRoleAttributeImpl(sess, rolename, ROLE_ATTR_LDAP, ""s);
		if (ldapAttr == "true")
		{
			return true;
		}
	}
#endif
	return false;
}


void AuthAdminServiceImpl::reset()
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	_inlineAttributes.clear();
	_permissionsCache.clear();
	_passwordCache.clear();
	_passwordPersistentHashCache.clear();

	_pSession.reset();

	discoverInlineAttributes();
}


void AuthAdminServiceImpl::invalidateCaches(int what)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	if (what & INVALIDATE_CACHED_PASSWORDS)
	{
		_passwordCache.clear();
		_passwordPersistentHashCache.clear();
	}
	if (what & INVALIDATE_CACHED_PERMISSIONS)
	{
		_permissionsCache.clear();
	}
}


bool AuthAdminServiceImpl::authorize(const std::string& username, const std::string& permission) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	std::string lookupUsername = username;
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	const auto lcUsername = Poco::toLower(username);
	if (lcUsername != username && !_ldapParams.uri.empty())
	{
		Poco::Data::Session& sess = session();
		Poco::Data::Transaction xa(sess, &_logger);
		std::string ldapAttr;
		if (isLDAPUserImpl(sess, username, ldapAttr))
		{
			lookupUsername = lcUsername;
		}
		xa.commit();
	}
#endif

	PermissionsPtr pPerms = _permissionsCache.get(lookupUsername);
	if (pPerms == nullptr)
	{
		Poco::Data::Session& sess = session();
		Poco::Data::Transaction xa(sess, &_logger);
		{
			pPerms = loadPermissions(sess, lookupUsername);
		}
		xa.commit();
	}
	return pPerms->contains(permission);
}


bool AuthAdminServiceImpl::authorize(const std::string& userName, const std::string& roleOrScope, const std::string& permission) const
{
	if (roleOrScope.empty())
	{
		return authorize(userName, permission);
	}
	else
	{
		return authorize(userName, permission) && authorizeRole(roleOrScope, permission);
	}
}


void AuthAdminServiceImpl::uncacheUser(const std::string& username)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	uncacheUserImpl(username);
}


void AuthAdminServiceImpl::uncacheUserImpl(const std::string& username) const
{
	_passwordCache.remove(username);
	_passwordPersistentHashCache.remove(username);
	_permissionsCache.remove(username);
}


std::string AuthAdminServiceImpl::getUserAttribute(const std::string& username, const std::string& attribute, const std::string& deflt) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	std::string result(deflt);
	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		if (attribute == "$hash")
		{
			result = hashForUser(sess, username);
		}
		else if (attribute == "$salt" || attribute == "$iterations" || attribute == "$version")
		{
			int version = 1;
			int iterations = 0;
			std::string salt = saltForUser(sess, username, version, iterations);
			if (attribute == "$salt")
				result = salt;
			else if (attribute == "$iterations")
				result = Poco::NumberFormatter::format(iterations);
			else
				result = Poco::NumberFormatter::format(version);
		}
		else
		{
			result = getUserAttributeImpl(sess, username, attribute, deflt);
		}
	}
	xa.commit();
	return result;
}


std::string AuthAdminServiceImpl::getUserAttributeImpl(Poco::Data::Session& sess, const std::string& username, const std::string& attribute, const std::string& deflt) const
{
	std::string result = deflt;
	if (isInlineAttribute(attribute))
	{
		Poco::Nullable<std::string> nullable;
		sess << sqlFor("getInlineUserAttribute"s), attribute, useRef(username), into(nullable), now;
		result = nullable.value(deflt);
	}
	else
	{
		sess << sqlFor("getUserAttribute"s), useRef(username), useRef(attribute), into(result), now;
	}
	return result;
}

std::string AuthAdminServiceImpl::getUserAttributeImpl(const std::string& username, const std::string& attribute, const std::string& deflt) const
{
	return getUserAttributeImpl(session(), username, attribute, deflt);
}


std::string AuthAdminServiceImpl::getRoleAttributeImpl(Poco::Data::Session& sess, const std::string& rolename, const std::string& attribute, const std::string& deflt) const
{
	std::string result = deflt;
	if (isInlineAttribute(attribute))
	{
		Poco::Nullable<std::string> nullable;
		sess << sqlFor("getInlineRoleAttribute"s), attribute, useRef(rolename), into(nullable), now;
		result = nullable.value(deflt);
	}
	else
	{
		sess << sqlFor("getRoleAttribute"s), useRef(rolename), useRef(attribute), into(result), now;
	}
	return result;
}

std::string AuthAdminServiceImpl::getRoleAttributeImpl(const std::string& rolename, const std::string& attribute, const std::string& deflt) const
{
	return getRoleAttributeImpl(session(), rolename, attribute, deflt);
}


void AuthAdminServiceImpl::setUserAttribute(const std::string& username, const std::string& attribute, const std::string& value)
{
	if (isAuthAttribute(attribute))
		throw Poco::InvalidArgumentException("Authentication attribute cannot be set"s, attribute);

	storeUserAttribute(username, attribute, value);
}


void AuthAdminServiceImpl::setAuthAttribute(const std::string& username, const std::string& attribute,
	const std::string& value)
{
	if (!isAuthAttributeName(attribute))
		throw Poco::InvalidArgumentException("Not an authentication attribute"s, attribute);

	storeUserAttribute(username, attribute, value);
}


void AuthAdminServiceImpl::storeUserAttribute(const std::string& username, const std::string& attribute,
	const std::string& value)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		replaceUserAttributeImpl(sess, username, attribute, value);
	}
	xa.commit();
}


void AuthAdminServiceImpl::setRoleAttribute(const std::string& rolename, const std::string& attribute, const std::string& value)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!roleExistsImpl(sess, rolename))
			throw Poco::NotFoundException(rolename);

		replaceRoleAttributeImpl(sess, rolename, attribute, value);
	}
	xa.commit();
}


void AuthAdminServiceImpl::removeUserAttribute(const std::string& username, const std::string& attribute)
{
	if (isAuthAttribute(attribute))
		throw Poco::InvalidArgumentException("Authentication attribute cannot be removed"s, attribute);

	deleteUserAttribute(username, attribute);
}


void AuthAdminServiceImpl::removeAuthAttribute(const std::string& username, const std::string& attribute)
{
	if (!isAuthAttributeName(attribute))
		throw Poco::InvalidArgumentException("Not an authentication attribute"s, attribute);

	deleteUserAttribute(username, attribute);
}


void AuthAdminServiceImpl::deleteUserAttribute(const std::string& username, const std::string& attribute)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		removeUserAttributeImpl(sess, username, attribute);
	}
	xa.commit();
}


void AuthAdminServiceImpl::removeUserAttributeImpl(Poco::Data::Session& sess, const std::string& username, const std::string& attribute) const
{
	if (isInlineAttribute(attribute))
	{
		sess << sqlFor("removeInlineUserAttribute"s), attribute, useRef(username), now;
	}
	else
	{
		sess << sqlFor("removeUserAttribute"s), useRef(username), useRef(attribute), now;
	}
}


void AuthAdminServiceImpl::replaceUserAttributes(const std::string& username, const std::map<std::string, std::string>& attributes)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		sess << sqlFor("removeUserAttributes"s), useRef(username), now;
		for (std::map<std::string, std::string>::const_iterator it = attributes.begin(); it != attributes.end(); ++it)
		{
			if (isAuthAttribute(it->first))
			{
				throw Poco::InvalidArgumentException("Authentication attribute cannot be replaced"s, it->first);
			}
			else if (isInlineAttribute(it->first))
			{
				sess << sqlFor("setInlineUserAttribute"s), it->first, useRef(it->second), useRef(username), now;
			}
			else
			{
				sess << sqlFor("setUserAttribute"s), useRef(username), useRef(it->first), useRef(it->second), now;
			}
		}
	}
	xa.commit();
}


void AuthAdminServiceImpl::updateUserAttributes(const std::string& username, const std::map<std::string, std::string>& attributes)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		for (std::map<std::string, std::string>::const_iterator it = attributes.begin(); it != attributes.end(); ++it)
		{
			if (isAuthAttribute(it->first))
			{
				throw Poco::InvalidArgumentException("Authentication attribute cannot be updated"s, it->first);
			}
			else
			{
				replaceUserAttributeImpl(sess, username, it->first, it->second);
			}
		}
	}
	xa.commit();
}


void AuthAdminServiceImpl::attributesForUser(const std::string& username, std::set<std::string>& attributes) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		sess << sqlFor("attributesForUser"s), useRef(username), into(attributes), now;
	}

	if (!_inlineAttributes.empty())
	{
		Poco::Data::Statement select = (sess << sqlFor("queryInlineAttributes"s), useRef(username));
		select.execute();
		Poco::Data::RecordSet rs(select);
		if (rs.moveFirst())
		{
			std::size_t cols = rs.columnCount();
			for (std::size_t col = 2; col < cols; ++col)
			{
				if (!rs[col].isEmpty() && !isAuthAttribute(rs.columnName(col)))
				{
					attributes.insert(rs.columnName(col));
				}
			}
		}
	}
	xa.commit();
}


int AuthAdminServiceImpl::attributesForUser(const std::string& username, std::map<std::string, std::string>& attributes) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	int flags = 0;
	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (!userExistsImpl(sess, username)) throw Poco::NotFoundException(username);

		flags = attributesForUserImpl(sess, username, attributes);
	}
	xa.commit();
	return flags;
}


int AuthAdminServiceImpl::attributesForUserImpl(Poco::Data::Session& sess, const std::string& username, std::map<std::string, std::string>& attributes) const
{
	int flags = 0;
	std::string attribute;
	std::string value;
	Poco::Data::Statement select = (sess << sqlFor("attributesValuesForUser"s), into(attribute), into(value), useRef(username), range(0, 1));
	while (!select.done())
	{
		select.execute();
		if (select.rowsExtracted() > 0)
		{
			if (!isAuthAttribute(attribute))
			{
				attributes[attribute] = value;
			}
			else if (attribute == USER_ATTR_LOCKOUT)
			{
				if (isInLockOutPeriod(username, value))
				{
					flags |= USER_LOCKED_OUT;
				}
			}
			else if (attribute == USER_ATTR_LDAP)
			{
				if (value == "true")
				{
					flags |= USER_EXTERNAL;
				}
			}
		}
	}

	if (!_inlineAttributes.empty())
	{
		Poco::Data::Statement inlineSelect = (sess << sqlFor("queryInlineAttributes"s), useRef(username));
		inlineSelect.execute();
		Poco::Data::RecordSet rs(inlineSelect);
		if (rs.moveFirst())
		{
			std::size_t cols = rs.columnCount();
			for (std::size_t col = 2; col < cols; ++col)
			{
				std::string columnName = rs.columnName(col);
				if (!rs[col].isEmpty())
				{
					if (!isAuthAttribute(columnName))
					{
						attributes[rs.columnName(col)] = rs[col].convert<std::string>();
					}
					else if (columnName == USER_ATTR_LOCKOUT)
					{
						if (isInLockOutPeriod(username, rs[col].convert<std::string>()))
						{
							flags |= USER_LOCKED_OUT;
						}
					}
					else if (columnName == USER_ATTR_LDAP)
					{
						if (rs[col].convert<std::string>() == "true")
						{
							flags |= USER_EXTERNAL;
						}
					}
				}
			}
		}
	}
	return flags;
}


std::vector<std::string> AuthAdminServiceImpl::findUsersByAttribute(const std::string& attribute, const std::string& value) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	std::vector<std::string> users;
	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		if (isInlineAttribute(attribute))
		{
			sess << sqlFor("usersByInlineAttribute"s), attribute, useRef(value), into(users), now;
		}
		else
		{
			sess << sqlFor("usersByAttribute"s), useRef(attribute), useRef(value), into(users), now;
		}
	}
	xa.commit();

	return users;
}


void AuthAdminServiceImpl::createOrUpdateUserImpl(Poco::Data::Session& sess, const std::string& username, const std::string& credentials) const
{
	storeCredentialsImpl(sess, username, checkPassword(username, credentials, storedHashImpl(sess, username), true));
}


void AuthAdminServiceImpl::createOrUpdateUserImpl(const std::string& username, const std::string& credentials) const
{
	const auto login = _ldapLogins.find(username);
	const std::optional<PasswordCheck> check =
		login != _ldapLogins.end() ? login->second.passwordCheck : std::nullopt;
	if (check.has_value())
	{
		storeCredentialsImpl(session(), username, *check);
	}
	else
	{
		createOrUpdateUserImpl(session(), username, credentials);
	}
}


const std::type_info& AuthAdminServiceImpl::type() const
{
	return typeid(AuthAdminServiceImpl);
}


bool AuthAdminServiceImpl::isA(const std::type_info& otherType) const
{
	return otherType == typeid(AuthAdminServiceImpl) || AuthAdminService::isA(otherType);
}


Poco::Data::Session& AuthAdminServiceImpl::session() const
{
	if (_loginSession.has_value()) return *_loginSession;

	// An expired session with an open transaction is kept, or the rest of the transaction would
	// run outside of it. A session that is not good is replaced even then: its transaction cannot
	// complete, and a failed rollback can leave the transaction flag set.
	if (_pSession == nullptr || !_pSession->isGood() || (!_pSession->isTransaction() && sessionExpired()))
	{
		_logger.debug("Creating new database session."s);
		_pSession = new Poco::Data::Session(_authParams.dbConnector, _authParams.dbConnectionString);
		setupSession(*_pSession, _properties, _authParams, _logger);
	}
	_sessionLastUsed.update();
	return *_pSession;
}


void AuthAdminServiceImpl::setupSession(Poco::Data::Session& session, const Poco::Util::AbstractConfiguration& properties, const AuthParams& authParams, Poco::Logger& logger)
{
	logger.debug("Setting up session..."s);
	std::vector<std::string> features;
	properties.keys("auth.sql.features."s + authParams.dbConnector, features);
	for (std::vector<std::string>::const_iterator it = features.begin(); it != features.end(); ++it)
	{
		std::string key(Poco::format("auth.sql.features.%s.%s"s, authParams.dbConnector, *it));
		bool enable = properties.getBool(key);
		logger.debug("Setting session feature %s to %b"s, *it, enable);
		session.setFeature(*it, enable);
	}

	std::string initStatements = properties.getString("auth.sql.init."s + authParams.dbConnector, ""s);
	Poco::StringTokenizer tok(initStatements, ";"s, Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
	for (Poco::StringTokenizer::Iterator it = tok.begin(); it != tok.end(); ++it)
	{
		logger.debug("Executing init statement: %s"s, *it);
		session << *it, now;
	}
}


bool AuthAdminServiceImpl::sessionExpired() const
{
	return _authParams.dbSessionTimeout > 0 && _sessionLastUsed.elapsed()/Poco::Timestamp::resolution() >= _authParams.dbSessionTimeout;
}


AuthAdminServiceImpl::PasswordCheck AuthAdminServiceImpl::checkPassword(const std::string& username,
	const std::string& credentials, const std::string& storedHash, bool replace) const
{
	PasswordCheck check;
	check.storedHash = storedHash;
	bool computeNewHash = true;
	if (!storedHash.empty())
	{
		int storedIterations = 0;
		const std::string salt = saltOfHash(storedHash, check.storedVersion, storedIterations);
		check.matches = equalsInConstantTime(storedHash,
			hashWithSalt(username, credentials, salt, check.storedVersion, storedIterations));
		// Versions 1 and 2 do not store the iteration count.
		const bool tooFewIterations = check.storedVersion == _authParams.hashVersion && check.storedVersion >= 3 &&
			storedIterations < pbkdf2Iterations();
		computeNewHash = check.matches ? (check.storedVersion < _authParams.hashVersion || tooFewIterations) : replace;
	}
	// Also without a stored hash (see the class documentation).
	if (computeNewHash)
	{
		check.newHash = saltAndHashCredentials(username, credentials);
	}
	return check;
}


void AuthAdminServiceImpl::changePasswordImpl(Poco::Data::Session& sess, const std::string& username, const std::string& hashedCredentials) const
{
	sess << sqlFor("changePassword"s), useRef(hashedCredentials), useRef(username), now;
}


void AuthAdminServiceImpl::replaceUserAttributeImpl(Poco::Data::Session& sess, const std::string& username, const std::string& attribute, const std::string& value) const
{
	if (isInlineAttribute(attribute))
	{
		sess << sqlFor("setInlineUserAttribute"s), attribute, useRef(value), useRef(username), now;
	}
	else
	{
		sess << sqlFor("removeUserAttribute"s), useRef(username), useRef(attribute), now;
		sess << sqlFor("setUserAttribute"s), useRef(username), useRef(attribute), useRef(value), now;
	}
}


void AuthAdminServiceImpl::replaceUserAttributeImpl(const std::string& username, const std::string& attribute, const std::string& value) const
{
	replaceUserAttributeImpl(session(), username, attribute, value);
}


void AuthAdminServiceImpl::replaceRoleAttributeImpl(Poco::Data::Session& sess, const std::string& rolename, const std::string& attribute, const std::string& value) const
{
	if (isInlineAttribute(attribute))
	{
		sess << sqlFor("setInlineRoleAttribute"s), attribute, useRef(value), useRef(rolename), now;
	}
	else
	{
		sess << sqlFor("removeRoleAttribute"s), useRef(rolename), useRef(attribute), now;
		sess << sqlFor("setRoleAttribute"s), useRef(rolename), useRef(attribute), useRef(value), now;
	}
}


void AuthAdminServiceImpl::replaceRoleAttributeImpl(const std::string& rolename, const std::string& attribute, const std::string& value) const
{
	replaceRoleAttributeImpl(session(), rolename, attribute, value);
}


bool AuthAdminServiceImpl::validateUserImpl(Poco::Data::Session& sess, const std::string& username, bool valid) const
{
	if (valid && validateUser.hasDelegates())
	{
		ValidateUserEvent ev;
		ev.username = username;
		ev.flags = attributesForUserImpl(sess, username, ev.attributes);
		ev.valid = valid;
		const_cast<AuthAdminServiceImpl*>(this)->validateUser(this, ev);
		valid = ev.valid;
	}
	return valid;
}


AuthAdminServiceImpl::PermissionsPtr AuthAdminServiceImpl::loadPermissions(Poco::Data::Session& sess, const std::string& username) const
{
	PermissionsPtr pPerms = new Permissions;
	sess << sqlFor("effectivePermissionsForUser"s), useRef(username), useRef(username), useRef(username), into(*pPerms), now;
	_permissionsCache.add(username, pPerms);
	return pPerms;
}


void AuthAdminServiceImpl::reloadPermissions(Poco::Data::Session& sess, const std::string& username) const
{
	_permissionsCache.remove(username);
	loadPermissions(sess, username);
}



void AuthAdminServiceImpl::lockOutUserIfTooManyFailures(Poco::Data::Session& sess, const std::string& username) const
{
	if (_authParams.maxFailures == 0 || !isInlineAttribute(USER_ATTR_FAILURES)) return;
	if (_authParams.neverLockOutUsers.contains(username)) return;

	sess << sqlFor("incInlineUserAttribute"s), USER_ATTR_FAILURES, useRef(username), now;
	int failures = 0;
	sess << sqlFor("getInlineUserAttribute"s), USER_ATTR_FAILURES, useRef(username), into(failures), now;

	if (failures >= _authParams.maxFailures && _authParams.lockOutTime > 0)
	{
		Poco::DateTime lockOutUntil;
		lockOutUntil += Poco::Timespan(_authParams.lockOutTime, 0);
		lockOutUserImpl(sess, username, lockOutUntil);

		_logger.notice("Locked out user %s after %d failed authentication attempts."s, username, failures);
	}
}


void AuthAdminServiceImpl::resetFailureCountImpl(Poco::Data::Session& sess, const std::string& username) const
{
	if (!isInlineAttribute(USER_ATTR_FAILURES)) return;

	Poco::Nullable<int> failures;
	sess << sqlFor("getInlineUserAttribute"s), USER_ATTR_FAILURES, useRef(username), into(failures), now;
	if (failures.value(0) != 0)
	{
		replaceUserAttributeImpl(sess, username, USER_ATTR_FAILURES, "0"s);
	}
}


void AuthAdminServiceImpl::lockOutUserImpl(Poco::Data::Session& sess, const std::string& username, const Poco::DateTime& lockOutUntil) const
{
	replaceUserAttributeImpl(sess, username, USER_ATTR_LOCKOUT, Poco::DateTimeFormatter::format(lockOutUntil, Poco::DateTimeFormat::ISO8601_FORMAT));
}


void AuthAdminServiceImpl::unlockOutUserImpl(Poco::Data::Session& sess, const std::string& username) const
{
	removeUserAttributeImpl(sess, username, USER_ATTR_LOCKOUT);
	if (isInlineAttribute(USER_ATTR_FAILURES))
	{
		replaceUserAttributeImpl(sess, username, USER_ATTR_FAILURES, "0"s);
	}
}


bool AuthAdminServiceImpl::isUserLockedOutImpl(Poco::Data::Session& sess, const std::string& username) const
{
	std::string lockout = getUserAttributeImpl(sess, username, USER_ATTR_LOCKOUT, ""s);
	return isInLockOutPeriod(username, lockout);
}


bool AuthAdminServiceImpl::isLockedOutLoginImpl(Poco::Data::Session& sess, const std::string& accountName,
	const std::string& username) const
{
	if (!isUserLockedOutImpl(sess, accountName)) return false;

	if (accountName != username)
	{
		_logger.notice("Login attempt by locked-out user %s as %s."s, accountName, username);
	}
	else
	{
		_logger.notice("Login attempt by locked-out user %s."s, username);
	}
	return true;
}


bool AuthAdminServiceImpl::isInLockOutPeriod(const std::string& username, const std::string& lockout) const
{
	if (!lockout.empty())
	{
		try
		{
			int tzd = 0;
			const Poco::DateTime lockoutDate = Poco::DateTimeParser::parse(Poco::DateTimeFormat::ISO8601_FORMAT, lockout, tzd);
			const Poco::DateTime now;
			if (lockoutDate >= now)
			{
				return true;
			}
		}
		catch (Poco::Exception& exc)
		{
			_logger.warning("The lock-out time of user %s is not valid (%s), so the user is not locked out: %s"s,
				username, lockout, exc.displayText());
		}
	}
	return false;
}


std::string AuthAdminServiceImpl::hashWithSalt(const std::string& username, const std::string& credentials,
	const std::string& salt, int version, int iterations) const
{
	if (version == 1)
	{
		return md5(salt, credentials);
	}
	else if (version == 2)
	{
		return pbkdf2(salt, credentials, version, iterations);
	}
	else if (version == 3)
	{
		std::string initialSalt = username;
		initialSalt += ":";
		initialSalt += _authParams.credentialsSalt;
		initialSalt += ":";
		std::string initialHash = md5(initialSalt, credentials);
		return pbkdf2(salt, initialHash, version, iterations);
	}
	else if (version == 4)
	{
		std::string initialSalt = username;
		initialSalt += ":";
		initialSalt += _authParams.credentialsSalt;
		initialSalt += ":";
		std::string initialHash = sha1(initialSalt, credentials);
		return pbkdf2(salt, initialHash, version, iterations);
	}
	else throw Poco::InvalidArgumentException(Poco::format("invalid hash version: %d"s, version));
}


std::string AuthAdminServiceImpl::saltAndHashCredentials(const std::string& username, const std::string& credentials) const
{
	if (_authParams.hashVersion == 1)
	{
		std::string salt = _properties.getString("auth.passwordSalt"s);
		return md5(salt, credentials);
	}
	else if (_authParams.hashVersion == 2)
	{
		std::string salt;
		for (const char c: randomBytes(static_cast<std::size_t>(std::max(_authParams.saltLength, 0))))
		{
			Poco::NumberFormatter::appendHex(salt, static_cast<unsigned>(static_cast<unsigned char>(c)), 2);
		}
		Poco::toLowerInPlace(salt);
		return pbkdf2(salt, credentials, _authParams.hashVersion, _authParams.pbkdf2Iterations);
	}
	else if (_authParams.hashVersion == 3)
	{
		std::string initialSalt = username;
		initialSalt += ":";
		initialSalt += _authParams.credentialsSalt;
		initialSalt += ":";
		std::string initialHash = md5(initialSalt, credentials);

		const std::string salt = randomBytes(static_cast<std::size_t>(std::max(_authParams.saltLength, 0)));
		return pbkdf2(salt, initialHash, _authParams.hashVersion, _authParams.pbkdf2Iterations);
	}
	else if (_authParams.hashVersion == 4)
	{
		std::string initialSalt = username;
		initialSalt += ":";
		initialSalt += _authParams.credentialsSalt;
		initialSalt += ":";
		std::string initialHash = sha1(initialSalt, credentials);

		int saltLength = _authParams.saltLength;
		if (saltLength < V4_MIN_SALT_LENGTH) saltLength = V4_MIN_SALT_LENGTH;

		const std::string salt = randomBytes(static_cast<std::size_t>(saltLength));
		return pbkdf2(salt, initialHash, _authParams.hashVersion, pbkdf2Iterations());
	}
	else throw Poco::InvalidArgumentException("invalid hash version"s);
}


int AuthAdminServiceImpl::pbkdf2Iterations() const
{
	if (_authParams.hashVersion == 4 && _authParams.pbkdf2Iterations < V4_MIN_ITERATIONS) return V4_MIN_ITERATIONS;
	return _authParams.pbkdf2Iterations;
}


std::string AuthAdminServiceImpl::hashCachedCredentials(const std::string& username, const std::string& credentials) const
{
	std::string salt = username;
	salt += ':';
	salt += _cacheSalt;
	// Note: Cached hashes are stored temporarily in RAM only, so we go with a
	// very low number of rounds and SHA1 for better performance over security.
	// If an attacker gets access to the RAM, there'll be bigger issues to deal with.
	Poco::PBKDF2Engine<Poco::HMACEngine<SHA1Engine>> pbkdf2(salt, 16);
	pbkdf2.update(credentials);
	return digestToBinaryString(pbkdf2);
}


std::string AuthAdminServiceImpl::md5(const std::string& salt, const std::string& credentials)
{
	Poco::MD5Engine md5;
	md5.update(salt);
	md5.update(credentials);
	return digestToHexString(md5);
}


std::string AuthAdminServiceImpl::sha1(const std::string& salt, const std::string& credentials)
{
	Poco::SHA1Engine sha1;
	sha1.update(salt);
	sha1.update(credentials);
	return digestToHexString(sha1);
}


std::string AuthAdminServiceImpl::pbkdf2(const std::string& salt, const std::string& credentials, int version, int iterations)
{
	std::string hash;
	if (version == 2)
	{
		Poco::PBKDF2Engine<Poco::HMACEngine<Poco::SHA1Engine>> pbkdf2(salt, iterations);
		pbkdf2.update(credentials);
		hash += "v=2;s=";
		hash += salt;
		hash += ";h=";
		hash += digestToHexString(pbkdf2);
	}
	else if (version == 3)
	{
		Poco::PBKDF2Engine<Poco::HMACEngine<Poco::SHA1Engine>> pbkdf2(salt, iterations);
		pbkdf2.update(credentials);
		hash += "v=3;s=";
		hash += encodeBase64(salt);
		hash += ";h=";
		hash += digestToBase64(pbkdf2);
		hash += ";i=";
		hash += Poco::NumberFormatter::format(iterations);
	}
	else if (version == 4)
	{
		Poco::PBKDF2Engine<Poco::HMACEngine<Poco::SHA2Engine256>> pbkdf2(salt, iterations);
		pbkdf2.update(credentials);
		hash += "v=4;s=";
		hash += encodeBase64(salt);
		hash += ";h=";
		hash += digestToBase64(pbkdf2);
		hash += ";i=";
		hash += Poco::NumberFormatter::format(iterations);
	}
	return hash;
}


std::map<std::string, std::string> AuthAdminServiceImpl::parseKeyValueMap(const std::string& s)
{
	std::map<std::string, std::string> kvm;
	std::string::const_iterator it = s.begin();
	std::string::const_iterator end = s.end();
	while (it != end)
	{
		std::string k;
		std::string v;
		while (it != end && *it != '=') k += *it++;
		if (it != end) ++it;
		while (it != end && *it != ';') v += *it++;
		if (it != end) ++it;
		kvm[k] = v;
	}
	return kvm;
}


std::string AuthAdminServiceImpl::sqlFor(const std::string& action) const
{
	auto it = _sqlCache.find(action);
	if (it == _sqlCache.end())
	{
		std::string key("auth.sql.");
		key += action;
		std::string clause = _properties.getString(key);
		std::string fixedClause = fixPlaceholders(clause);
		_sqlCache[action] = fixedClause;
		return fixedClause;
	}
	else return it->second;
}


std::string AuthAdminServiceImpl::hashForUser(Poco::Data::Session& sess, const std::string& username) const
{
	std::string hash;
	Poco::SharedPtr<std::string> pCachedPass = _passwordPersistentHashCache.get(username);
	if (pCachedPass != nullptr)
	{
		hash = *pCachedPass;
	}
	else
	{
		hash = storedHashImpl(sess, username);
		if (!hash.empty())
		{
			_passwordPersistentHashCache.update(username, hash);
		}
	}
	if (hash.compare(0, 4, "v=3;") == 0 || hash.compare(0, 4, "v=4;") == 0)
	{
		std::map<std::string, std::string> kvm = parseKeyValueMap(hash);
		return decodeBase64(kvm["h"s]);
	}
	else if (hash.compare(0, 4, "v=2;") == 0)
	{
		std::map<std::string, std::string> kvm = parseKeyValueMap(hash);
		return kvm["h"s];
	}
	else
	{
		return hash;
	}
}


std::string AuthAdminServiceImpl::saltForUser(Poco::Data::Session& sess, const std::string& username, int& version, int& iterations) const
{
	std::string hash;
	Poco::SharedPtr<std::string> pCachedPass = _passwordPersistentHashCache.get(username);
	if (pCachedPass != nullptr)
	{
		hash = *pCachedPass;
	}
	else
	{
		hash = storedHashImpl(sess, username);
		if (!hash.empty())
		{
			_passwordPersistentHashCache.update(username, hash);
		}
	}
	return saltOfHash(hash, version, iterations);
}


std::string AuthAdminServiceImpl::saltOfHash(const std::string& hash, int& version, int& iterations) const
{
	if (hash.compare(0, 4, "v=3;") == 0 || hash.compare(0, 4, "v=4;") == 0)
	{
		std::map<std::string, std::string> kvm = parseKeyValueMap(hash);
		version = Poco::NumberParser::parse(kvm["v"s]);
		iterations = Poco::NumberParser::parse(kvm["i"s]);
		return decodeBase64(kvm["s"s]);
	}
	else if (hash.compare(0, 4, "v=2;") == 0)
	{
		std::map<std::string, std::string> kvm = parseKeyValueMap(hash);
		version = Poco::NumberParser::parse(kvm["v"s]);
		iterations = _authParams.pbkdf2Iterations;
		return kvm["s"s];
	}
	else
	{
		version = 1;
		iterations = 0;
		return _properties.getString("auth.passwordSalt"s);
	}
}


std::string AuthAdminServiceImpl::translatePattern(const std::string& pattern)
{
	std::string sqlPattern;
	std::string::const_iterator it = pattern.begin();
	std::string::const_iterator end = pattern.end();
	while (it != end)
	{
		switch (*it)
		{
		case '?':
			sqlPattern += '_';
			break;
		case '*':
			sqlPattern += '%';
			break;
		case '!':
		case '%':
		case '_':
			sqlPattern += '!';
			sqlPattern += *it;
			break;
		default:
			sqlPattern += *it;
		}
		++it;
	}
	return sqlPattern;
}


void AuthAdminServiceImpl::discoverInlineAttributes()
{
	Poco::Data::Session& sess = session();
	Poco::Data::Transaction xa(sess, &_logger);
	{
		Poco::Data::Statement select(sess);
		select << sqlFor("discoverInlineAttributes"s), range(0, 1);
		select.execute();
		Poco::Data::RecordSet rs(select);
		// First two columns are username and (hashed) password.
		std::size_t cols = rs.columnCount();
		for (std::size_t col = 2; col < cols; ++col)
		{
			_inlineAttributes.insert(rs.columnName(col));
		}
	}
	xa.commit();
}


StatementPlaceholderProvider::Ptr AuthAdminServiceImpl::statementPlaceholderProvider() const
{
	if (Poco::icompare(_authParams.dbConnector, "postgresql"s) == 0)
		return std::make_unique<PostgresStatementPlaceholderProvider>();
	else if (Poco::icompare(_authParams.dbConnector, "mysql"s) == 0)
		return std::make_unique<MySQLStatementPlaceholderProvider>();
	else
		return std::make_unique<DefaultStatementPlaceholderProvider>();
}


std::string AuthAdminServiceImpl::fixPlaceholders(const std::string& clause) const
{
	auto pSPP = statementPlaceholderProvider();
	const char quote = pSPP->quote();

	std::string result;
	auto it = clause.begin();
	auto end = clause.end();
	while (it != end)
	{
		switch (*it)
		{
		case '"':
			result += quote;
			it++;
			while (it != end && *it != '"') result += *it++;
			if (it != end) { result += quote; it++; }
			break;

		case '\'':
			result += *it++;
			while (it != end && *it != '\'') result += *it++;
			if (it != end) result += *it++;
			break;

		case '?':
			result += pSPP->next();
			++it;
			break;

		default:
			result += *it++;
			break;
		}
	}
	return result;
}


std::string AuthAdminServiceImpl::generateCacheSalt()
{
	constexpr std::size_t CACHE_SALT_LENGTH = 8;
	return randomBytes(CACHE_SALT_LENGTH);
}


} // namespace Poco::OSP::Auth::Data
