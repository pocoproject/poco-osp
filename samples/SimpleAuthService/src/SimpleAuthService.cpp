//
// SimpleAuthService.cpp
//
// Copyright (c) 2007, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
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
using Poco::OSP::Properties;
using Poco::OSP::ServiceRef;
using Poco::OSP::PreferencesService;
using Poco::AutoPtr;
using Poco::StringTokenizer;


namespace
{
	class SimpleAuthService: public Poco::OSP::Auth::AuthService
		/// An AuthService for testing on this computer only; the SimpleAuth bundle provides the service with
		/// hashed passwords. It knows the users "admin", with all permissions, and "user", with the given
		/// permissions; without a password, "user" cannot log in. Denies every check restricted to a role or scope.
	{
	public:
		SimpleAuthService(const std::string& adminPassword, const std::string& userPassword,
			const std::set<std::string>& userPermissions):
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
				return !_userPasswordHash.empty()
					&& Poco::DigestEngine::constantTimeEquals(hash(credentials), _userPasswordHash);
			}
			return false;
		}

		[[nodiscard]] bool authorize(const std::string& userName, const std::string& permission) const override
		{
			return userName == "admin"
				|| (userName == "user" && _userPermissions.contains(permission));
		}

		[[nodiscard]] bool authorize(const std::string& userName, const std::string& roleOrScope,
			const std::string& permission) const override
		{
			return roleOrScope.empty() && authorize(userName, permission);
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
		/// Registers the SimpleAuthService with the "adminPassword", "userPassword" and "userPermissions"
		/// (a comma-separated list) of the global configuration. Without "adminPassword", registers nothing.
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

			ServiceRef::Ptr pPrefsRef = pContext->registry().findByName(PreferencesService::SERVICE_NAME);
			if (pPrefsRef == nullptr)
			{
				pContext->logger().error(
					"The sample authentication service is not registered: the preferences service is not available.");
				return;
			}
			AutoPtr<PreferencesService> pPrefs = pPrefsRef->castedInstance<PreferencesService>();

			adminPassword = pPrefs->configuration()->getString("adminPassword", adminPassword);
			userPassword = pPrefs->configuration()->getString("userPassword", userPassword);
			perms = pPrefs->configuration()->getString("userPermissions", perms);
			if (adminPassword.empty())
			{
				pContext->logger().error("The sample authentication service is not registered: "
					"no password is configured for the user admin (property adminPassword).");
				return;
			}
			StringTokenizer tok(perms, ",;", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
			for (StringTokenizer::Iterator it = tok.begin(); it != tok.end(); ++it)
			{
				userPermissions.insert(*it);
			}

			AutoPtr<SimpleAuthService> pService = new SimpleAuthService(adminPassword, userPassword, userPermissions);
			_pService = pContext->registry().registerService("osp.auth", pService, Properties());
		}

		void stop(BundleContext::Ptr pContext) override
		{
			pContext->registry().unregisterService(_pService);
			// The activator is reused, and the next start may register nothing.
			_pService.reset();
		}

	private:
		ServiceRef::Ptr _pService;
	};
}


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(SimpleAuthServiceBundleActivator)
POCO_END_MANIFEST
