//
// WebServerCollector.cpp
//
// Copyright (c) 2024, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "WebServerCollector.h"
#include "MetricFamily.h"
#include "Poco/OSP/Web/WebServerService.h"
#include "Poco/Prometheus/Collector.h"
#include "Poco/Prometheus/Exporter.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Mutex.h"
#include "Poco/Types.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>


using namespace std::string_literals;


namespace Poco::OSP::Prometheus {


namespace
{
	[[nodiscard]] std::string serverLabel(const Poco::OSP::Web::WebServerService& webServerService)
	{
		std::string label(webServerService.secure() ? "https://"s : "http://"s);
		label += webServerService.host();
		label += ':';
		label += Poco::NumberFormatter::format(webServerService.port());
		return label;
	}
}


WebServerCollector::WebServerCollector():
	Poco::Prometheus::Collector("osp_web_server"s)
{
	using Type = Poco::Prometheus::Metric::Type;
	using WebServerService = Poco::OSP::Web::WebServerService;

	addMetric(Type::GAUGE, "osp_web_current_connections"s, "Number of current connections"s,
		[](const WebServerService& service) { return service.currentConnections(); });
	addMetric(Type::GAUGE, "osp_web_current_threads"s, "Number of current threads used"s,
		[](const WebServerService& service) { return service.currentThreads(); });
	addMetric(Type::GAUGE, "osp_web_max_concurrent_connections"s, "Maximum number of concurrent connections"s,
		[](const WebServerService& service) { return service.maxConcurrentConnections(); });
	addMetric(Type::GAUGE, "osp_web_queued_connections"s, "Number of currently queued connections"s,
		[](const WebServerService& service) { return service.queuedConnections(); });
	addMetric(Type::COUNTER, "osp_web_total_connections"s, "Total number of handled connections"s,
		[](const WebServerService& service) { return service.totalConnections(); });
	addMetric(Type::COUNTER, "osp_web_refused_connections"s, "Total number of refused connections"s,
		[](const WebServerService& service) { return service.refusedConnections(); });
}


WebServerCollector::~WebServerCollector() = default;


void WebServerCollector::addServer(const WebServerPtr& pWebServerService)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	_servers.push_back(pWebServerService);
}


void WebServerCollector::removeServer(const WebServerPtr& pWebServerService)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	_servers.erase(std::remove(_servers.begin(), _servers.end(), pWebServerService), _servers.end());
}


void WebServerCollector::exportTo(Poco::Prometheus::Exporter& exporter) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	if (_servers.empty()) return;

	const std::vector<std::string> labelNames{"server"s};
	for (const auto& metric: _metrics)
	{
		exporter.writeHeader(*metric.pMetric);
		for (const auto& pServer: _servers)
		{
			exporter.writeSample(*metric.pMetric, labelNames, {serverLabel(*pServer)}, metric.value(*pServer), 0);
		}
	}
}


void WebServerCollector::addMetric(Poco::Prometheus::Metric::Type type, const std::string& name,
	const std::string& help, std::function<Poco::Int64(const Poco::OSP::Web::WebServerService&)> value)
{
	_metrics.push_back(ServerMetric{std::make_unique<MetricFamily>(type, name, help), std::move(value)});
}


} // namespace Poco::OSP::Prometheus
