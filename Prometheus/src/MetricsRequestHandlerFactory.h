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


#include "Poco/OSP/BundleContext.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPResponse.h"
#include <atomic>
#include <string>


namespace Poco {
namespace OSP {
namespace Prometheus {


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
{
public:
	MetricsRequestHandlerFactory() = delete;

	MetricsRequestHandlerFactory(Poco::OSP::BundleContext::Ptr pContext, const std::string& path, const std::string& authServiceName, const std::string& permission);
		/// Creates the MetricsRequestHandlerFactory. An empty permission allows
		/// every request.

	~MetricsRequestHandlerFactory() override;
		/// Destroys the MetricsRequestHandlerFactory.

	MetricsRequestHandlerFactory(const MetricsRequestHandlerFactory&) = delete;
	MetricsRequestHandlerFactory(MetricsRequestHandlerFactory&&) = delete;
	MetricsRequestHandlerFactory& operator = (const MetricsRequestHandlerFactory&) = delete;
	MetricsRequestHandlerFactory& operator = (MetricsRequestHandlerFactory&&) = delete;

	// Poco::Net::HTTPRequestHandlerFactory
	[[nodiscard]] Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;

private:
	[[nodiscard]] Poco::Net::HTTPResponse::HTTPStatus authorize(const Poco::Net::HTTPServerRequest& request) const;
		/// Returns HTTP_OK if the request may read the metrics, or the status
		/// of the answer otherwise.

	Poco::OSP::BundleContext::Ptr _pContext;
	const std::string _path;
	const std::string _authServiceName;
	const std::string _permission;
	mutable std::atomic<bool> _authServiceMissing{false};
		/// True if the auth service was not available at the last request,
		/// so that its absence is logged once, not for every request.
};


} } } // namespace Poco::OSP::Prometheus


#endif // OSP_Prometheus_MetricsRequestHandlerFactory_INCLUDED
