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
#include "Poco/OSP/Preferences.h"
#include "Poco/OSP/Auth/TwoFactor/TwoFactorServiceImpl.h"
#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/StringTokenizer.h"
#include "Poco/Timespan.h"
#include "Poco/DigestEngine.h"
#include "Poco/RandomStream.h"
#include "Poco/ClassLibrary.h"
#include <cstddef>
#include <ios>
#include <string>


using Poco::OSP::BundleActivator;
using Poco::OSP::BundleContext;
using Poco::OSP::Bundle;
using Poco::OSP::Service;
using Poco::OSP::ServiceRef;
using Poco::OSP::PreferencesService;
using Poco::OSP::Properties;
using namespace std::string_literals;


namespace Poco::OSP::Auth::TwoFactor {


class TwoFactorBundleActivator: public BundleActivator
	/// The BundleActivator for the Auth TwoFactor Bundle.
	///
	/// Registers the TwoFactorServiceImpl service.
{
public:
	TwoFactorBundleActivator() = default;
	~TwoFactorBundleActivator() override = default;

	void start(BundleContext::Ptr pContext) override
	{
		PreferencesService::Ptr pPrefsService = Poco::OSP::ServiceFinder::find<Poco::OSP::PreferencesService>(pContext);

		TwoFactorServiceImpl::Config config;
		config.secretLengthBits = pPrefsService->configuration()->getUInt("auth.twofactor.secretLengthBits"s, 128);
		config.codeDigits = pPrefsService->configuration()->getUInt("auth.twofactor.codeDigits"s, 6);
		config.passphrase = pPrefsService->configuration()->getString("auth.twofactor.passphrase"s, ""s);
		config.issuer = pPrefsService->configuration()->getString("auth.twofactor.issuer"s, ""s);
		config.maxFailures = pPrefsService->configuration()->getUInt("auth.twofactor.maxFailures"s,
			static_cast<unsigned>(config.maxFailures));
		const unsigned lockOutSeconds = pPrefsService->configuration()->getUInt("auth.twofactor.lockOutPeriod"s,
			static_cast<unsigned>(config.lockOutPeriod.totalSeconds()));
		config.lockOutPeriod = Poco::Timespan(static_cast<long>(lockOutSeconds), 0);

		if (!config.passphrase.empty() && !config.issuer.empty())
		{
			config.keySalt = pPrefsService->configuration()->getString("auth.twofactor.keySalt"s, ""s);
			if (config.keySalt.empty())
			{
				config.keySalt = keySalt(pPrefsService->preferences(pContext->thisBundle()->symbolicName()));
			}
			AuthAdminService::Ptr pAuthAdminService = Poco::OSP::ServiceFinder::find<AuthAdminService>(pContext);
			TwoFactorServiceImpl::Ptr pTwoFactorService = new TwoFactorServiceImpl(pAuthAdminService, config,
				pContext->logger());
			_pTwoFactorServiceRef = pContext->registry().registerService(TwoFactorService::SERVICE_NAME, pTwoFactorService, Properties());
			pContext->logger().information("TwoFactorService registered.");
		}
		else
		{
			pContext->logger().notice("No key and/or issuer configuration for TwoFactorService found. Service will not be available.");
		}
	}

	void stop(BundleContext::Ptr pContext) override
	{
		if (_pTwoFactorServiceRef != nullptr)
		{
			pContext->registry().unregisterService(_pTwoFactorServiceRef);
			_pTwoFactorServiceRef = nullptr;
		}
	}

private:
	[[nodiscard]] static std::string keySalt(Poco::OSP::Preferences::Ptr pPrefs);
		/// Returns the salt of the key derivation that is kept in the given
		/// preferences of the bundle. Generates a random salt and stores it
		/// there if the preferences have none, as at the first start.

	static constexpr std::size_t KEY_SALT_SIZE = 16;
	static inline const std::string KEY_SALT_PROPERTY{"keySalt"};

	ServiceRef::Ptr _pTwoFactorServiceRef;
};


std::string TwoFactorBundleActivator::keySalt(Poco::OSP::Preferences::Ptr pPrefs)
{
	std::string salt = pPrefs->getString(KEY_SALT_PROPERTY, ""s);
	if (salt.empty())
	{
		std::string randomBytes(KEY_SALT_SIZE, '\0');
		Poco::RandomInputStream rnd;
		rnd.read(randomBytes.data(), static_cast<std::streamsize>(randomBytes.size()));
		salt = Poco::DigestEngine::digestToHex(Poco::DigestEngine::Digest(randomBytes.begin(), randomBytes.end()));
		pPrefs->setString(KEY_SALT_PROPERTY, salt);
		pPrefs->save();
	}
	return salt;
}


} // namespace Poco::OSP::Auth::TwoFactor


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(Poco::OSP::Auth::TwoFactor::TwoFactorBundleActivator)
POCO_END_MANIFEST
