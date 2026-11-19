//
// JWTBundleActivator.cpp
//
// Copyright (c) 2019, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/StringTokenizer.h"
#include "Poco/ClassLibrary.h"
#include "JWTValidator.h"


using Poco::OSP::BundleContext;
using Poco::OSP::Service;
using Poco::OSP::ServiceRef;
using Poco::OSP::PreferencesService;
using Poco::OSP::Properties;


namespace Poco::OSP::JWT {


class BundleActivator: public Poco::OSP::BundleActivator
{
public:
	BundleActivator() = default;

	~BundleActivator() override = default;

	void start(BundleContext::Ptr pContext) override
	{
		Poco::OSP::PreferencesService::Ptr pPrefs = Poco::OSP::ServiceFinder::find<Poco::OSP::PreferencesService>(pContext);
		if (pPrefs != nullptr)
		{
			std::string algos = pPrefs->configuration()->getString("jwt.validator.algorithms", "HS256");
			Poco::StringTokenizer algosTok(algos, ",;", Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);

			JWTValidator::Config config;
			config.algorithms.insert(algosTok.begin(), algosTok.end());
			config.hmacKey = pPrefs->configuration()->getString("jwt.validator.hmac.key", "");
			config.rsaPublicKey = pPrefs->configuration()->getString("jwt.validator.rsa.publicKey", "");
			config.ecPublicKey = pPrefs->configuration()->getString("jwt.validator.ec.publicKey", "");
			config.leeway = Poco::Timespan(pPrefs->configuration()->getInt("jwt.validator.leeway", 5), 0);
			config.issuer = pPrefs->configuration()->getString("jwt.validator.issuer", "");
			config.audience = pPrefs->configuration()->getString("jwt.validator.audience", "");

			JWTValidator::Ptr pValidator = new JWTValidator(config);
			_pValidatorRef = pContext->registry().registerService(JWTValidator::SERVICE_NAME, pValidator, Properties());
		}
		else
		{
			pContext->logger().error("No PreferencesService available, JWTValidator will not be available.");
		}
	}

	void stop(BundleContext::Ptr pContext) override
	{
		// The registry unregisters by name: a reference left from an earlier start
		// would remove a validator that another bundle has registered since.
		pContext->registry().unregisterService(_pValidatorRef);
		_pValidatorRef = nullptr;
	}

private:
	Poco::OSP::ServiceRef::Ptr _pValidatorRef;
};


} // namespace Poco::OSP::JWT


POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
	POCO_EXPORT_CLASS(Poco::OSP::JWT::BundleActivator)
POCO_END_MANIFEST
