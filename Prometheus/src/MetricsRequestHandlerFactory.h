//
// MetricsRequestHandlerFactory.h
//
// Definition of the MetricsRequestHandlerFactory class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Prometheus_MetricsRequestHandlerFactory_INCLUDED
#define OSP_Prometheus_MetricsRequestHandlerFactory_INCLUDED


#include "Poco/OSP/Auth/ClientAddressKey.h"
#include "Poco/OSP/Auth/LoginThrottle.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPResponse.h"
#include <atomic>
#include <chrono>
#include <string>


namespace Poco::OSP::Prometheus {


class MetricsRequestHandlerFactory: public Poco::Net::HTTPRequestHandlerFactory
	/// Creates the request handlers of the metrics server: for the metrics
	/// path, a handler that sends the metrics of the default registry, and
	/// for every other path, a handler that sends 404 Not Found.
	///
	/// If a permission is given, a request for the metrics must carry HTTP
	/// Basic credentials of a user with this permission, which the auth service
	/// with the given name checks. Otherwise the answer is 401 Unauthorized for
	/// a request without valid credentials, 403 Forbidden for a user without
	/// the permission, and 503 Service Unavailable while the auth service is
	/// not available.
	///
	/// The password checks are limited per client address by a LoginThrottle:
	/// while a client address is refused, the answer is 429 Too Many Requests
	/// with a Retry-After header, and the auth service does not check the password.
{
public:
	MetricsRequestHandlerFactory() = delete;

	MetricsRequestHandlerFactory(
		Poco::OSP::BundleContext::Ptr pContext,
		const std::string& path,
		const std::string& authServiceName,
		const std::string& permission,
		const Poco::OSP::Auth::LoginThrottleConfig& loginThrottle);
		/// Creates the MetricsRequestHandlerFactory. An empty permission allows
		/// every request. loginThrottle holds the settings of the limit of password
		/// checks per client address.

	~MetricsRequestHandlerFactory() override;
		/// Destroys the MetricsRequestHandlerFactory.

	MetricsRequestHandlerFactory(const MetricsRequestHandlerFactory&) = delete;
	MetricsRequestHandlerFactory(MetricsRequestHandlerFactory&&) = delete;
	MetricsRequestHandlerFactory& operator = (const MetricsRequestHandlerFactory&) = delete;
	MetricsRequestHandlerFactory& operator = (MetricsRequestHandlerFactory&&) = delete;

	// Poco::Net::HTTPRequestHandlerFactory
	[[nodiscard]] Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;

private:
	[[nodiscard]] Poco::Net::HTTPResponse::HTTPStatus authorize(
		const Poco::Net::HTTPServerRequest& request,
		std::chrono::seconds& retryAfter);
		/// Returns HTTP_OK if the request may read the metrics, or the status
		/// of the answer otherwise. For HTTP_TOO_MANY_REQUESTS, retryAfter is set
		/// to the seconds until the client address may try again.

	Poco::OSP::BundleContext::Ptr _pContext;
	const std::string _path;
	const std::string _authServiceName;
	const std::string _permission;
	std::atomic<bool> _authServiceMissing{false};
		/// True if the auth service was not available at the last request,
		/// so that its absence is logged once, not for every request.
	const Poco::OSP::Auth::ExemptAddresses _exemptAddresses;
	Poco::OSP::Auth::LoginThrottle _loginThrottle;
};


} // namespace Poco::OSP::Prometheus


#endif // OSP_Prometheus_MetricsRequestHandlerFactory_INCLUDED
