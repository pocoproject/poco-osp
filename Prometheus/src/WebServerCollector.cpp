//
// WebServerCollector.cpp
//
// Copyright (c) 2024, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "WebServerCollector.h"
#include "Poco/Prometheus/Exporter.h"
#include "Poco/Net/HTTPBufferAllocator.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Format.h"


using namespace std::string_literals;


namespace Poco {
namespace OSP {
namespace Prometheus {


WebServerCollector::WebServerCollector(Poco::AutoPtr<Poco::OSP::Web::WebServerService> pWebServerService):
	Poco::Prometheus::Collector(Poco::format("osp_web_server_%hu"s, pWebServerService->port())),
	_pWebServerService(pWebServerService)
{
	buildMetrics();
}


void WebServerCollector::exportTo(Poco::Prometheus::Exporter& exporter) const
{
	const std::vector<std::string> labelNames{
		"host"s, 
		"port"s, 
		"protocol"s
	};
	const std::vector<std::string> labelValues{
		_pWebServerService->host(), 
		Poco::NumberFormatter::format(_pWebServerService->port()), 
		_pWebServerService->secure() ? "https"s : "http"s
	};

	for (const auto& p: _gauges)
	{
		exporter.writeHeader(*p);
		exporter.writeSample(*p, labelNames, labelValues, p->value(), 0);
	}

	for (const auto& p: _counters)
	{
		exporter.writeHeader(*p);
		exporter.writeSample(*p, labelNames, labelValues, p->value(), 0);
	}
}


void WebServerCollector::buildMetrics()
{
	Poco::AutoPtr<Poco::OSP::Web::WebServerService> pWebServerService = _pWebServerService;

	_gauges.push_back(std::make_unique<Poco::Prometheus::CallbackIntGauge>(
		"osp_web_current_connections"s,
		"Number of current connections"s,
		nullptr,
		[pWebServerService]()
		{
			return pWebServerService->currentConnections();
		}));

	_gauges.push_back(std::make_unique<Poco::Prometheus::CallbackIntGauge>(
		"osp_web_current_threads"s,
		"Number of current threads used"s,
		nullptr,
		[pWebServerService]()
		{
			return pWebServerService->currentThreads();
		}));

	_gauges.push_back(std::make_unique<Poco::Prometheus::CallbackIntGauge>(
		"osp_web_max_concurrent_connections"s,
		"Maximum number of concurrent connections"s,
		nullptr,
		[pWebServerService]()
		{
			return pWebServerService->maxConcurrentConnections();
		}));

	_gauges.push_back(std::make_unique<Poco::Prometheus::CallbackIntGauge>(
		"osp_web_queued_connections"s,
		"Number of currently queued connections"s,
		nullptr,
		[pWebServerService]()
		{
			return pWebServerService->queuedConnections();
		}));

	_counters.push_back(std::make_unique<Poco::Prometheus::CallbackIntCounter>(
		"osp_web_total_connections"s,
		"Total number of handled connections"s,
		nullptr,
		[pWebServerService]()
		{
			return pWebServerService->totalConnections();
		}));

	_counters.push_back(std::make_unique<Poco::Prometheus::CallbackIntCounter>(
		"osp_web_refused_connections"s,
		"Total number of refused connections"s,
		nullptr,
		[pWebServerService]()
		{
			return pWebServerService->refusedConnections();
		}));

	_gauges.push_back(std::make_unique<Poco::Prometheus::CallbackIntGauge>(
		"osp_web_buffer_pool_allocated_memory"s,
		"Total memory allocated in HTTP buffer pool"s,
		nullptr,
		[]()
		{
			return Poco::Net::HTTPBufferAllocator::pool().blockSize()*Poco::Net::HTTPBufferAllocator::pool().allocated();
		}));

	_gauges.push_back(std::make_unique<Poco::Prometheus::CallbackIntGauge>(
		"osp_web_buffer_pool_available_memory"s,
		"Total memory available in HTTP buffer pool"s,
		nullptr,
		[]()
		{
			return Poco::Net::HTTPBufferAllocator::pool().blockSize()*Poco::Net::HTTPBufferAllocator::pool().available();
		}));
}


} } } // namespace Poco::OSP::Prometheus
