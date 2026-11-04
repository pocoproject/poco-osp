//
// WebSessionCollector.h
//
// Copyright (c) 2024, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_Prometheus_WebSessionCollector_INCLUDED
#define OSP_Prometheus_WebSessionCollector_INCLUDED


#include "Poco/Prometheus/Collector.h"
#include "Poco/Prometheus/CallbackMetric.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/AutoPtr.h"
#include <memory>


namespace Poco::OSP::Prometheus {


class WebSessionCollector: public Poco::Prometheus::Collector
{
public:
	explicit WebSessionCollector(Poco::AutoPtr<Poco::OSP::Web::WebSessionManager> pWebSessionManager);
		/// Creates the WebSessionCollector for the sessions of the given
		/// WebSessionManager and registers it with the default registry.

	~WebSessionCollector() override = default;
		/// Destroys the WebSessionCollector.

	// Collector
	void exportTo(Poco::Prometheus::Exporter& exporter) const override;
		/// Writes the number of active sessions (osp_web_active_sessions)
		/// to the Exporter.

protected:
	void buildMetrics();

private:
	using GaugePtr = std::unique_ptr<Poco::Prometheus::CallbackIntGauge>;
	GaugePtr _activeSessions;
	Poco::AutoPtr<Poco::OSP::Web::WebSessionManager> _pWebSessionManager;
};


} // namespace Poco::OSP::Prometheus


#endif // OSP_Prometheus_WebSessionCollector_INCLUDED
