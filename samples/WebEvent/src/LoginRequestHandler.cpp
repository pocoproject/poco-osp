//
// LoginRequestHandler.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Web/WebRequestHandlerFactory.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/ClassLibrary.h"
#include "Poco/Exception.h"
#include <string>
#include <utility>


using Poco::Net::HTMLForm;
using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPResponse;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPServerResponse;
using Poco::OSP::Auth::AuthService;
using Poco::OSP::Web::WebRequestHandlerFactory;
using Poco::OSP::Web::WebSession;
using Poco::OSP::Web::WebSessionManager;
using Poco::OSP::BundleContext;
using Poco::OSP::ServiceRef;


namespace
{
	class LoginRequestHandler: public HTTPRequestHandler
		/// Logs a browser in to the web session of the sample. The form of a POST request gives the user name
		/// ("username") and the password ("password"), which the authentication service checks. If they are
		/// valid, the session gets a new ID and the account name as its "username", and the response is
		/// 204 No Content; otherwise it is 401 Unauthorized.
	{
	public:
		LoginRequestHandler(
			WebSessionManager::Ptr pWebSessionManager,
			const std::string& sessionName,
			int sessionTimeout,
			const std::string& authServiceName,
			BundleContext::Ptr pContext):
			_pWebSessionManager(std::move(pWebSessionManager)),
			_sessionName(sessionName),
			_sessionTimeout(sessionTimeout),
			_authServiceName(authServiceName),
			_pContext(std::move(pContext))
		{
		}

		void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) override
		{
			HTMLForm form;
			try
			{
				form.load(request, request.stream());
			}
			catch (Poco::Exception&)
			{
				sendText(response, HTTPResponse::HTTP_BAD_REQUEST, "The login form is malformed.");
				return;
			}

			ServiceRef::Ptr pAuthServiceRef = _pContext->registry().findByName(_authServiceName);
			if (pAuthServiceRef == nullptr)
			{
				sendText(response, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "No authentication service is available.");
				return;
			}
			// The user may log in under another name of the account; the session keeps the account name.
			std::string accountName;
			if (!pAuthServiceRef->castedInstance<AuthService>()->authenticateAccount(
				form.get("username", ""), form.get("password", ""), accountName))
			{
				sendText(response, HTTPResponse::HTTP_UNAUTHORIZED, "The user name or the password is wrong.");
				return;
			}

			// A new session ID, so that an ID known before the login does not give access to the session.
			WebSession::Ptr pSession = _pWebSessionManager->get(_sessionName, request, _sessionTimeout, _pContext);
			pSession = _pWebSessionManager->renew(_sessionName, request, pSession);
			pSession->setValue<std::string>("username", accountName);
			response.setStatusAndReason(HTTPResponse::HTTP_NO_CONTENT);
			response.setContentLength(0);
			response.send();
		}

	private:
		static void sendText(HTTPServerResponse& response, HTTPResponse::HTTPStatus status, const std::string& text)
			/// Sends the text as a plain text response with the given status.
		{
			response.setStatusAndReason(status);
			response.setContentType("text/plain");
			response.sendBuffer(text.data(), text.size());
		}

		WebSessionManager::Ptr _pWebSessionManager;
		std::string _sessionName;
		int _sessionTimeout;
		std::string _authServiceName;
		BundleContext::Ptr _pContext;
	};


	class LoginRequestHandlerFactory: public WebRequestHandlerFactory
	{
	public:
		LoginRequestHandlerFactory() = default;

		[[nodiscard]] HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& /*request*/) override
		{
			if (_pWebSessionManager == nullptr)
			{
				return nullptr;
			}
			return new LoginRequestHandler(_pWebSessionManager, _sessionName, _sessionTimeout, _authServiceName,
				context());
		}

	protected:
		void initImpl() override
		{
			_sessionName = context()->thisBundle()->properties().getString("session");
			_sessionTimeout = context()->thisBundle()->properties().getInt("sessionTimeout");
			// The web server authenticates the requests with this service.
			_authServiceName = Poco::OSP::ServiceFinder::find<Poco::OSP::PreferencesService>(context())->configuration()
				->getString("osp.web.authServiceName", "osp.auth");
			ServiceRef::Ptr pWebSessionManagerRef = context()->registry().findByName(WebSessionManager::SERVICE_NAME);
			if (pWebSessionManagerRef != nullptr)
			{
				_pWebSessionManager = pWebSessionManagerRef->castedInstance<WebSessionManager>();
			}
			else
			{
				context()->logger().error(
					"The WebSessionManager service is not available. The login of the WebEvent sample is not served.");
			}
		}

	private:
		WebSessionManager::Ptr _pWebSessionManager;
		std::string _sessionName;
		int _sessionTimeout = 0;
		std::string _authServiceName;
	};
}


POCO_BEGIN_NAMED_MANIFEST(WebServer, WebRequestHandlerFactory)
	POCO_EXPORT_CLASS(LoginRequestHandlerFactory)
POCO_END_MANIFEST
