//
// JWTBundleActivator.cpp
//
// Copyright (c) 2019, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceListener.h"
#include "Poco/OSP/Web/WebServerService.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Prometheus/Registry.h"
#include "Poco/Prometheus/ThreadPoolCollector.h"
#include "Poco/Prometheus/ProcessCollector.h"
#include "Poco/Prometheus/IntCounter.h"
#include "Poco/Prometheus/Counter.h"
#include "Poco/Prometheus/Histogram.h"
#include "Poco/Prometheus/MetricsServer.h"
#include "Poco/Util/Timer.h"
#include "Poco/Util/TimerTask.h"
#include "Poco/NumberFormatter.h"
#include "Poco/SharedPtr.h"
#include "Poco/ClassLibrary.h"
#include "Poco/Delegate.h"
#include "Poco/String.h"
#include "WebServerCollector.h"
#include "WebSessionCollector.h"
#include "StringExporter.h"
#include <vector>
#include <map>


using Poco::OSP::BundleContext;
using Poco::OSP::Service;
using Poco::OSP::ServiceRef;
using Poco::OSP::PreferencesService;
using Poco::OSP::Properties;
using namespace std::string_literals;


namespace Poco {
namespace OSP {
namespace Prometheus {


class LogMetricsTask: public Poco::Util::TimerTask
{
public:
	void run()
	{
		StringExporter exporter;
		Poco::Prometheus::Registry::defaultRegistry().exportTo(exporter);
		std::string metrics = exporter.toString();
		Poco::trimRightInPlace(metrics);
		_logger.information(metrics);
	}

private:
	Poco::Logger& _logger = Poco::Logger::get("Metrics"s);
};


class BundleActivator: public Poco::OSP::BundleActivator
{
public:
	void start(BundleContext::Ptr pContext)
	{
		_pContext = pContext;
		Poco::OSP::PreferencesService::Ptr pPrefs = Poco::OSP::ServiceFinder::find<Poco::OSP::PreferencesService>(pContext);

		const bool enable = pPrefs->configuration()->getBool("prometheus.enable"s, false);
		const Poco::UInt16 port = pPrefs->configuration()->getUInt16("prometheus.port"s, 9100);
		const std::string host = pPrefs->configuration()->getString("prometheus.host"s, "0.0.0.0"s);
		const std::string path = pPrefs->configuration()->getString("prometheus.path"s, "/metrics"s);
		const bool logMetricsEnable = pPrefs->configuration()->getBool("prometheus.log.enable"s, false);
		const long logMetricsInterval = 1000*pPrefs->configuration()->getInt("prometheus.log.interval"s, 30);

		if (enable)
		{
			_generalCollectors.emplace_back(std::make_unique<Poco::Prometheus::ThreadPoolCollector>());
			_generalCollectors.emplace_back(std::make_unique<Poco::Prometheus::ProcessCollector>());
			createWebCollectors(pContext);

			if (port != 0)
			{
				Poco::Net::SocketAddress address(host, port);
				Poco::Net::ServerSocket socket(address);
				Poco::Net::HTTPServerParams::Ptr pParams = new Poco::Net::HTTPServerParams;
				pParams->setMaxQueued(pPrefs->configuration()->getInt("prometheus.server.maxQueued"s, 8));
				pParams->setMaxThreads(pPrefs->configuration()->getInt("prometheus.server.maxThreads"s, 2));
				pParams->setKeepAlive(pPrefs->configuration()->getBool("prometheus.server.keepAlive"s, false));
				pParams->setKeepAliveTimeout(Poco::Timespan(pPrefs->configuration()->getInt("prometheus.server.keepAliveTime"s, 10)));
				pParams->setMaxKeepAliveRequests(pPrefs->configuration()->getInt("prometheus.server.maxKeepAlive"s, 10));
				_pMetricsServer.reset(new Poco::Prometheus::MetricsServer(Poco::Prometheus::Registry::defaultRegistry(), socket, pParams, path));
				_pMetricsServer->start();
			}

			if (logMetricsEnable)
			{
				_pTimer = std::make_unique<Poco::Util::Timer>();
				_pTimer->scheduleAtFixedRate(new LogMetricsTask, logMetricsInterval, logMetricsInterval);
			}
		}
	}

	void stop(BundleContext::Ptr pContext)
	{
		// Unsubscribe from requestHandled BEFORE tearing down the counters or
		// destroying this activator. Otherwise, an in-flight HTTP request can
		// fire the event after the counters are gone (or this is destroyed)
		// and crash the worker thread.
		if (_pWebServerDispatcher)
		{
			_pWebServerDispatcher->requestHandled -= Poco::delegate(this, &BundleActivator::onWebServerDispatcherRequestHandled);
		}

		if (_pMetricsServer)
		{
			_pMetricsServer->stop();
			_pMetricsServer.reset();
		}
		_pTimer.reset();
		_pWebServerDispatcherRequestCounter.reset();
		_pWebServerDispatcherRequestByStatusCounter.reset();
		_pWebServerDispatcherRequestDurationHistogram.reset();
		_generalCollectors.clear();
		_webServerDispatcherCollector.reset();
		_webServerCollectors.clear();
		_pWebServerDispatcherListener.reset();
		_pWebSessionManagerListener.reset();
		_pWebServerListener.reset();
		_pWebServerDispatcher.reset();
		_pContext.reset();
	}

protected:
	void createWebCollectors(Poco::OSP::BundleContext::Ptr pContext)
	{
		_pWebServerDispatcherListener = pContext->registry().createListener(
			"name == \"osp.web.dispatcher\""s,
			Poco::delegate(this, &BundleActivator::onWebServerDispatcherRegistered),
			Poco::delegate(this, &BundleActivator::onWebServerDispatcherUnregistered)
		);

		_pWebSessionManagerListener = pContext->registry().createListener(
			"name == \"osp.web.session\""s,
			Poco::delegate(this, &BundleActivator::onWebSessionManagerRegistered),
			Poco::delegate(this, &BundleActivator::onWebSessionManagerUnregistered)
		);

		_pWebServerListener = pContext->registry().createListener(
			"name == \"osp.web.server\" || name == \"osp.web.server.secure\""s,
			Poco::delegate(this, &BundleActivator::onWebServerRegistered),
			Poco::delegate(this, &BundleActivator::onWebServerUnregistered)
		);
	}

	void onWebServerDispatcherRegistered(const Poco::OSP::ServiceRef::Ptr& pServiceRef)
	{
		_pWebServerDispatcher = pServiceRef->castedInstance<Poco::OSP::Web::WebServerDispatcher>();
		if (_pWebServerDispatcher)
		{
			_pContext->logger().information("Creating ThreadPoolCollector for WebServerDispatcher thread pool."s);
			_webServerDispatcherCollector.reset(new Poco::Prometheus::ThreadPoolCollector(_pWebServerDispatcher->threadPool()));

			_pWebServerDispatcherRequestCounter = std::make_unique<Poco::Prometheus::IntCounter>("osp_web_requests_total"s);
			_pWebServerDispatcherRequestCounter->help("Total number of requests handled"s);
			_pWebServerDispatcherRequestByStatusCounter = std::make_unique<Poco::Prometheus::Counter>(
				"osp_web_requests_by_status_total"s,
				Poco::Prometheus::Counter::Params {
					/*.help = */"Total number of requests handled by response status"s,
					/*.labelNames =*/ {"status"s}
				});
			_pWebServerDispatcherRequestDurationHistogram = std::make_unique<Poco::Prometheus::Histogram>(
				"osp_web_request_duration_seconds"s,
				Poco::Prometheus::Histogram::Params {
					/*.help =*/ "Request durations"s,
					/*.labelNames =*/ {},
					/*.buckets =*/ {0.01, 0.1, 0.5, 1.0, 5.0, 10.0, 30.0}
				});

			_pWebServerDispatcher->requestHandled += Poco::delegate(this, &BundleActivator::onWebServerDispatcherRequestHandled);
		}
	}

	void onWebServerDispatcherUnregistered(const Poco::OSP::ServiceRef::Ptr& pServiceRef)
	{
		// stop() may have already unsubscribed; guard against double -=.
		if (_pWebServerDispatcher)
		{
			_pWebServerDispatcher->requestHandled -= Poco::delegate(this, &BundleActivator::onWebServerDispatcherRequestHandled);
		}
		_webServerDispatcherCollector.reset();
		_pWebServerDispatcher.reset();
	}

	void onWebSessionManagerRegistered(const Poco::OSP::ServiceRef::Ptr& pServiceRef)
	{
		Poco::AutoPtr<Poco::OSP::Web::WebSessionManager> pWSM = pServiceRef->castedInstance<Poco::OSP::Web::WebSessionManager>();
		if (pWSM)
		{
			_pContext->logger().information("Creating WebSessionCollector for WebSessionManager."s);
			_webSessionManagerCollector.reset(new WebSessionCollector(pWSM));
		}
	}

	void onWebSessionManagerUnregistered(const Poco::OSP::ServiceRef::Ptr& pServiceRef)
	{
		_webSessionManagerCollector.reset();
	}

	void onWebServerRegistered(const Poco::OSP::ServiceRef::Ptr& pServiceRef)
	{
		Poco::AutoPtr<Poco::OSP::Web::WebServerService> pWSS = pServiceRef->castedInstance<Poco::OSP::Web::WebServerService>();
		if (pWSS)
		{
			_pContext->logger().information("Creating WebServerCollector for WebServerService at %s:%hu."s, pWSS->host(), pWSS->port());
			_webServerCollectors[pServiceRef].reset(new WebServerCollector(pWSS));
		}
	}

	void onWebServerUnregistered(const Poco::OSP::ServiceRef::Ptr& pServiceRef)
	{
		_webServerCollectors.erase(pServiceRef);
	}

	void onWebServerDispatcherRequestHandled(const Poco::OSP::Web::WebServerDispatcher::RequestHandledEvent& event)
	{
		// Defends against a notification snapshot taken before stop()'s -= ran:
		// AbstractEvent::notify() copies the delegate list under a lock and
		// then iterates after releasing it, so a concurrent unsubscribe can
		// lose the race and we still get called with counters torn down.
		if (!_pWebServerDispatcherRequestCounter) return;
		_pWebServerDispatcherRequestCounter->inc();
		_pWebServerDispatcherRequestByStatusCounter->labels({Poco::NumberFormatter::format(static_cast<int>(event.response.getStatus()))}).inc();
		_pWebServerDispatcherRequestDurationHistogram->observe(event.duration);
	}

private:
	Poco::OSP::BundleContext::Ptr _pContext;
	Poco::OSP::Web::WebServerDispatcher::Ptr _pWebServerDispatcher;
	Poco::OSP::ServiceListener::Ptr _pWebServerDispatcherListener;
	Poco::OSP::ServiceListener::Ptr _pWebServerListener;
	Poco::OSP::ServiceListener::Ptr _pWebSessionManagerListener;
	std::unique_ptr<Poco::Prometheus::IntCounter> _pWebServerDispatcherRequestCounter;
	std::unique_ptr<Poco::Prometheus::Counter> _pWebServerDispatcherRequestByStatusCounter;
	std::unique_ptr<Poco::Prometheus::Histogram> _pWebServerDispatcherRequestDurationHistogram;
	std::unique_ptr<Poco::Prometheus::MetricsServer> _pMetricsServer;
	std::vector<std::unique_ptr<Poco::Prometheus::Collector>> _generalCollectors;
	std::unique_ptr<Poco::Prometheus::Collector> _webServerDispatcherCollector;
	std::unique_ptr<Poco::Prometheus::Collector> _webSessionManagerCollector;
	std::map<Poco::OSP::ServiceRef::Ptr, std::unique_ptr<Poco::Prometheus::Collector>> _webServerCollectors;
	std::unique_ptr<Poco::Util::Timer> _pTimer;
};


} } } // namespace Poco::OSP::Prometheus


POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
	POCO_EXPORT_CLASS(Poco::OSP::Prometheus::BundleActivator)
POCO_END_MANIFEST
