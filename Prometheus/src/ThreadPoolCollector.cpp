//
// ThreadPoolCollector.cpp
//
// Library: OSP/Prometheus
// Package: Prometheus
// Module:  ThreadPoolCollector
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "ThreadPoolCollector.h"
#include "MetricFamily.h"
#include "Poco/Prometheus/Collector.h"
#include "Poco/Prometheus/Exporter.h"
#include "Poco/Prometheus/Metric.h"
#include "Poco/Mutex.h"
#include "Poco/ThreadPool.h"
#include "Poco/Types.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>


using namespace std::string_literals;


namespace Poco::OSP::Prometheus {


ThreadPoolCollector::ThreadPoolCollector():
	Poco::Prometheus::Collector("poco_threadpool"s)
{
	using Type = Poco::Prometheus::Metric::Type;

	_metrics.push_back(PoolMetric{std::make_unique<MetricFamily>(Type::GAUGE, "poco_threadpool_max_threads"s,
		"Maximum number of threads available in the thread pool (capacity)"s),
		[](const Poco::ThreadPool& threadPool) { return threadPool.capacity(); }});
	_metrics.push_back(PoolMetric{std::make_unique<MetricFamily>(Type::GAUGE, "poco_threadpool_used_threads"s,
		"Number of currently used threads"s),
		[](const Poco::ThreadPool& threadPool) { return threadPool.used(); }});
	_metrics.push_back(PoolMetric{std::make_unique<MetricFamily>(Type::GAUGE, "poco_threadpool_allocated_threads"s,
		"Number of currently allocated threads"s),
		[](const Poco::ThreadPool& threadPool) { return threadPool.allocated(); }});
}


ThreadPoolCollector::~ThreadPoolCollector() = default;


void ThreadPoolCollector::addThreadPool(const Poco::ThreadPool& threadPool)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	_threadPools.push_back(&threadPool);
}


void ThreadPoolCollector::removeThreadPool(const Poco::ThreadPool& threadPool)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	_threadPools.erase(std::remove(_threadPools.begin(), _threadPools.end(), &threadPool), _threadPools.end());
}


void ThreadPoolCollector::exportTo(Poco::Prometheus::Exporter& exporter) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	if (_threadPools.empty()) return;

	const std::vector<std::string> labelNames{"name"s};
	for (const auto& metric: _metrics)
	{
		exporter.writeHeader(*metric.pMetric);
		for (const auto* pThreadPool: _threadPools)
		{
			exporter.writeSample(*metric.pMetric, labelNames, {pThreadPool->name()},
				static_cast<Poco::Int64>(metric.value(*pThreadPool)), 0);
		}
	}
}


} // namespace Poco::OSP::Prometheus
