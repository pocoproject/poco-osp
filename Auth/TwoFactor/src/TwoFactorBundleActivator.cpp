//
// TwoFactorBundleActivator.cpp
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/Auth/TwoFactor/TwoFactorServiceImpl.h"
#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/StringTokenizer.h"
#include "Poco/ClassLibrary.h"


using Poco::OSP::BundleActivator;
using Poco::OSP::BundleContext;
using Poco::OSP::Bundle;
using Poco::OSP::Service;
using Poco::OSP::ServiceRef;
using Poco::OSP::PreferencesService;
using Poco::OSP::Properties;
using namespace std::string_literals;


namespace Poco {
namespace OSP {
namespace Auth {
namespace TwoFactor {


class TwoFactorBundleActivator: public BundleActivator
	/// The BundleActivator for the Auth TwoFactor Bundle.
	///
	/// Registers the TwoFactorServiceImpl service.
{
public:
	TwoFactorBundleActivator() = default;
	~TwoFactorBundleActivator() = default;

	void start(BundleContext::Ptr pContext)
	{
		PreferencesService::Ptr pPrefsService = Poco::OSP::ServiceFinder::find<Poco::OSP::PreferencesService>(pContext);

		TwoFactorServiceImpl::Config config;
		config.secretLengthBits = pPrefsService->configuration()->getUInt("auth.twofactor.secretLengthBits"s, 128);
		config.codeDigits = pPrefsService->configuration()->getUInt("auth.twofactor.codeDigits"s, 6);
		config.passphrase = pPrefsService->configuration()->getString("auth.twofactor.passphrase"s, ""s);
		config.issuer = pPrefsService->configuration()->getString("auth.twofactor.issuer"s, ""s);

		if (!config.passphrase.empty() && !config.issuer.empty())
		{
			AuthAdminService::Ptr pAuthAdminService = Poco::OSP::ServiceFinder::find<AuthAdminService>(pContext);
			TwoFactorServiceImpl::Ptr pTwoFactorService = new TwoFactorServiceImpl(pAuthAdminService, config);
			_pTwoFactorServiceRef = pContext->registry().registerService(TwoFactorService::SERVICE_NAME, pTwoFactorService, Properties());
			pContext->logger().information("TwoFactorService registered.");
		}
		else
		{
			pContext->logger().notice("No key and/or issuer configuration for TwoFactorService found. Service will not be available.");
		}
	}

	void stop(BundleContext::Ptr pContext)
	{
		if (_pTwoFactorServiceRef)
		{
			pContext->registry().unregisterService(_pTwoFactorServiceRef);
			_pTwoFactorServiceRef = 0;
		}
	}

private:
	ServiceRef::Ptr _pTwoFactorServiceRef;
};


} } } } // namespace Poco::OSP::Auth::TwoFactor


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(Poco::OSP::Auth::TwoFactor::TwoFactorBundleActivator)
POCO_END_MANIFEST
