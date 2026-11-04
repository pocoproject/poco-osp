//
// SimpleAuth.h
//
// Definition of the SimpleAuthService and SimpleAuthBundleActivator classes.
//
// Copyright (c) 2007-2015, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef SimpleAuth_SimpleAuth_INCLUDED
#define SimpleAuth_SimpleAuth_INCLUDED


#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/AutoPtr.h"
#include "Poco/StringTokenizer.h"
#include "Poco/MD5Engine.h"
#include "Poco/SHA1Engine.h"
#include "Poco/SHA2Engine.h"
#include "Poco/HMACEngine.h"
#include "Poco/PBKDF2Engine.h"
#include "Poco/Base64Decoder.h"
#include "Poco/Base64Encoder.h"
#include "Poco/NumberParser.h"
#include "Poco/StreamCopier.h"
#include <map>
#include <set>
#include <sstream>
#include <string>


using Poco::OSP::BundleActivator;
using Poco::OSP::BundleContext;
using Poco::OSP::Bundle;
using Poco::OSP::Properties;
using Poco::OSP::Service;
using Poco::OSP::ServiceRef;
using Poco::OSP::PreferencesService;
using Poco::AutoPtr;
using Poco::StringTokenizer;


class SimpleAuthService: public Poco::OSP::Auth::AuthService
	/// A very simple implementation of AuthService.
	///
	/// Only two users are known - "user" and "admin". The
	/// names of these two users can be changed in the global
	/// configuration using the "auth.simple.admin.name" and
	/// "auth.simple.user.name" properties and default to
	/// "admin" and "user", respectively.
	///
	/// The password for "admin" and "user" can be set in the global
	/// application configuration file as salted MD5 hashes with the
	/// "auth.simple.admin.passwordHash" and "auth.simple.user.passwordHash"
	/// properties. The (optional) salt can be specified with
	/// the "auth.simple.salt" property.
	///
	/// A password hash can also be given in the version 4 format of the
	/// OSP Auth Data bundle ("v=4;s=...;h=...;i=...", PBKDF2 with HMAC-SHA256),
	/// computed with "auth.simple.salt" as the credentials salt
	/// ("auth.credentialsSalt" of the Auth Data bundle).
	///
	/// The "admin" user has all permissions. The set of permissions
	/// for "user" can be set in the global configuration file,
	/// using the "auth.simple.user.permissions" property. The permissions are
	/// specified as a comma-separated list.
	///
	/// SimpleAuth has no scopes: authorize() with a role or scope returns false.
{
public:
	SimpleAuthService(const std::string& adminName, const std::string& adminPasswordHash, const std::string& userName,
		const std::string& userPasswordHash, const std::set<std::string>& userPermissions, const std::string& salt):
		_adminName(adminName),
		_adminPasswordHash(adminPasswordHash),
		_userName(userName),
		_userPasswordHash(userPasswordHash),
		_userPermissions(userPermissions),
		_salt(salt)
	{
	}

	~SimpleAuthService() override = default;

	SimpleAuthService() = delete;
	SimpleAuthService(const SimpleAuthService&) = delete;
	SimpleAuthService(SimpleAuthService&&) = delete;
	SimpleAuthService& operator = (const SimpleAuthService&) = delete;
	SimpleAuthService& operator = (SimpleAuthService&&) = delete;

	// AuthService
	bool authenticate(const std::string& userName, const std::string& credentials) const override
	{
		return (userName == _userName && checkCredentials(userName, credentials, _userPasswordHash))
		    || (userName == _adminName && checkCredentials(userName, credentials, _adminPasswordHash));
	}

	bool authorize(const std::string& userName, const std::string& permission) const override
	{
		return userName == _adminName
		    || (userName == _userName && _userPermissions.find(permission) != _userPermissions.end());
	}

	bool authorize(const std::string& userName, const std::string& roleOrScope,
		const std::string& permission) const override;

	bool userExists(const std::string& userName) const override
	{
		return userName == _adminName || userName == _userName;
	}

	// Service
	const std::type_info& type() const override
	{
		return typeid(SimpleAuthService);
	}

	bool isA(const std::type_info& otherType) const override
	{
		std::string name(typeid(SimpleAuthService).name());
		return name == otherType.name() || Poco::OSP::Auth::AuthService::isA(otherType);
	}

protected:
	[[nodiscard]] bool checkCredentials(const std::string& userName, const std::string& credentials,
		const std::string& passwordHash) const;

	std::string hashCredentials(const std::string& credentials) const
	{
		Poco::MD5Engine md5;
		md5.update(_salt);
		md5.update(credentials);
		return Poco::DigestEngine::digestToHex(md5.digest());
	}

	[[nodiscard]] std::string hashCredentialsV4(const std::string& userName, const std::string& credentials,
		const std::string& passwordHash) const;
		/// Returns the hash of the credentials in the version 4 format, with
		/// the salt and the iteration count of the given hash in that format.

	static inline const std::string V4_PREFIX{"v=4;"};

private:
	std::string _adminName;
	std::string _adminPasswordHash;
	std::string _userName;
	std::string _userPasswordHash;
	std::set<std::string> _userPermissions;
	std::string _salt;
};


inline bool SimpleAuthService::authorize(const std::string& userName, const std::string& roleOrScope,
	const std::string& permission) const
{
	return roleOrScope.empty() && authorize(userName, permission);
}


inline bool SimpleAuthService::checkCredentials(const std::string& userName, const std::string& credentials,
	const std::string& passwordHash) const
{
	const std::string hash = passwordHash.compare(0, V4_PREFIX.size(), V4_PREFIX) == 0 ?
		hashCredentialsV4(userName, credentials, passwordHash) : hashCredentials(credentials);
	return Poco::DigestEngine::constantTimeEquals(Poco::DigestEngine::Digest(hash.begin(), hash.end()),
		Poco::DigestEngine::Digest(passwordHash.begin(), passwordHash.end()));
}


inline std::string SimpleAuthService::hashCredentialsV4(const std::string& userName, const std::string& credentials,
	const std::string& passwordHash) const
{
	std::map<std::string, std::string> fields;
	const StringTokenizer tok(passwordHash, ";", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
	for (const auto& field: tok)
	{
		const auto pos = field.find('=');
		if (pos != std::string::npos) fields[field.substr(0, pos)] = field.substr(pos + 1);
	}
	unsigned iterations = 0;
	if (!Poco::NumberParser::tryParseUnsigned(fields["i"], iterations) || iterations == 0) return std::string();

	std::string salt;
	std::istringstream saltStream(fields["s"]);
	Poco::Base64Decoder decoder(saltStream);
	Poco::StreamCopier::copyToString(decoder, salt);

	Poco::SHA1Engine sha1;
	sha1.update(userName + ":" + _salt + ":");
	sha1.update(credentials);
	Poco::PBKDF2Engine<Poco::HMACEngine<Poco::SHA2Engine256>> pbkdf2(salt, iterations);
	pbkdf2.update(Poco::DigestEngine::digestToHex(sha1.digest()));
	const Poco::DigestEngine::Digest digest = pbkdf2.digest();

	std::ostringstream hashStream;
	hashStream << V4_PREFIX << "s=" << fields["s"] << ";h=";
	Poco::Base64Encoder encoder(hashStream);
	encoder.rdbuf()->setLineLength(0);
	encoder << std::string(digest.begin(), digest.end());
	encoder.close();
	hashStream << ";i=" << iterations;
	return hashStream.str();
}


class SimpleAuthBundleActivator: public BundleActivator
	/// The BundleActivator for the SimpleAuth.
	/// Registes the SimpleAuthService with the ServiceRegistry.
{
public:
	SimpleAuthBundleActivator() = default;
	~SimpleAuthBundleActivator() override = default;

	SimpleAuthBundleActivator(const SimpleAuthBundleActivator&) = delete;
	SimpleAuthBundleActivator(SimpleAuthBundleActivator&&) = delete;
	SimpleAuthBundleActivator& operator = (const SimpleAuthBundleActivator&) = delete;
	SimpleAuthBundleActivator& operator = (SimpleAuthBundleActivator&&) = delete;

	void start(BundleContext::Ptr pContext) override
	{
		ServiceRef::Ptr pPrefsRef = pContext->registry().findByName(PreferencesService::SERVICE_NAME);
		if (pPrefsRef == nullptr)
		{
			pContext->logger().error("The preferences service is not available, so the simple authentication service "
				"is not started.");
			return;
		}
		AutoPtr<PreferencesService> pPrefs = pPrefsRef->castedInstance<PreferencesService>();

		bool enable = pPrefs->configuration()->getBool("auth.simple.enable", true);
		if (enable)
		{
			std::string adminName = pPrefs->configuration()->getString("auth.simple.admin.name", "admin");
			std::string adminPasswordHash = pPrefs->configuration()->getString("auth.simple.admin.passwordHash", "");
			std::string userName = pPrefs->configuration()->getString("auth.simple.user.name", "user");
			std::string userPasswordHash = pPrefs->configuration()->getString("auth.simple.user.passwordHash", "");
			std::string salt = pPrefs->configuration()->getString("auth.simple.salt", "");
			std::string perms = pPrefs->configuration()->getString("auth.simple.user.permissions", "");
			StringTokenizer tok(perms, ",;", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
			std::set<std::string> userPermissions;
			for (StringTokenizer::Iterator it = tok.begin(); it != tok.end(); ++it)
			{
				userPermissions.insert(*it);
			}

			AutoPtr<SimpleAuthService> pService = new SimpleAuthService(adminName, adminPasswordHash, userName,
				userPasswordHash, userPermissions, salt);
			_pService = pContext->registry().registerService("osp.auth", pService, Properties());
		}
		else
		{
			pContext->logger().information("Simple authentication service disabled.");
		}
	}

	void stop(BundleContext::Ptr pContext) override
	{
		if (_pService != nullptr)
		{
			pContext->registry().unregisterService(_pService);
		}
	}

private:
	ServiceRef::Ptr _pService;
};


#endif // SimpleAuth_SimpleAuth_INCLUDED
