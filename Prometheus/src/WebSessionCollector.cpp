//
// WebSessionCollector.cpp
//
// Copyright (c) 2024, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "WebSessionCollector.h"
#include "Poco/Prometheus/Exporter.h"
#include "Poco/NumberFormatter.h"


using namespace std::string_literals;


namespace Poco::OSP::Prometheus {


WebSessionCollector::WebSessionCollector(Poco::AutoPtr<Poco::OSP::Web::WebSessionManager> pWebSessionManager):
	Poco::Prometheus::Collector("osp_web_session"s),
	_pWebSessionManager(pWebSessionManager)
{
	buildMetrics();
}


void WebSessionCollector::exportTo(Poco::Prometheus::Exporter& exporter) const
{
	_activeSessions->exportTo(exporter);
}


void WebSessionCollector::buildMetrics()
{
	Poco::OSP::Web::WebSessionManager::Ptr pWSM = _pWebSessionManager;

	_activeSessions = std::make_unique<Poco::Prometheus::CallbackIntGauge>(
		"osp_web_active_sessions"s,
		"Number of active sessions"s,
		nullptr,
		[pWSM]() mutable
		{
			return pWSM->countSessions();
		});
}


} // namespace Poco::OSP::Prometheus
