//
// BundleAdminTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "BundleAdminTest.h"
#include "HTTPStubs.h"
#include "LogCapture.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/BundleContextFactory.h"
#include "Poco/OSP/BundleFactory.h"
#include "Poco/OSP/BundleInstallerService.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/BundleRepository.h"
#include "Poco/OSP/CodeCache.h"
#include "Poco/OSP/LanguageTag.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/Service.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/SystemEvents.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPMessage.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/StringPartSource.h"
#include "Poco/Zip/Compress.h"
#include "Poco/ClassLoader.h"
#include "Poco/DateTime.h"
#include "Poco/File.h"
#include "Poco/FileStream.h"
#include "Poco/Message.h"
#include "Poco/Path.h"
#include "Poco/StreamCopier.h"
#include "Poco/TemporaryFile.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>


using Poco::OSP::Bundle;
using Poco::Path;
using namespace std::string_literals;


namespace
{
	const std::string TARGET_NAME("<b>x</b>\"'&${symbolicName}");
	const std::string ESCAPED_TARGET_NAME("&lt;b&gt;x&lt;/b&gt;&quot;&#39;&amp;${symbolicName}");
	const std::string ODD_SYMBOLIC_NAME("osp.test.odd \"'&<> name");
	const std::string ESCAPED_ODD_SYMBOLIC_NAME("osp.test.odd &quot;&#39;&amp;&lt;&gt; name");
	const std::string ENCODED_ODD_SYMBOLIC_NAME("osp.test.odd%20%22%27%26%3C%3E%20name");

	std::string manifest(const std::string& symbolicName, const std::string& name, const std::string& requiredBundles)
	{
		std::string result(
			"Manifest-Version: 1.0\n"
			"Bundle-Name: " + name + "\n"
			"Bundle-SymbolicName: " + symbolicName + "\n"
			"Bundle-Version: 1.0.0\n"
			"Bundle-Vendor: Applied Informatics\n"
			"Bundle-Copyright: (c) 2026, Applied Informatics Software Engineering GmbH\n");
		if (!requiredBundles.empty()) result += "Require-Bundle: " + requiredBundles + "\n";
		return result;
	}

	void writeFile(const Path& baseDir, const std::string& relativePath, const std::string& content)
		/// Writes content to the file with the given path (in UNIX notation)
		/// below baseDir, and creates the directories of the path.
	{
		const Path path(baseDir, Path(relativePath, Path::PATH_UNIX));
		Poco::File(path.parent()).createDirectories();
		Poco::FileOutputStream ostr(path.toString());
		ostr << content;
	}

	Path directoryBelow(const Path& baseDir, const std::string& first, const std::string& second)
	{
		Path path(baseDir);
		path.pushDirectory(first);
		path.pushDirectory(second);
		return path;
	}

	std::string readFile(const std::string& path)
	{
		Poco::FileInputStream istr(path);
		std::string content;
		Poco::StreamCopier::copyToString(istr, content);
		return content;
	}

	std::string zipFile(const std::string& manifestContent)
		/// Returns a ZIP bundle file with only the given manifest.
	{
		std::ostringstream ostr;
		Poco::Zip::Compress compress(ostr, true);
		std::istringstream istr(manifestContent);
		compress.addFile(istr, Poco::DateTime(), Path("META-INF/manifest.mf"s, Path::PATH_UNIX));
		compress.close();
		return ostr.str();
	}

	std::string bundleFile(const std::string& symbolicName, const std::string& name)
		/// Returns a ZIP bundle file with only a manifest.
	{
		return zipFile(manifest(symbolicName, name, ""s));
	}

	bool contains(const std::string& text, const std::string& part)
	{
		return text.find(part) != std::string::npos;
	}

	class OtherService: public Poco::OSP::Service
		/// A service that is not a web session manager.
	{
	public:
		OtherService() = default;

		~OtherService() override = default;

		OtherService(const OtherService&) = delete;
		OtherService(OtherService&&) = delete;
		OtherService& operator = (const OtherService&) = delete;
		OtherService& operator = (OtherService&&) = delete;

		[[nodiscard]] const std::type_info& type() const override
		{
			return typeid(OtherService);
		}

		[[nodiscard]] bool isA(const std::type_info& otherType) const override
		{
			return otherType == typeid(OtherService) || Poco::OSP::Service::isA(otherType);
		}
	};
}


struct BundleAdminTest::Fixture
	/// The bundles osp.web, com.appinf.osp.bundleadmin (with the bundle properties
	/// of the bundle, and requiring osp.web), osp.test.target (named TARGET_NAME)
	/// and ODD_SYMBOLIC_NAME, all started, a bundle installer service, and the
	/// request handler factories of the bundle library.
{
	Fixture() = delete;

	explicit Fixture(const std::string& extraProperties)
	{
		const Path base(Path(directory.path()).makeDirectory());
		const Path webPath(directoryBelow(base, "bundles"s, "osp.web_1.0.0"s));
		writeFile(webPath, "META-INF/manifest.mf"s, manifest("osp.web"s, "Web"s, ""s));
		const Path adminPath(directoryBelow(base, "bundles"s, "com.appinf.osp.bundleadmin_1.0.0"s));
		writeFile(adminPath, "META-INF/manifest.mf"s, manifest("com.appinf.osp.bundleadmin"s, "Bundle Administration"s, "osp.web"s));
		writeFile(adminPath, "bundle.properties"s, readFile(BUNDLEADMIN_BUNDLE_PROPERTIES) + "\n" + extraProperties);
		const Path targetPath(directoryBelow(base, "bundles"s, "osp.test.target_1.0.0"s));
		writeFile(targetPath, "META-INF/manifest.mf"s, manifest("osp.test.target"s, TARGET_NAME, ""s));
		const Path oddPath(directoryBelow(base, "bundles"s, "osp.test.odd_1.0.0"s));
		writeFile(oddPath, "META-INF/manifest.mf"s, manifest(ODD_SYMBOLIC_NAME, "Odd"s, ""s));
		const Path repositoryPath(directoryBelow(base, "repository"s, "bundles"s));
		Poco::File(repositoryPath).createDirectories();

		pCodeCache = std::make_unique<Poco::OSP::CodeCache>(Path(base, "codeCache/"s).toString());
		pContextFactory = new Poco::OSP::BundleContextFactory(registry, systemEvents, Path(base, "persistency/"s));
		pLoader = std::make_unique<Poco::OSP::BundleLoader>(*pCodeCache, new Poco::OSP::BundleFactory(Poco::OSP::LanguageTag("en"s, "US"s)), pContextFactory);
		pRepository = std::make_unique<Poco::OSP::BundleRepository>(repositoryPath.toString(), *pLoader);
		pWeb = pLoader->loadBundle(webPath.toString());
		pAdmin = pLoader->loadBundle(adminPath.toString());
		pTarget = pLoader->loadBundle(targetPath.toString());
		pOdd = pLoader->loadBundle(oddPath.toString());
		for (Bundle::Ptr pBundle: {pWeb, pAdmin, pTarget, pOdd})
		{
			pBundle->resolve();
			pBundle->start();
		}
		registry.registerService(Poco::OSP::BundleInstallerService::SERVICE_NAME, new Poco::OSP::BundleInstallerService(*pRepository), Poco::OSP::Properties());
		classLoader.loadLibrary(BUNDLEADMIN_BUNDLE_LIBRARY, "WebServer"s);
	}

	~Fixture() = default;

	Fixture(const Fixture&) = delete;
	Fixture(Fixture&&) = delete;
	Fixture& operator = (const Fixture&) = delete;
	Fixture& operator = (Fixture&&) = delete;

	Poco::OSP::Web::WebSession::Ptr createSession(const std::string& name)
		/// Registers a web session manager and returns a new session with the given name
		/// for the client of RequestStub.
	{
		Poco::OSP::BundleContext::Ptr pContext = pLoader->contextForBundle(pAdmin);
		Poco::OSP::Web::WebSessionManager::Ptr pSessionManager = new Poco::OSP::Web::WebSessionManager(pContext);
		registry.registerService(Poco::OSP::Web::WebSessionManager::SERVICE_NAME, pSessionManager, Poco::OSP::Properties());
		RequestStub login(Poco::Net::HTTPRequest::HTTP_GET, "/login"s, ""s);
		return pSessionManager->create(name, login, 600, pContext);
	}

	ResponseStub& handle(const std::string& factoryName, RequestStub& request)
		/// Lets a handler of the factory with the given class name handle the request.
	{
		std::unique_ptr<Poco::OSP::Web::WebRequestHandlerFactory> pFactory(classLoader.create(factoryName));
		pFactory->init(pLoader->contextForBundle(pAdmin));
		std::unique_ptr<Poco::Net::HTTPRequestHandler> pHandler(pFactory->createRequestHandler(request));
		pHandler->handleRequest(request, request.response());
		return request.responseStub();
	}

	Poco::TemporaryFile directory;
	Poco::OSP::SystemEvents systemEvents;
	std::unique_ptr<Poco::OSP::CodeCache> pCodeCache;
	Poco::OSP::BundleContextFactory::Ptr pContextFactory;
	std::unique_ptr<Poco::OSP::BundleLoader> pLoader;
	std::unique_ptr<Poco::OSP::BundleRepository> pRepository;
	Bundle::Ptr pWeb;
	Bundle::Ptr pAdmin;
	Bundle::Ptr pTarget;
	Bundle::Ptr pOdd;
	Poco::ClassLoader<Poco::OSP::Web::WebRequestHandlerFactory> classLoader;
	// Declared last, so that the services are destroyed before the bundles and the loader.
	Poco::OSP::ServiceRegistry registry;
};


namespace
{
	std::string formBody(const std::map<std::string, std::string>& fields)
		/// Returns the fields in the application/x-www-form-urlencoded format.
	{
		Poco::Net::HTMLForm form;
		for (const auto& field: fields) form.set(field.first, field.second);
		std::ostringstream ostr;
		form.write(ostr);
		return ostr.str();
	}

	std::unique_ptr<RequestStub> postRequest(const std::string& uri, const std::map<std::string, std::string>& fields)
		/// Returns a POST request with the given form fields, from a page of the server.
	{
		auto pRequest = std::make_unique<RequestStub>(Poco::Net::HTTPRequest::HTTP_POST, uri, formBody(fields));
		pRequest->setContentType("application/x-www-form-urlencoded"s);
		pRequest->set("Origin"s, RequestStub::ORIGIN);
		return pRequest;
	}

	const std::string SHOW_TARGET("/bundleAdmin/showBundle?symbolicName=osp.test.target"s);
	const std::map<std::string, std::string> STOP_TARGET{{"symbolicName"s, "osp.test.target"s}, {"action"s, "stop"s}};

	std::unique_ptr<RequestStub> uploadRequest(const std::string& uri, const std::map<std::string, std::string>& fields, const std::vector<std::string>& bundles)
		/// Returns a request that uploads the given bundle files with the given form fields,
		/// which precede the files in the body.
	{
		Poco::Net::HTMLForm form(Poco::Net::HTMLForm::ENCODING_MULTIPART);
		for (const auto& field: fields) form.set(field.first, field.second);
		for (const auto& bundle: bundles)
		{
			form.addPart("file"s, new Poco::Net::StringPartSource(bundle, "application/octet-stream"s, "upload.bndl"s));
		}
		Poco::Net::HTTPRequest prepared(Poco::Net::HTTPRequest::HTTP_POST, uri, Poco::Net::HTTPMessage::HTTP_1_1);
		form.prepareSubmit(prepared);
		std::ostringstream ostr;
		form.write(ostr);
		auto pRequest = std::make_unique<RequestStub>(Poco::Net::HTTPRequest::HTTP_POST, uri, ostr.str());
		pRequest->setContentType(prepared.getContentType());
		pRequest->set("Origin"s, RequestStub::ORIGIN);
		return pRequest;
	}
}


BundleAdminTest::BundleAdminTest(const std::string& name): CppUnit::TestCase(name)
{
}


BundleAdminTest::~BundleAdminTest() = default;


void BundleAdminTest::testListPageEscapesBundleName()
{
	RequestStub request(Poco::Net::HTTPRequest::HTTP_GET, "/bundleAdmin"s, ""s);

	const std::string page = _pFixture->handle("BundleListHandlerFactory"s, request).body();

	assertTrue (contains(page, ESCAPED_TARGET_NAME));
	assertTrue (!contains(page, "<b>x</b>"s));
	assertTrue (contains(page, "HREF=\"/bundleAdmin/showBundle?symbolicName=osp.test.target\""s));
}


void BundleAdminTest::testInfoPageEscapesBundleName()
{
	RequestStub request(Poco::Net::HTTPRequest::HTTP_GET, "/bundleAdmin/showBundle?symbolicName=osp.test.target"s, ""s);

	const std::string page = _pFixture->handle("BundleInfoHandlerFactory"s, request).body();

	assertTrue (contains(page, "<H1>Bundle: " + ESCAPED_TARGET_NAME + "</H1>"));
	assertTrue (!contains(page, "<b>x</b>"s));
}


void BundleAdminTest::testInstallCompletePageEscapesBundleName()
{
	std::unique_ptr<RequestStub> pRequest = uploadRequest("/bundleAdmin/installBundle"s, {}, {bundleFile("osp.test.uploaded"s, TARGET_NAME)});

	const std::string page = _pFixture->handle("BundleInstallHandlerFactory"s, *pRequest).body();

	assertTrue (_pFixture->pLoader->findBundle("osp.test.uploaded"s) != nullptr);
	assertTrue (contains(page, ESCAPED_TARGET_NAME));
	assertTrue (!contains(page, "<b>x</b>"s));
}


void BundleAdminTest::testListPageEncodesSymbolicName()
{
	RequestStub request(Poco::Net::HTTPRequest::HTTP_GET, "/bundleAdmin"s, ""s);

	const std::string page = _pFixture->handle("BundleListHandlerFactory"s, request).body();

	assertTrue (contains(page, "HREF=\"/bundleAdmin/showBundle?symbolicName=" + ENCODED_ODD_SYMBOLIC_NAME + "\""));
}


void BundleAdminTest::testActionFormEscapesSymbolicName()
{
	RequestStub request(Poco::Net::HTTPRequest::HTTP_GET, "/bundleAdmin/showBundle?symbolicName=" + ENCODED_ODD_SYMBOLIC_NAME, ""s);

	const std::string page = _pFixture->handle("BundleInfoHandlerFactory"s, request).body();

	assertTrue (contains(page, "<INPUT TYPE=\"hidden\" NAME=\"symbolicName\" VALUE=\"" + ESCAPED_ODD_SYMBOLIC_NAME + "\">"));
	assertTrue (!contains(page, ODD_SYMBOLIC_NAME));
}


void BundleAdminTest::testInstallCompletePageEncodesSymbolicName()
{
	// Without the characters that file names on Windows cannot contain: the
	// repository stores the bundle file under its symbolic name.
	std::unique_ptr<RequestStub> pRequest = uploadRequest("/bundleAdmin/installBundle"s, {}, {bundleFile("osp.test.uploaded '&name"s, "Uploaded"s)});

	const std::string page = _pFixture->handle("BundleInstallHandlerFactory"s, *pRequest).body();

	assertTrue (_pFixture->pLoader->findBundle("osp.test.uploaded '&name"s) != nullptr);
	assertTrue (contains(page, "HREF=\"/bundleAdmin/showBundle?symbolicName=osp.test.uploaded%20%27%26name\""s));
}


void BundleAdminTest::testGetDoesNotChangeBundle()
{
	RequestStub request(Poco::Net::HTTPRequest::HTTP_GET, SHOW_TARGET + "&action=stop"s, ""s);
	request.set("Referer"s, RequestStub::ORIGIN + SHOW_TARGET);

	ResponseStub& response = _pFixture->handle("BundleInfoHandlerFactory"s, request);

	assertTrue (response.getStatus() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
	assertTrue (_pFixture->pTarget->isActive());
}


void BundleAdminTest::testCrossOriginPostDoesNotChangeBundle()
{
	std::unique_ptr<RequestStub> pCrossOrigin = postRequest(SHOW_TARGET, STOP_TARGET);
	pCrossOrigin->set("Origin"s, "http://www.example.com"s);
	std::unique_ptr<RequestStub> pWithoutOrigin = postRequest(SHOW_TARGET, STOP_TARGET);
	pWithoutOrigin->erase("Origin"s);

	assertTrue (_pFixture->handle("BundleInfoHandlerFactory"s, *pCrossOrigin).getStatus() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
	assertTrue (_pFixture->handle("BundleInfoHandlerFactory"s, *pWithoutOrigin).getStatus() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
	assertTrue (_pFixture->pTarget->isActive());
}


void BundleAdminTest::testSameOriginPostStopsBundle()
{
	std::unique_ptr<RequestStub> pRequest = postRequest(SHOW_TARGET, STOP_TARGET);
	pRequest->erase("Origin"s);
	pRequest->set("Referer"s, RequestStub::ORIGIN + SHOW_TARGET);

	ResponseStub& response = _pFixture->handle("BundleInfoHandlerFactory"s, *pRequest);

	assertTrue (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK);
	assertTrue (!_pFixture->pTarget->isActive());
}


void BundleAdminTest::testSessionRequiresCSRFToken()
{
	_pFixture = std::make_unique<Fixture>("web.session = bundleadmin\n"s);
	Poco::OSP::Web::WebSession::Ptr pSession = _pFixture->createSession("bundleadmin"s);
	const std::string cookie("osp.web.session.bundleadmin=" + pSession->id());

	std::unique_ptr<RequestStub> pWithoutToken = postRequest(SHOW_TARGET, STOP_TARGET);
	pWithoutToken->set("Cookie"s, cookie);
	std::map<std::string, std::string> fields(STOP_TARGET);
	fields["csrfToken"s] = "wrong"s;
	std::unique_ptr<RequestStub> pWrongToken = postRequest(SHOW_TARGET, fields);
	pWrongToken->set("Cookie"s, cookie);
	fields["csrfToken"s] = pSession->csrfToken();
	std::unique_ptr<RequestStub> pWithToken = postRequest(SHOW_TARGET, fields);
	pWithToken->set("Cookie"s, cookie);

	assertTrue (_pFixture->handle("BundleInfoHandlerFactory"s, *pWithoutToken).getStatus() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
	assertTrue (_pFixture->handle("BundleInfoHandlerFactory"s, *pWrongToken).getStatus() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
	assertTrue (_pFixture->pTarget->isActive());
	assertTrue (_pFixture->handle("BundleInfoHandlerFactory"s, *pWithToken).getStatus() == Poco::Net::HTTPResponse::HTTP_OK);
	assertTrue (!_pFixture->pTarget->isActive());
}


void BundleAdminTest::testActionFormCarriesCSRFToken()
{
	_pFixture = std::make_unique<Fixture>("web.session = bundleadmin\n"s);
	Poco::OSP::Web::WebSession::Ptr pSession = _pFixture->createSession("bundleadmin"s);
	RequestStub request(Poco::Net::HTTPRequest::HTTP_GET, SHOW_TARGET, ""s);
	request.set("Cookie"s, "osp.web.session.bundleadmin=" + pSession->id());

	const std::string page = _pFixture->handle("BundleInfoHandlerFactory"s, request).body();

	assertTrue (contains(page, "<INPUT TYPE=\"hidden\" NAME=\"csrfToken\" VALUE=\"" + pSession->csrfToken() + "\">"));
}


void BundleAdminTest::testPageIsCompleteWithoutSessionManager()
{
	_pFixture = std::make_unique<Fixture>("web.session = bundleadmin\n"s);
	RequestStub request(Poco::Net::HTTPRequest::HTTP_GET, SHOW_TARGET, ""s);

	ResponseStub& response = _pFixture->handle("BundleInfoHandlerFactory"s, request);

	assertTrue (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK);
	assertTrue (contains(response.body(), "VALUE=\"stop\""s));
	assertTrue (contains(response.body(), "</HTML>"s));
}


void BundleAdminTest::testErrorAfterPartOfPageIsLogged()
{
	_pFixture = std::make_unique<Fixture>("web.session = bundleadmin\n"s);
	_pFixture->registry.registerService(Poco::OSP::Web::WebSessionManager::SERVICE_NAME, new OtherService, Poco::OSP::Properties());
	LogCapture capture("osp.bundle.com.appinf.osp.bundleadmin"s);
	RequestStub request(Poco::Net::HTTPRequest::HTTP_GET, SHOW_TARGET, ""s);

	ResponseStub& response = _pFixture->handle("BundleInfoHandlerFactory"s, request);

	assertTrue (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK);
	assertTrue (!contains(response.body(), "</HTML>"s));
	bool logged = false;
	for (const auto& msg: capture.channel().waitForMessages(1))
	{
		if (msg.getPriority() == Poco::Message::PRIO_ERROR && contains(msg.getText(), SHOW_TARGET)) logged = true;
	}
	assertTrue (logged);
}


void BundleAdminTest::testCrossOriginUploadDoesNotInstall()
{
	std::unique_ptr<RequestStub> pRequest = uploadRequest("/bundleAdmin/installBundle"s, {}, {bundleFile("osp.test.uploaded"s, "Uploaded"s)});
	pRequest->set("Origin"s, "http://www.example.com"s);

	ResponseStub& response = _pFixture->handle("BundleInstallHandlerFactory"s, *pRequest);

	assertTrue (response.getStatus() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
	assertTrue (_pFixture->pLoader->findBundle("osp.test.uploaded"s) == nullptr);
}


void BundleAdminTest::testUploadWithPutIsRefused()
{
	std::unique_ptr<RequestStub> pRequest = uploadRequest("/bundleAdmin/installBundle"s, {}, {bundleFile("osp.test.uploaded"s, "Uploaded"s)});
	pRequest->setMethod(Poco::Net::HTTPRequest::HTTP_PUT);
	pRequest->set("Origin"s, "http://www.example.com"s);
	pRequest->setChunkedTransferEncoding(true);

	ResponseStub& response = _pFixture->handle("BundleInstallHandlerFactory"s, *pRequest);

	assertTrue (response.getStatus() == Poco::Net::HTTPResponse::HTTP_METHOD_NOT_ALLOWED);
	assertEqual ("GET, HEAD, POST"s, response.get("Allow"s, ""s));
	assertTrue (_pFixture->pLoader->findBundle("osp.test.uploaded"s) == nullptr);
}


void BundleAdminTest::testUploadInSessionRequiresCSRFToken()
{
	_pFixture = std::make_unique<Fixture>("web.session = bundleadmin\n"s);
	Poco::OSP::Web::WebSession::Ptr pSession = _pFixture->createSession("bundleadmin"s);
	std::unique_ptr<RequestStub> pWithoutToken = uploadRequest("/bundleAdmin/installBundle"s, {}, {bundleFile("osp.test.uploaded"s, "Uploaded"s)});
	pWithoutToken->set("Cookie"s, "osp.web.session.bundleadmin=" + pSession->id());
	std::unique_ptr<RequestStub> pWithToken = uploadRequest("/bundleAdmin/installBundle"s, {{"csrfToken"s, pSession->csrfToken()}}, {bundleFile("osp.test.uploaded"s, "Uploaded"s)});
	pWithToken->set("Cookie"s, "osp.web.session.bundleadmin=" + pSession->id());

	assertTrue (_pFixture->handle("BundleInstallHandlerFactory"s, *pWithoutToken).getStatus() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
	assertTrue (_pFixture->pLoader->findBundle("osp.test.uploaded"s) == nullptr);
	assertTrue (_pFixture->handle("BundleInstallHandlerFactory"s, *pWithToken).getStatus() == Poco::Net::HTTPResponse::HTTP_OK);
	assertTrue (_pFixture->pLoader->findBundle("osp.test.uploaded"s) != nullptr);
}


void BundleAdminTest::testStopOfNeededBundleIsRefused()
{
	for (const std::string& symbolicName: {"com.appinf.osp.bundleadmin"s, "osp.web"s})
	{
		for (const std::string& action: {"stop"s, "confirmedStop"s, "confirmedUninstall"s})
		{
			std::unique_ptr<RequestStub> pRequest = postRequest("/bundleAdmin/showBundle?symbolicName=" + symbolicName, {{"symbolicName"s, symbolicName}, {"action"s, action}});

			ResponseStub& response = _pFixture->handle("BundleInfoHandlerFactory"s, *pRequest);

			assertTrue (response.getStatus() == Poco::Net::HTTPResponse::HTTP_CONFLICT);
		}
	}
	assertTrue (_pFixture->pAdmin->isActive());
	assertTrue (_pFixture->pWeb->isActive());
}


void BundleAdminTest::testNeededBundleHasNoStopButton()
{
	RequestStub request(Poco::Net::HTTPRequest::HTTP_GET, "/bundleAdmin/showBundle?symbolicName=osp.web"s, ""s);

	const std::string page = _pFixture->handle("BundleInfoHandlerFactory"s, request).body();

	assertTrue (contains(page, "<H1>Bundle: Web</H1>"s));
	assertTrue (!contains(page, "VALUE=\"stop\""s));
}


void BundleAdminTest::testUpgradeOfNeededBundleIsRefused()
{
	for (const std::string& symbolicName: {"com.appinf.osp.bundleadmin"s, "osp.web"s})
	{
		std::unique_ptr<RequestStub> pRequest = uploadRequest("/bundleAdmin/installBundle"s, {{"symbolicName"s, symbolicName}}, {bundleFile(symbolicName, "Upgrade"s)});

		ResponseStub& response = _pFixture->handle("BundleInstallHandlerFactory"s, *pRequest);

		assertTrue (response.getStatus() == Poco::Net::HTTPResponse::HTTP_CONFLICT);
	}
	assertTrue (_pFixture->pLoader->findBundle("com.appinf.osp.bundleadmin"s) == _pFixture->pAdmin);
	assertTrue (_pFixture->pLoader->findBundle("osp.web"s) == _pFixture->pWeb);
	assertTrue (_pFixture->pAdmin->isActive());
	assertTrue (_pFixture->pWeb->isActive());
}


void BundleAdminTest::testUpgradeOfUnknownBundle()
{
	std::unique_ptr<RequestStub> pRequest = uploadRequest("/bundleAdmin/installBundle"s, {{"symbolicName"s, "does.not.exist"s}}, {bundleFile("does.not.exist"s, "Unknown"s)});

	ResponseStub& response = _pFixture->handle("BundleInstallHandlerFactory"s, *pRequest);

	assertTrue (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK);
	assertTrue (contains(response.body(), "<H1>Unknown bundle.</H1>"s));
	assertTrue (_pFixture->pLoader->findBundle("does.not.exist"s) == nullptr);
}


void BundleAdminTest::testInstallErrorIsEscaped()
{
	// The manifest parser quotes the unsupported manifest version in its message.
	std::unique_ptr<RequestStub> pRequest = uploadRequest("/bundleAdmin/installBundle"s, {}, {zipFile("Manifest-Version: <b>2</b>\nBundle-Name: Invalid\nBundle-SymbolicName: osp.test.invalid\nBundle-Version: 1.0.0\n"s)});

	ResponseStub& response = _pFixture->handle("BundleInstallHandlerFactory"s, *pRequest);

	assertTrue (response.getStatus() == Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
	assertTrue (contains(response.body(), "&lt;b&gt;2&lt;/b&gt;"s));
	assertTrue (!contains(response.body(), "<b>2</b>"s));
}


void BundleAdminTest::testOnlyFirstBundleFileIsInstalled()
{
	std::unique_ptr<RequestStub> pRequest = uploadRequest("/bundleAdmin/installBundle"s, {}, {bundleFile("osp.test.first"s, "First"s), bundleFile("osp.test.second"s, "Second"s)});

	ResponseStub& response = _pFixture->handle("BundleInstallHandlerFactory"s, *pRequest);

	assertTrue (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK);
	assertTrue (_pFixture->pLoader->findBundle("osp.test.first"s) != nullptr);
	assertTrue (_pFixture->pLoader->findBundle("osp.test.second"s) == nullptr);
}


void BundleAdminTest::testUploadSizeIsLimited()
{
	_pFixture = std::make_unique<Fixture>("upload.maxSize = 100\n"s);
	std::unique_ptr<RequestStub> pTooLarge = uploadRequest("/bundleAdmin/installBundle"s, {}, {bundleFile("osp.test.uploaded"s, "Uploaded"s)});
	std::unique_ptr<RequestStub> pWithoutLength = uploadRequest("/bundleAdmin/installBundle"s, {}, {bundleFile("osp.test.uploaded"s, "Uploaded"s)});
	pWithoutLength->setChunkedTransferEncoding(true);

	assertTrue (_pFixture->handle("BundleInstallHandlerFactory"s, *pTooLarge).getStatus() == Poco::Net::HTTPResponse::HTTP_REQUEST_ENTITY_TOO_LARGE);
	assertTrue (_pFixture->handle("BundleInstallHandlerFactory"s, *pWithoutLength).getStatus() == Poco::Net::HTTPResponse::HTTP_LENGTH_REQUIRED);
	assertTrue (_pFixture->pLoader->findBundle("osp.test.uploaded"s) == nullptr);
}


void BundleAdminTest::testMissingInstallerService()
{
	_pFixture->registry.unregisterService(_pFixture->registry.findByName(Poco::OSP::BundleInstallerService::SERVICE_NAME));
	RequestStub request(Poco::Net::HTTPRequest::HTTP_GET, "/bundleAdmin/installBundle"s, ""s);

	ResponseStub& response = _pFixture->handle("BundleInstallHandlerFactory"s, request);

	assertTrue (response.getStatus() == Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
}


void BundleAdminTest::setUp()
{
	_pFixture = std::make_unique<Fixture>(""s);
}


void BundleAdminTest::tearDown()
{
	_pFixture.reset();
}


CppUnit::Test* BundleAdminTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("BundleAdminTest");

	CppUnit_addTest(pSuite, BundleAdminTest, testListPageEscapesBundleName);
	CppUnit_addTest(pSuite, BundleAdminTest, testInfoPageEscapesBundleName);
	CppUnit_addTest(pSuite, BundleAdminTest, testInstallCompletePageEscapesBundleName);
	CppUnit_addTest(pSuite, BundleAdminTest, testListPageEncodesSymbolicName);
	CppUnit_addTest(pSuite, BundleAdminTest, testActionFormEscapesSymbolicName);
	CppUnit_addTest(pSuite, BundleAdminTest, testInstallCompletePageEncodesSymbolicName);
	CppUnit_addTest(pSuite, BundleAdminTest, testGetDoesNotChangeBundle);
	CppUnit_addTest(pSuite, BundleAdminTest, testCrossOriginPostDoesNotChangeBundle);
	CppUnit_addTest(pSuite, BundleAdminTest, testSameOriginPostStopsBundle);
	CppUnit_addTest(pSuite, BundleAdminTest, testSessionRequiresCSRFToken);
	CppUnit_addTest(pSuite, BundleAdminTest, testActionFormCarriesCSRFToken);
	CppUnit_addTest(pSuite, BundleAdminTest, testPageIsCompleteWithoutSessionManager);
	CppUnit_addTest(pSuite, BundleAdminTest, testErrorAfterPartOfPageIsLogged);
	CppUnit_addTest(pSuite, BundleAdminTest, testCrossOriginUploadDoesNotInstall);
	CppUnit_addTest(pSuite, BundleAdminTest, testUploadWithPutIsRefused);
	CppUnit_addTest(pSuite, BundleAdminTest, testUploadInSessionRequiresCSRFToken);
	CppUnit_addTest(pSuite, BundleAdminTest, testStopOfNeededBundleIsRefused);
	CppUnit_addTest(pSuite, BundleAdminTest, testNeededBundleHasNoStopButton);
	CppUnit_addTest(pSuite, BundleAdminTest, testUpgradeOfNeededBundleIsRefused);
	CppUnit_addTest(pSuite, BundleAdminTest, testUpgradeOfUnknownBundle);
	CppUnit_addTest(pSuite, BundleAdminTest, testInstallErrorIsEscaped);
	CppUnit_addTest(pSuite, BundleAdminTest, testOnlyFirstBundleFileIsInstalled);
	CppUnit_addTest(pSuite, BundleAdminTest, testUploadSizeIsLimited);
	CppUnit_addTest(pSuite, BundleAdminTest, testMissingInstallerService);

	return pSuite;
}
