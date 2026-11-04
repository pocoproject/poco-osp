//
// WebServerCollector.h
//
// Copyright (c) 2024, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Prometheus_WebServerCollector_INCLUDED
#define OSP_Prometheus_WebServerCollector_INCLUDED


#include "Poco/Prometheus/Collector.h"
#include "Poco/Prometheus/Metric.h"
#include "Poco/OSP/Web/WebServerService.h"
#include "Poco/AutoPtr.h"
#include "Poco/Mutex.h"
#include <functional>
#include <memory>
#include <vector>


namespace Poco::OSP::Prometheus {


class WebServerCollector: public Poco::Prometheus::Collector
	/// Exports the connection and thread metrics of the web servers, with one
	/// sample per server in each metric. The label "server" names the server
	/// in the form protocol://host:port.
{
public:
	using WebServerPtr = Poco::AutoPtr<Poco::OSP::Web::WebServerService>;

	WebServerCollector();
		/// Creates a WebServerCollector without web servers and registers it
		/// with the default registry.

	~WebServerCollector() override;
		/// Destroys the WebServerCollector.

	WebServerCollector(const WebServerCollector&) = delete;
	WebServerCollector(WebServerCollector&&) = delete;
	WebServerCollector& operator = (const WebServerCollector&) = delete;
	WebServerCollector& operator = (WebServerCollector&&) = delete;

	void addServer(const WebServerPtr& pWebServerService);
		/// Adds the samples of the given web server.

	void removeServer(const WebServerPtr& pWebServerService);
		/// Removes the samples of the given web server.

	// Collector
	void exportTo(Poco::Prometheus::Exporter& exporter) const override;
		/// Writes the metrics of all web servers to the Exporter.

private:
	struct ServerMetric
	{
		std::unique_ptr<Poco::Prometheus::Metric> pMetric;
		std::function<Poco::Int64(const Poco::OSP::Web::WebServerService&)> value;
	};

	void addMetric(Poco::Prometheus::Metric::Type type, const std::string& name, const std::string& help,
		std::function<Poco::Int64(const Poco::OSP::Web::WebServerService&)> value);

	std::vector<ServerMetric> _metrics;
	std::vector<WebServerPtr> _servers;
	mutable Poco::FastMutex _mutex;
};


} // namespace Poco::OSP::Prometheus


#endif // OSP_Prometheus_WebServerCollector_INCLUDED
