//
// PrometheusBundleActivator.cpp
//
// Copyright (c) 2019, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
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
#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Prometheus/Registry.h"
#include "Poco/Prometheus/ProcessCollector.h"
#include "Poco/Prometheus/IntCounter.h"
#include "Poco/Prometheus/Counter.h"
#include "Poco/Prometheus/Histogram.h"
#include "Poco/Util/Timer.h"
#include "Poco/Util/TimerTask.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Exception.h"
#include "Poco/Format.h"
#include "Poco/Timespan.h"
#include "Poco/Types.h"
#include "Poco/SharedPtr.h"
#include "Poco/ClassLibrary.h"
#include "Poco/Delegate.h"
#include "Poco/String.h"
#include "Poco/ThreadPool.h"
#include "MetricsRequestHandlerFactory.h"
#include "ThreadPoolCollector.h"
#include "WebServerCollector.h"
#include "WebSessionCollector.h"
#include "StringExporter.h"
#include <limits>
#include <memory>
#include <vector>


using Poco::OSP::BundleContext;
using Poco::OSP::Service;
using Poco::OSP::ServiceRef;
using Poco::OSP::PreferencesService;
using Poco::OSP::Properties;
using namespace std::string_literals;


namespace Poco::OSP::Prometheus {


class LogMetricsTask: public Poco::Util::TimerTask
{
public:
	void run() override
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


struct CollectorDeleter
	/// Unregisters a collector from the default registry before it is deleted:
	/// the collectors register themselves, but do not unregister.
{
	void operator () (Poco::Prometheus::Collector* pCollector) const
	{
		Poco::Prometheus::Registry::defaultRegistry().unregisterCollector(pCollector);
		delete pCollector;
	}
};


template <class C>
using CollectorPtr = std::unique_ptr<C, CollectorDeleter>;


class BundleActivator: public Poco::OSP::BundleActivator
{
public:
	void start(BundleContext::Ptr pContext) override
	{
		_pContext = pContext;
		Poco::OSP::PreferencesService::Ptr pPrefs = Poco::OSP::ServiceFinder::find<Poco::OSP::PreferencesService>(pContext);

		const bool enable = pPrefs->configuration()->getBool("prometheus.enable"s, false);
		const Poco::UInt16 port = pPrefs->configuration()->getUInt16("prometheus.port"s, 9100);
		const std::string host = pPrefs->configuration()->getString("prometheus.host"s, "0.0.0.0"s);
		const std::string path = pPrefs->configuration()->getString("prometheus.path"s, "/metrics"s);
		const std::string permission = pPrefs->configuration()->getString("prometheus.permission"s, ""s);
		const std::string authServiceName = pPrefs->configuration()->getString("osp.web.authServiceName"s, "osp.auth"s);
		const bool logMetricsEnable = pPrefs->configuration()->getBool("prometheus.log.enable"s, false);
		const int logMetricsInterval = pPrefs->configuration()->getInt("prometheus.log.interval"s, 30);

		if (!enable) return;
		if (logMetricsEnable && (logMetricsInterval < 1 || logMetricsInterval > MAX_LOG_INTERVAL))
		{
			throw Poco::InvalidArgumentException(
				Poco::format("prometheus.log.interval must be between 1 and %d"s, MAX_LOG_INTERVAL),
				Poco::NumberFormatter::format(logMetricsInterval));
		}

		// The framework does not call stop() when start() fails.
		try
		{
			_pThreadPoolCollector.reset(new ThreadPoolCollector);
			_pThreadPoolCollector->addThreadPool(Poco::ThreadPool::defaultPool());
			_generalCollectors.push_back(CollectorPtr<Poco::Prometheus::Collector>(new Poco::Prometheus::ProcessCollector));
			_pWebServerCollector.reset(new WebServerCollector);
			createWebCollectors(pContext);

			if (port != 0)
			{
				Poco::Net::SocketAddress address(host, port);
				Poco::Net::ServerSocket socket(address);
				Poco::Net::HTTPServerParams::Ptr pParams = new Poco::Net::HTTPServerParams;
				pParams->setMaxQueued(pPrefs->configuration()->getInt("prometheus.server.maxQueued"s, 8));
				pParams->setMaxThreads(pPrefs->configuration()->getInt("prometheus.server.maxThreads"s, 2));
				pParams->setKeepAlive(pPrefs->configuration()->getBool("prometheus.server.keepAlive"s, false));
				pParams->setKeepAliveTimeout(Poco::Timespan(pPrefs->configuration()->getInt("prometheus.server.keepAliveTime"s, 10), 0));
				pParams->setMaxKeepAliveRequests(pPrefs->configuration()->getInt("prometheus.server.maxKeepAlive"s, 10));
				_pMetricsServer = std::make_unique<Poco::Net::HTTPServer>(new MetricsRequestHandlerFactory(pContext, path, authServiceName, permission), socket, pParams);
				_pMetricsServer->start();
			}

			if (logMetricsEnable)
			{
				_pTimer = std::make_unique<Poco::Util::Timer>();
				const long interval = static_cast<long>(Poco::Timespan(logMetricsInterval, 0).totalMilliseconds());
				_pTimer->scheduleAtFixedRate(new LogMetricsTask, interval, interval);
			}
		}
		catch (...)
		{
			cleanup();
			throw;
		}
	}

	void stop(BundleContext::Ptr /*pContext*/) override
	{
		cleanup();
	}

protected:
	void cleanup()
	{
		// Without the listeners, no collector is created while the others are destroyed.
		_pWebServerDispatcherListener.reset();
		_pWebSessionManagerListener.reset();
		_pWebServerListener.reset();

		// Unsubscribe from requestHandled BEFORE tearing down the counters or
		// destroying this activator. Otherwise, an in-flight HTTP request can
		// fire the event after the counters are gone (or this is destroyed)
		// and crash the worker thread.
		if (_pWebServerDispatcher != nullptr)
		{
			_pWebServerDispatcher->requestHandled -= Poco::delegate(this, &BundleActivator::onWebServerDispatcherRequestHandled);
		}

		if (_pMetricsServer != nullptr)
		{
			_pMetricsServer->stopAll();
			_pMetricsServer.reset();
		}
		_pTimer.reset();
		resetRequestMetrics();
		_generalCollectors.clear();
		_webSessionManagerCollector.reset();
		_pWebServerCollector.reset();
		_pThreadPoolCollector.reset();
		_pWebServerDispatcher.reset();
		_pContext.reset();
	}

	void resetRequestMetrics()
	{
		_pWebServerDispatcherRequestCounter.reset();
		_pWebServerDispatcherRequestByStatusCounter.reset();
		_pWebServerDispatcherRequestDurationHistogram.reset();
	}

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
		if (_pWebServerDispatcher != nullptr)
		{
			_pContext->logger().information("Collecting the metrics of the thread pool %s of the web server."s,
				_pWebServerDispatcher->threadPool().name());
			_pThreadPoolCollector->addThreadPool(_pWebServerDispatcher->threadPool());

			_pWebServerDispatcherRequestCounter.reset(new Poco::Prometheus::IntCounter("osp_web_requests_total"s));
			_pWebServerDispatcherRequestCounter->help("Total number of requests handled"s);
			_pWebServerDispatcherRequestByStatusCounter.reset(new Poco::Prometheus::Counter(
				"osp_web_requests_by_status_total"s,
				Poco::Prometheus::Counter::Params {
					/*.help = */"Total number of requests handled by response status"s,
					/*.labelNames =*/ {"status"s}
				}));
			_pWebServerDispatcherRequestDurationHistogram.reset(new Poco::Prometheus::Histogram(
				"osp_web_request_duration_seconds"s,
				Poco::Prometheus::Histogram::Params {
					/*.help =*/ "Request durations"s,
					/*.labelNames =*/ {},
					/*.buckets =*/ {0.01, 0.1, 0.5, 1.0, 5.0, 10.0, 30.0}
				}));

			_pWebServerDispatcher->requestHandled += Poco::delegate(this, &BundleActivator::onWebServerDispatcherRequestHandled);
		}
	}

	void onWebServerDispatcherUnregistered(const Poco::OSP::ServiceRef::Ptr& /*pServiceRef*/)
	{
		// stop() may have already unsubscribed; guard against double -=.
		if (_pWebServerDispatcher != nullptr)
		{
			_pWebServerDispatcher->requestHandled -= Poco::delegate(this, &BundleActivator::onWebServerDispatcherRequestHandled);
			_pThreadPoolCollector->removeThreadPool(_pWebServerDispatcher->threadPool());
		}
		resetRequestMetrics();
		_pWebServerDispatcher.reset();
	}

	void onWebSessionManagerRegistered(const Poco::OSP::ServiceRef::Ptr& pServiceRef)
	{
		Poco::AutoPtr<Poco::OSP::Web::WebSessionManager> pWSM = pServiceRef->castedInstance<Poco::OSP::Web::WebSessionManager>();
		if (pWSM != nullptr)
		{
			_pContext->logger().information("Creating WebSessionCollector for WebSessionManager."s);
			_webSessionManagerCollector.reset(new WebSessionCollector(pWSM));
		}
	}

	void onWebSessionManagerUnregistered(const Poco::OSP::ServiceRef::Ptr& /*pServiceRef*/)
	{
		_webSessionManagerCollector.reset();
	}

	void onWebServerRegistered(const Poco::OSP::ServiceRef::Ptr& pServiceRef)
	{
		Poco::AutoPtr<Poco::OSP::Web::WebServerService> pWSS = pServiceRef->castedInstance<Poco::OSP::Web::WebServerService>();
		if (pWSS != nullptr)
		{
			_pContext->logger().information("Collecting the metrics of the web server at %s:%hu."s, pWSS->host(), pWSS->port());
			_pWebServerCollector->addServer(pWSS);
		}
	}

	void onWebServerUnregistered(const Poco::OSP::ServiceRef::Ptr& pServiceRef)
	{
		_pWebServerCollector->removeServer(pServiceRef->castedInstance<Poco::OSP::Web::WebServerService>());
	}

	void onWebServerDispatcherRequestHandled(const Poco::OSP::Web::WebServerDispatcher::RequestHandledEvent& event)
	{
		_pWebServerDispatcherRequestCounter->inc();
		_pWebServerDispatcherRequestByStatusCounter->labels({Poco::NumberFormatter::format(static_cast<int>(event.response.getStatus()))}).inc();
		_pWebServerDispatcherRequestDurationHistogram->observe(event.duration);
	}

private:
	static constexpr int MAX_LOG_INTERVAL = std::numeric_limits<Poco::Int32>::max()/1000;
		/// The longest log interval in seconds. The timer takes the interval in
		/// milliseconds as a long, which has 32 bits on Windows.

	Poco::OSP::BundleContext::Ptr _pContext;
	Poco::OSP::Web::WebServerDispatcher::Ptr _pWebServerDispatcher;
	Poco::OSP::ServiceListener::Ptr _pWebServerDispatcherListener;
	Poco::OSP::ServiceListener::Ptr _pWebServerListener;
	Poco::OSP::ServiceListener::Ptr _pWebSessionManagerListener;
	CollectorPtr<Poco::Prometheus::IntCounter> _pWebServerDispatcherRequestCounter;
	CollectorPtr<Poco::Prometheus::Counter> _pWebServerDispatcherRequestByStatusCounter;
	CollectorPtr<Poco::Prometheus::Histogram> _pWebServerDispatcherRequestDurationHistogram;
	std::unique_ptr<Poco::Net::HTTPServer> _pMetricsServer;
	std::vector<CollectorPtr<Poco::Prometheus::Collector>> _generalCollectors;
	CollectorPtr<Poco::Prometheus::Collector> _webSessionManagerCollector;
	CollectorPtr<WebServerCollector> _pWebServerCollector;
	CollectorPtr<ThreadPoolCollector> _pThreadPoolCollector;
	std::unique_ptr<Poco::Util::Timer> _pTimer;
};


} // namespace Poco::OSP::Prometheus


POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
	POCO_EXPORT_CLASS(Poco::OSP::Prometheus::BundleActivator)
POCO_END_MANIFEST
