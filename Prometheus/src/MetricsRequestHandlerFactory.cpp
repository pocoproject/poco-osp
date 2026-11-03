//
// MetricsRequestHandlerFactory.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "MetricsRequestHandlerFactory.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/Prometheus/MetricsRequestHandler.h"
#include "Poco/Prometheus/Registry.h"
#include "Poco/Net/HTTPBasicCredentials.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Exception.h"
#include "Poco/String.h"
#include <string>


using namespace std::string_literals;


namespace Poco::OSP::Prometheus {


namespace
{
	class StatusRequestHandler: public Poco::Net::HTTPRequestHandler
		/// Sends a response with the given status and no content.
	{
	public:
		StatusRequestHandler() = delete;

		explicit StatusRequestHandler(Poco::Net::HTTPResponse::HTTPStatus status):
			_status(status)
		{
		}

		~StatusRequestHandler() override = default;

		StatusRequestHandler(const StatusRequestHandler&) = delete;
		StatusRequestHandler(StatusRequestHandler&&) = delete;
		StatusRequestHandler& operator = (const StatusRequestHandler&) = delete;
		StatusRequestHandler& operator = (StatusRequestHandler&&) = delete;

		void handleRequest(Poco::Net::HTTPServerRequest& /*request*/, Poco::Net::HTTPServerResponse& response) override
		{
			response.setStatusAndReason(_status);
			if (_status == Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED)
			{
				response.requireAuthentication("metrics"s);
			}
			response.setContentLength(0);
			response.send();
		}

	private:
		const Poco::Net::HTTPResponse::HTTPStatus _status;
	};
}


MetricsRequestHandlerFactory::MetricsRequestHandlerFactory(Poco::OSP::BundleContext::Ptr pContext, const std::string& path, const std::string& authServiceName, const std::string& permission):
	_pContext(pContext),
	_path(path),
	_authServiceName(authServiceName),
	_permission(permission)
{
}


MetricsRequestHandlerFactory::~MetricsRequestHandlerFactory() = default;


Poco::Net::HTTPRequestHandler* MetricsRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest& request)
{
	if (request.getURI() != _path) return new StatusRequestHandler(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);

	const Poco::Net::HTTPResponse::HTTPStatus status = authorize(request);
	if (status != Poco::Net::HTTPResponse::HTTP_OK) return new StatusRequestHandler(status);

	return new Poco::Prometheus::MetricsRequestHandler(Poco::Prometheus::Registry::defaultRegistry());
}


Poco::Net::HTTPResponse::HTTPStatus MetricsRequestHandlerFactory::authorize(const Poco::Net::HTTPServerRequest& request) const
{
	if (_permission.empty()) return Poco::Net::HTTPResponse::HTTP_OK;

	// ServiceFinder::findByName() throws if the service is not registered.
	Poco::OSP::ServiceRef::Ptr pAuthServiceRef = _pContext->registry().findByName(_authServiceName);
	Poco::OSP::Auth::AuthService::Ptr pAuthService;
	if (pAuthServiceRef != nullptr) pAuthService = pAuthServiceRef->castedInstance<Poco::OSP::Auth::AuthService>();
	if (pAuthService == nullptr)
	{
		if (_authServiceMissing.exchange(true))
			_pContext->logger().debug("The metrics are not available: the auth service %s is not available."s, _authServiceName);
		else
			_pContext->logger().warning("The metrics are not available: the auth service %s is not available."s, _authServiceName);
		return Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE;
	}
	if (_authServiceMissing.exchange(false))
	{
		_pContext->logger().information("The metrics are available again: the auth service %s is available."s, _authServiceName);
	}

	std::string username;
	std::string password;
	try
	{
		const Poco::Net::HTTPBasicCredentials credentials(request);
		username = credentials.getUsername();
		password = credentials.getPassword();
	}
	catch (Poco::Exception&)
	{
		// The request has no Basic credentials, or they are malformed.
		return Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED;
	}
	const bool authenticated = pAuthService->authenticate(username, password);
	Poco::secureClear(password);
	if (!authenticated) return Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED;
	if (!pAuthService->authorize(username, _permission)) return Poco::Net::HTTPResponse::HTTP_FORBIDDEN;
	return Poco::Net::HTTPResponse::HTTP_OK;
}


} // namespace Poco::OSP::Prometheus
