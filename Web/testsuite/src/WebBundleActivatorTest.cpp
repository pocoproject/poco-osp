//
// WebBundleActivatorTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "WebBundleActivatorTest.h"
#include "TemporaryBundle.h"
#include "ServerRequestStub.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Web/MediaTypeMapper.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/Net/HTTPBasicCredentials.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/ClassLoader.h"
#include "Poco/Exception.h"
#include "Poco/Path.h"
#include "Poco/TemporaryFile.h"
#include <memory>
#include <string>
#include <vector>


using Poco::OSP::Web::MediaTypeMapper;
using Poco::OSP::Web::WebServerDispatcher;
using Poco::OSP::PreferencesService;
using Poco::Net::HTTPResponse;
using namespace std::string_literals;


namespace
{
	const std::string AUTH_SERVICE_NAME("osp.web.test.auth");

	class RejectingAuthService: public Poco::OSP::Auth::AuthService
		/// Knows no user, so that every password check fails.
	{
	public:
		RejectingAuthService() = default;
		RejectingAuthService(const RejectingAuthService&) = delete;
		RejectingAuthService(RejectingAuthService&&) = delete;
		RejectingAuthService& operator = (const RejectingAuthService&) = delete;
		RejectingAuthService& operator = (RejectingAuthService&&) = delete;
		~RejectingAuthService() override = default;

		[[nodiscard]] bool authenticate(const std::string&, const std::string&) const override
		{
			return false;
		}

		[[nodiscard]] bool authorize(const std::string&, const std::string&) const override
		{
			return false;
		}

		[[nodiscard]] bool authorize(const std::string&, const std::string&, const std::string&) const override
		{
			return false;
		}

		[[nodiscard]] bool userExists(const std::string&) const override
		{
			return false;
		}
	};

	struct WebBundle
		/// The bundle activator of the bundle library, with a bundle that has the given properties,
		/// in whose registry the preferences service and a RejectingAuthService are registered.
	{
		WebBundle() = delete;

		explicit WebBundle(const std::string& properties):
			bundles(properties),
			pSettings(new Poco::Util::MapConfiguration),
			pPreferences(new PreferencesService(Poco::Path(persistency.path()), pSettings))
		{
			bundles.registry().registerService(PreferencesService::SERVICE_NAME, pPreferences, Poco::OSP::Properties());
			bundles.registry().registerService(AUTH_SERVICE_NAME, new RejectingAuthService, Poco::OSP::Properties());
			loader.loadLibrary(WEB_BUNDLE_LIBRARY);
			pActivator.reset(loader.create("WebBundleActivator"s));
		}

		~WebBundle() = default;

		WebBundle(const WebBundle&) = delete;
		WebBundle(WebBundle&&) = delete;
		WebBundle& operator = (const WebBundle&) = delete;
		WebBundle& operator = (WebBundle&&) = delete;

		void start()
			/// Starts the bundle and maps /secured to www/ of the bundle with the permission "read".
		{
			pActivator->start(bundles.context());
			dispatcher()->addVirtualPath(WebServerDispatcher::VirtualPath("/secured"s, "www"s,
				WebServerDispatcher::PathSecurity(WebServerDispatcher::SM_OWNER, ""s, "read"s), bundles.bundle()));
		}

		void stop()
		{
			pActivator->stop(bundles.context());
		}

		[[nodiscard]] WebServerDispatcher::Ptr dispatcher()
		{
			const Poco::OSP::ServiceRef::Ptr pRef = bundles.registry().findByName(WebServerDispatcher::SERVICE_NAME);
			if (pRef == nullptr) throw Poco::NotFoundException(WebServerDispatcher::SERVICE_NAME);
			return pRef->castedInstance<WebServerDispatcher>();
		}

		[[nodiscard]] HTTPResponse::HTTPStatus statusOfFailedLogin()
			/// Returns the status of the answer to a request for /secured with HTTP Basic
			/// credentials from 127.0.0.1.
		{
			ServerRequestStub request("127.0.0.1:50000"s, false);
			request.setURI("/secured/hello.txt"s);
			Poco::Net::HTTPBasicCredentials("alice"s, "wrong"s).authenticate(request);
			dispatcher()->handleRequest(request, request.response(), false);
			return request.response().getStatus();
		}

		TemporaryBundle bundles;
		Poco::TemporaryFile persistency;
		Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings;
		PreferencesService::Ptr pPreferences;
		Poco::ClassLoader<Poco::OSP::BundleActivator> loader;
		// Declared after the loader, so that it is destroyed before the library is unloaded.
		std::unique_ptr<Poco::OSP::BundleActivator> pActivator;
	};

	const std::string LIMITED_PROPERTIES("authServiceName = " + AUTH_SERVICE_NAME + "\nlogin.maxAttempts = 1\n");
}


WebBundleActivatorTest::WebBundleActivatorTest(const std::string& name): CppUnit::TestCase(name)
{
}


WebBundleActivatorTest::~WebBundleActivatorTest() = default;


void WebBundleActivatorTest::setUp()
{
}


void WebBundleActivatorTest::tearDown()
{
}


void WebBundleActivatorTest::testLoginSettingsOfBundleAndApplication()
{
	{
		WebBundle web(LIMITED_PROPERTIES);
		web.start();
		const HTTPResponse::HTTPStatus first = web.statusOfFailedLogin();
		const HTTPResponse::HTTPStatus second = web.statusOfFailedLogin();
		web.stop();

		assertTrue (first == HTTPResponse::HTTP_UNAUTHORIZED);
		assertTrue (second == HTTPResponse::HTTP_TOO_MANY_REQUESTS);
	}

	// The application configuration overrides the bundle properties.
	WebBundle web(LIMITED_PROPERTIES);
	web.pSettings->setInt("osp.web.login.maxAttempts"s, 0);
	web.start();
	const HTTPResponse::HTTPStatus first = web.statusOfFailedLogin();
	const HTTPResponse::HTTPStatus second = web.statusOfFailedLogin();
	web.stop();

	assertTrue (first == HTTPResponse::HTTP_UNAUTHORIZED);
	assertTrue (second == HTTPResponse::HTTP_UNAUTHORIZED);
}


void WebBundleActivatorTest::testInvalidLoginSettingFailsStart()
{
	struct InvalidSetting
	{
		std::string bundleProperties;
		std::string applicationKey;
		std::string applicationValue;
		std::string key;
			// The key that the error names.
	};
	const std::vector<InvalidSetting> invalidSettings{
		{.bundleProperties = "login.maxAttempts = many\n"s, .applicationKey = ""s, .applicationValue = ""s,
			.key = "login.maxAttempts"s},
		{.bundleProperties = ""s, .applicationKey = "osp.web.login.cooldownSeconds"s, .applicationValue = "0"s,
			.key = "osp.web.login.cooldownSeconds"s},
		{.bundleProperties = ""s, .applicationKey = "osp.web.login.exemptAddresses"s,
			.applicationValue = "192.0.2.1/24"s, .key = "osp.web.login.exemptAddresses"s}};

	for (const auto& setting: invalidSettings)
	{
		WebBundle web(setting.bundleProperties);
		if (!setting.applicationKey.empty()) web.pSettings->setString(setting.applicationKey, setting.applicationValue);
		std::string error;
		try
		{
			web.pActivator->start(web.bundles.context());
		}
		catch (const Poco::InvalidArgumentException& exc)
		{
			error = exc.message();
		}
		if (error.empty()) web.stop();

		assertTrue (error.find(setting.key) != std::string::npos);
		// The settings are read before any service is registered.
		assertTrue (web.bundles.registry().findByName(MediaTypeMapper::SERVICE_NAME) == nullptr);
		assertTrue (web.bundles.registry().findByName(WebServerDispatcher::SERVICE_NAME) == nullptr);
	}
}


CppUnit::Test* WebBundleActivatorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebBundleActivatorTest");

	CppUnit_addTest(pSuite, WebBundleActivatorTest, testLoginSettingsOfBundleAndApplication);
	CppUnit_addTest(pSuite, WebBundleActivatorTest, testInvalidLoginSettingFailsStart);

	return pSuite;
}
