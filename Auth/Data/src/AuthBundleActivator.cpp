//
// AuthBundleActivator.cpp
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/Auth/Data/AuthAdminServiceImpl.h"
#include "Poco/OSP/Auth/PooledAuthAdminService.h"
#include "Poco/StringTokenizer.h"
#include "Poco/ClassLibrary.h"


using Poco::OSP::BundleActivator;
using Poco::OSP::BundleContext;
using Poco::OSP::Bundle;
using Poco::OSP::Service;
using Poco::OSP::ServiceRef;
using Poco::OSP::PreferencesService;
using Poco::OSP::Properties;
using Poco::OSP::Auth::AuthAdminService;
using namespace std::string_literals;


namespace Poco {
namespace OSP {
namespace Auth {
namespace Data {


class AuthBundleActivator: public BundleActivator
	/// The BundleActivator for the Auth Bundle.
	///
	/// Registers the AuthAdminServiceImpl service.
{
public:
	AuthBundleActivator()
	{
	}

	~AuthBundleActivator()
	{
	}

	void start(BundleContext::Ptr pContext)
	{
		AuthAdminServiceImpl::AuthParams authParams;
		AuthAdminServiceImpl::LDAPParams ldapParams;
		ldapParams.timeout = 30;

		ServiceRef::Ptr pPrefsSvcRef = pContext->registry().findByName("osp.core.preferences"s);
		int databaseStartupTimeout = 10;
		int poolSize = 1;
		if (pPrefsSvcRef)
		{
			PreferencesService::Ptr pPrefsSvc = pPrefsSvcRef->castedInstance<PreferencesService>();

			authParams.dbConnector         = pPrefsSvc->configuration()->getString("auth.db.connector"s, ""s);
			authParams.dbConnectionString  = pPrefsSvc->configuration()->getString("auth.db.connectionString"s, ""s);
			authParams.dbSessionTimeout    = pPrefsSvc->configuration()->getInt("auth.db.sessionTimeout"s, 0);
			authParams.cacheSize           = pPrefsSvc->configuration()->getInt("auth.cacheSize"s, pContext->thisBundle()->properties().getInt("auth.cacheSize"s, 1024));
			authParams.cacheTimeout        = pPrefsSvc->configuration()->getInt("auth.cacheTimeout"s, pContext->thisBundle()->properties().getInt("auth.cacheTimeout"s, 600));
			authParams.hashVersion         = pPrefsSvc->configuration()->getInt("auth.hashVersion"s, pContext->thisBundle()->properties().getInt("auth.hashVersion"s, 4));
			authParams.saltLength          = pPrefsSvc->configuration()->getInt("auth.saltLength"s, pContext->thisBundle()->properties().getInt("auth.saltLength"s, 7));
			authParams.pbkdf2Iterations    = pPrefsSvc->configuration()->getInt("auth.pbkdf2Rounds"s, pContext->thisBundle()->properties().getInt("auth.pbkdf2Rounds"s, 1024));
			authParams.credentialsSalt     = pPrefsSvc->configuration()->getString("auth.credentialsSalt"s, "poco"s);
			authParams.maxFailures         = pPrefsSvc->configuration()->getInt("auth.maxFailures"s, 0);
			authParams.lockOutTime         = pPrefsSvc->configuration()->getInt("auth.lockOutTime"s, 600);

			std::string neverLockOutUsers = pPrefsSvc->configuration()->getString("auth.neverLockOutUsers"s, ""s);
			Poco::StringTokenizer nlouTok(neverLockOutUsers, ",;"s, Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
			authParams.neverLockOutUsers.insert(nlouTok.begin(), nlouTok.end());

			ldapParams.uri                 = pPrefsSvc->configuration()->getString("auth.ldap.uri"s, ""s);
			ldapParams.bindDN              = pPrefsSvc->configuration()->getString("auth.ldap.bindDN"s, ""s);
			ldapParams.bindUser            = pPrefsSvc->configuration()->getString("auth.ldap.bindUser"s, ""s);
			ldapParams.bindPassword        = pPrefsSvc->configuration()->getString("auth.ldap.bindPassword"s, ""s);
			ldapParams.bindMode            = pPrefsSvc->configuration()->getString("auth.ldap.bindMode"s, ""s);
			ldapParams.searchBase          = pPrefsSvc->configuration()->getString("auth.ldap.searchBase"s, ldapParams.bindDN);
			ldapParams.userObjectClass     = pPrefsSvc->configuration()->getString("auth.ldap.userObjectClass"s, "user"s);
			ldapParams.groupObjectClass    = pPrefsSvc->configuration()->getString("auth.ldap.groupObjectClass"s, "group"s);
			ldapParams.authMode            = pPrefsSvc->configuration()->getString("auth.ldap.authMode"s, "DN"s);
			ldapParams.timeout             = pPrefsSvc->configuration()->getInt("auth.ldap.timeout"s, 30);
			ldapParams.permissionAttribute = pPrefsSvc->configuration()->getString("auth.ldap.permissionAttribute"s, ""s);
			ldapParams.rolePermissionAttribute = pPrefsSvc->configuration()->getString("auth.ldap.rolePermissionAttribute"s, ""s);
			ldapParams.roleAttribute       = pPrefsSvc->configuration()->getString("auth.ldap.roleAttribute"s, ""s);
			ldapParams.localUsers          = pPrefsSvc->configuration()->getString("auth.ldap.localUsers"s, ""s);
			ldapParams.localRoles          = pPrefsSvc->configuration()->getString("auth.ldap.localRoles"s, ""s);
			ldapParams.roleFilter          = pPrefsSvc->configuration()->getString("auth.ldap.roleFilter"s, ""s);
			ldapParams.localFallback       = pPrefsSvc->configuration()->getBool("auth.ldap.inaccessible.fallbackToLocal"s, false);
			ldapParams.uNameLifeDaysDays   = pPrefsSvc->configuration()->getInt("auth.ldap.cache.lifetimeDays"s, 7);

			std::string extraAttributes    = pPrefsSvc->configuration()->getString("auth.ldap.extraAttributes"s, ""s);
			Poco::StringTokenizer attrTok(extraAttributes, ",;", Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
			for (const auto& attr: attrTok)
			{
				auto pos = attr.find(':');
				if (pos != std::string::npos)
				{
					std::string ldapAttr(attr, 0, pos);
					std::string userAttr(attr, pos + 1);
					ldapParams.extraAttributes.push_back({ldapAttr, userAttr});
				}
				else
				{
					ldapParams.extraAttributes.push_back({attr, attr});
				}
			}

			databaseStartupTimeout = pPrefsSvc->configuration()->getInt("auth.db.databaseStartupTimeout"s, 10);
			poolSize = pPrefsSvc->configuration()->getInt("auth.poolSize"s, 1);
		}

		if (!authParams.dbConnector.empty())
		{
			pContext->logger().information("Checking database connection (%s)..."s, authParams.dbConnector);
			int time = 0;
			bool databaseOK = AuthAdminServiceImpl::checkDatabaseConnectivity(pContext->thisBundle()->properties(), authParams, pContext->logger());
			while (!databaseOK && time <= databaseStartupTimeout)
			{
				pContext->logger().warning("Database connection failure. Retrying..."s);
				Poco::Thread::sleep(2000);
				time += 2;
				databaseOK = AuthAdminServiceImpl::checkDatabaseConnectivity(pContext->thisBundle()->properties(), authParams, pContext->logger());
			}
			if (databaseOK)
			{
				pContext->logger().information("Database connection OK."s);
			}
			else
			{
				pContext->logger().error("Database connection failed. Service will not be available."s);
				return;
			}

			AuthAdminService::Ptr pAuthAdminService;
			if (poolSize > 1)
			{
				pContext->logger().information("Creating %d backends for PooledAuthAdminService."s, poolSize);
				std::vector<AuthAdminService::Ptr> backends;
				for (int i = 0; i < poolSize; i++)
				{
					AuthAdminService::Ptr pBackend = new AuthAdminServiceImpl(pContext->thisBundle()->properties(), pContext->logger(), authParams, ldapParams);
					backends.push_back(pBackend);
				}
				pAuthAdminService = new PooledAuthAdminService(backends);
			}
			else
			{
				pAuthAdminService = new AuthAdminServiceImpl(pContext->thisBundle()->properties(), pContext->logger(), authParams, ldapParams);
			}
			_pAuthAdminServiceRef = pContext->registry().registerService(AuthAdminService::SERVICE_NAME, pAuthAdminService, Properties());
			pContext->logger().information("AuthAdminService registered."s);
		}
		else
		{
			pContext->logger().notice("No configuration for AuthAdminService found. Service will not be available."s);
		}
	}

	void stop(BundleContext::Ptr pContext)
	{
		if (_pAuthAdminServiceRef)
		{
			pContext->registry().unregisterService(_pAuthAdminServiceRef);
			_pAuthAdminServiceRef = 0;
		}
	}

private:
	ServiceRef::Ptr _pAuthAdminServiceRef;
};


} } } } // namespace Poco::OSP::Auth::Data


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(Poco::OSP::Auth::Data::AuthBundleActivator)
POCO_END_MANIFEST
