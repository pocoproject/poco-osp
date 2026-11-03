//
// BundleInstallHandler.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "BundleInstallHandler.h"
#include "Poco/OSP/BundleInstallerService.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/PartHandler.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/AutoPtr.h"
#include "Poco/Exception.h"
#include "Poco/NumberFormatter.h"
#include <functional>
#include <istream>
#include <utility>
#include <vector>


using Poco::OSP::BundleContext;
using Poco::OSP::Bundle;
using Poco::OSP::BundleInstallerService;
using Poco::OSP::ServiceRef;
using Poco::Net::HTMLForm;
using Poco::Net::PartHandler;
using Poco::Net::MessageHeader;
using Poco::NumberFormatter;
using Poco::AutoPtr;


class InstallPartHandler: public PartHandler
	/// Installs the bundle in a file part with the given function.
{
public:
	using InstallFunction = std::function<Bundle::Ptr(std::istream&)>;

	explicit InstallPartHandler(InstallFunction install):
		_install(std::move(install))
	{
	}
	
	void handlePart(const MessageHeader& header, std::istream& stream) override
		/// Installs the bundle of the first file part only.
	{
		if (header.has("Content-Disposition") && _pBundle == nullptr)
		{
			_pBundle = _install(stream);
		}
	}
	
	[[nodiscard]] bool installed() const
	{
		return !_pBundle.isNull();
	}
	
	[[nodiscard]] Bundle::Ptr bundle() const
	{
		return _pBundle;
	}
		
private:
	InstallFunction _install;
	Bundle::Ptr _pBundle;
};


BundleInstallHandler::BundleInstallHandler(Poco::OSP::BundleContext::Ptr pContext):
	RequestHandler(pContext)
{
}


BundleInstallHandler::~BundleInstallHandler() = default;

	
void BundleInstallHandler::handle()
{
	// HTMLForm also reads a file from the body of other requests, such as PUT,
	// which would bypass the checks of an upload.
	const std::string& method = request().getMethod();
	if (method != Poco::Net::HTTPRequest::HTTP_GET && method != Poco::Net::HTTPRequest::HTTP_HEAD && method != Poco::Net::HTTPRequest::HTTP_POST)
	{
		response().set("Allow", "GET, HEAD, POST");
		sendErrorResponse(Poco::Net::HTTPResponse::HTTP_METHOD_NOT_ALLOWED, htmlize(text("methodNotAllowed")));
		return;
	}

	// A bundle is only installed from a form of this server, never from a form
	// of another site that the browser of the user sends.
	const bool upload = method == Poco::Net::HTTPRequest::HTTP_POST;
	if (upload && !isSameOriginPost())
	{
		sendForbidden();
		return;
	}
	if (upload && !request().hasContentLength())
	{
		sendErrorResponse(Poco::Net::HTTPResponse::HTTP_LENGTH_REQUIRED, htmlize(text("lengthRequired")));
		return;
	}
	if (upload && request().getContentLength64() > bundle()->properties().getInt64("upload.maxSize"))
	{
		sendErrorResponse(Poco::Net::HTTPResponse::HTTP_REQUEST_ENTITY_TOO_LARGE, htmlize(text("tooLarge")));
		return;
	}

	ServiceRef::Ptr pInstallerRef = context()->registry().findByName(BundleInstallerService::SERVICE_NAME);
	AutoPtr<BundleInstallerService> pInstaller;
	if (pInstallerRef != nullptr) pInstaller = pInstallerRef->castedInstance<BundleInstallerService>();
	if (pInstaller == nullptr)
	{
		sendErrorResponse(Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE, htmlize(text("noInstaller")));
		return;
	}

	HTMLForm form;
	bool neededBundle = false;
	InstallPartHandler partHandler([this, &form, &pInstaller, &neededBundle](std::istream& stream)
		{
			// The forms send the token before the file.
			if (!hasValidCSRFToken(form.get("csrfToken", ""))) throw Poco::NoPermissionException("CSRF token");

			const std::string symbolicName = form.get("symbolicName", "");
			if (symbolicName.empty())
				return pInstaller->installBundle(stream);

			Bundle::Ptr pBundle = context()->findBundle(symbolicName);
			if (pBundle == nullptr)
				return Bundle::Ptr();
			// Replacing a bundle stops it and the bundles that require it, this one included.
			neededBundle = isNeededByThisBundle(pBundle);
			if (neededBundle)
				return Bundle::Ptr();
			return pInstaller->replaceBundle(symbolicName, stream);
		});
	try
	{
		form.load(request(), request().stream(), partHandler);
	}
	catch (Poco::NoPermissionException&)
	{
		sendForbidden();
		return;
	}
	if (neededBundle)
	{
		sendErrorResponse(Poco::Net::HTTPResponse::HTTP_CONFLICT, htmlize(text("neededBundle")));
		return;
	}

	std::string title;
	std::string backLink;
	std::string backLinkTitle;
	std::string symbolicName = form.get("symbolicName", "");
	if (!symbolicName.empty())
	{
		Bundle::Ptr pBundle = context()->findBundle(symbolicName);
		if (pBundle == nullptr)
		{
			beginPage(text("unknownBundle"));
			endPage();
			return;
		}
		title = text("upgradeBundle");
		title += ": ";
		title += symbolicName;
		backLink = bundlePath(symbolicName);
		backLinkTitle = text("bundle");
		backLinkTitle += ": ";
		backLinkTitle += pBundle->name();
	}
	else
	{
		title = text("installBundle");
		backLink = bundle()->properties().getString("web.path");
		backLinkTitle = text("installedBundles");
	}
		
	beginPage(title, backLink, backLinkTitle);
	if (partHandler.installed())
	{
		templat().clear();
		templat().setString("symbolicName", htmlize(encodeQueryValue(partHandler.bundle()->symbolicName())));
		templat().setString("name", htmlize(partHandler.bundle()->name()));
		sendTemplate("html.installComplete");
		beginList();
		item(text("id"), NumberFormatter::format(partHandler.bundle()->id()));
		item(text("symbolicName"), partHandler.bundle()->symbolicName());
		item(text("version"), partHandler.bundle()->version().toString());
		item(text("path"), partHandler.bundle()->path());
		endList();
	}
	else if (symbolicName.empty())
	{
		templat().clear();
		templat().setString("csrfToken", htmlize(csrfTokenOfSession()));
		sendTemplate("html.installForm");
	}
	else
	{
		templat().clear();
		templat().setString("symbolicName", htmlize(symbolicName));
		templat().setString("csrfToken", htmlize(csrfTokenOfSession()));
		sendTemplate("html.upgradeForm");
	}
	endPage();
}


std::string BundleInstallHandler::bundlePath(const std::string& symbolicName)
{
	templat().clear();
	templat().setString("symbolicName", encodeQueryValue(symbolicName));
	return expandArgs(bundle()->properties().getString("html.bundlePath"));
}

