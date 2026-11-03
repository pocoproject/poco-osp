//
// SimpleAuthService.cpp
//
// Copyright (c) 2007, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/AutoPtr.h"
#include "Poco/StringTokenizer.h"
#include "Poco/ClassLibrary.h"
#include "Poco/DigestEngine.h"
#include "Poco/SHA2Engine.h"
#include <set>
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
	/// A very simple implementation of AuthService, for testing on
	/// this computer only. The SimpleAuth bundle
	/// (com.appinf.osp.simple-auth) provides the same service with
	/// hashed passwords in the configuration.
	///
	/// Only two users are known - "user" and "admin".
	/// Their passwords are set in the global configuration file with
	/// the "adminPassword" and "userPassword" properties. Without
	/// "adminPassword", the service is not registered. Without
	/// "userPassword", "user" cannot log in.
	/// The "admin" user has all permissions. The set of permissions
	/// for "user" can be set in the global configuration file,
	/// using the "userPermissions" property. The permissions are
	/// specified as a comma-separated list; by default, "user" has
	/// no permissions.
{
public:
	SimpleAuthService(const std::string& adminPassword, const std::string& userPassword, const std::set<std::string>& userPermissions):
		_adminPasswordHash(hash(adminPassword)),
		_userPasswordHash(userPassword.empty() ? Poco::DigestEngine::Digest() : hash(userPassword)),
		_userPermissions(userPermissions)
	{
	}

	~SimpleAuthService() override = default;

	// AuthService
	[[nodiscard]] bool authenticate(const std::string& userName, const std::string& credentials) const override
	{
		if (userName == "admin")
		{
			return Poco::DigestEngine::constantTimeEquals(hash(credentials), _adminPasswordHash);
		}
		if (userName == "user")
		{
			return !_userPasswordHash.empty() && Poco::DigestEngine::constantTimeEquals(hash(credentials), _userPasswordHash);
		}
		return false;
	}

	[[nodiscard]] bool authorize(const std::string& userName, const std::string& permission) const override
	{
		return userName == "admin" || (userName == "user" && _userPermissions.find(permission) != _userPermissions.end());
	}

	[[nodiscard]] bool authorize(const std::string& userName, const std::string& roleOrScope,
		const std::string& permission) const override
	{
		return authorize(userName, permission);
	}

	[[nodiscard]] bool userExists(const std::string& userName) const override
	{
		return userName == "admin" || userName == "user";
	}

	// Service
	[[nodiscard]] const std::type_info& type() const override
	{
		return typeid(SimpleAuthService);
	}

	[[nodiscard]] bool isA(const std::type_info& otherType) const override
	{
		std::string name(typeid(SimpleAuthService).name());
		return name == otherType.name() || Poco::OSP::Auth::AuthService::isA(otherType);
	}

private:
	[[nodiscard]] static Poco::DigestEngine::Digest hash(const std::string& password)
	{
		Poco::SHA2Engine256 sha256;
		sha256.update(password);
		return sha256.digest();
	}

	Poco::DigestEngine::Digest _adminPasswordHash;
	Poco::DigestEngine::Digest _userPasswordHash;
	std::set<std::string> _userPermissions;
};


class SimpleAuthServiceBundleActivator: public BundleActivator
	/// The BundleActivator for the SimpleAuthService.
	/// Registers the SimpleAuthService with the Service Registry.
{
public:
	SimpleAuthServiceBundleActivator() = default;

	~SimpleAuthServiceBundleActivator() override = default;

	void start(BundleContext::Ptr pContext) override
	{
		std::string adminPassword = pContext->thisBundle()->properties().getString("adminPassword", "");
		std::string userPassword = pContext->thisBundle()->properties().getString("userPassword", "");
		std::string perms = pContext->thisBundle()->properties().getString("userPermissions", "");
		std::set<std::string> userPermissions;

		// Find Preferences service
		ServiceRef::Ptr pPrefsRef = pContext->registry().findByName(PreferencesService::SERVICE_NAME);
		AutoPtr<PreferencesService> pPrefs = pPrefsRef->castedInstance<PreferencesService>();

		// Get passwords and user permissions from global config
		adminPassword = pPrefs->configuration()->getString("adminPassword", adminPassword);
		userPassword = pPrefs->configuration()->getString("userPassword", userPassword);
		perms = pPrefs->configuration()->getString("userPermissions", perms);
		if (adminPassword.empty())
		{
			pContext->logger().error("The sample authentication service is not registered: no password is configured for the user admin (property adminPassword).");
			return;
		}
		StringTokenizer tok(perms, ",;", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
		for (StringTokenizer::Iterator it = tok.begin(); it != tok.end(); ++it)
		{
			userPermissions.insert(*it);
		}

		// Create an instance of the SimpleAuthService
		AutoPtr<SimpleAuthService> pService = new SimpleAuthService(adminPassword, userPassword, userPermissions);
		// Register the SimpleAuthService with the ServiceRegistry.
		_pService = pContext->registry().registerService("osp.auth", pService, Properties());
	}

	void stop(BundleContext::Ptr pContext) override
	{
		// Unregister the SimpleAuthService
		pContext->registry().unregisterService(_pService);
		// The activator is reused when the bundle starts again, and that
		// start registers no service if adminPassword has been removed.
		_pService.reset();
	}

private:
	ServiceRef::Ptr _pService;
};


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(SimpleAuthServiceBundleActivator)
POCO_END_MANIFEST
