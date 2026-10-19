//
// WebServerCollector.h
//
// Copyright (c) 2024, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_Prometheus_WebServerCollector_INCLUDED
#define OSP_Prometheus_WebServerCollector_INCLUDED


#include "Poco/Prometheus/Collector.h"
#include "Poco/Prometheus/CallbackMetric.h"
#include "Poco/OSP/Web/WebServerService.h"
#include "Poco/AutoPtr.h"
#include <memory>


namespace Poco {
namespace OSP {
namespace Prometheus {


class WebServerCollector: public Poco::Prometheus::Collector
{
public:
	explicit WebServerCollector(Poco::AutoPtr<Poco::OSP::Web::WebServerService> pWebServerService);
		/// Creates a default WebServerCollector.

	~WebServerCollector() = default;
		/// Destroys the WebServerCollector.

	// Collector
	void exportTo(Poco::Prometheus::Exporter& exporter) const override;
		/// Writes the given Metric to the Exporter.

protected:
	void buildMetrics();

private:
	using GaugePtr = std::unique_ptr<Poco::Prometheus::CallbackIntGauge>;
	using CounterPtr = std::unique_ptr<Poco::Prometheus::CallbackIntCounter>;
	std::vector<GaugePtr> _gauges;
	std::vector<CounterPtr> _counters;
	Poco::AutoPtr<Poco::OSP::Web::WebServerService> _pWebServerService;
};


} } } // namespace Poco::OSP::Prometheus


#endif // OSP_Prometheus_WebServerCollector_INCLUDED
