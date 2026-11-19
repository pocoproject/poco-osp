//
// ThreadPoolCollector.h
//
// Library: OSP/Prometheus
// Package: Prometheus
// Module:  ThreadPoolCollector
//
// Definition of the ThreadPoolCollector class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Prometheus_ThreadPoolCollector_INCLUDED
#define OSP_Prometheus_ThreadPoolCollector_INCLUDED


#include "Poco/Prometheus/Collector.h"
#include "Poco/Prometheus/Exporter.h"
#include "Poco/Prometheus/Metric.h"
#include "Poco/Mutex.h"
#include "Poco/ThreadPool.h"
#include <functional>
#include <memory>
#include <vector>


namespace Poco::OSP::Prometheus {


class ThreadPoolCollector: public Poco::Prometheus::Collector
	/// Exports poco_threadpool_max_threads, poco_threadpool_used_threads and
	/// poco_threadpool_allocated_threads, one sample per thread pool with the label "name" set to its name.
{
public:
	ThreadPoolCollector();
		/// Creates a ThreadPoolCollector without thread pools and registers it
		/// with the default registry.

	~ThreadPoolCollector() override;
		/// Destroys the ThreadPoolCollector.

	ThreadPoolCollector(const ThreadPoolCollector&) = delete;
	ThreadPoolCollector(ThreadPoolCollector&&) = delete;
	ThreadPoolCollector& operator = (const ThreadPoolCollector&) = delete;
	ThreadPoolCollector& operator = (ThreadPoolCollector&&) = delete;

	void addThreadPool(const Poco::ThreadPool& threadPool);
		/// Adds the samples of the given thread pool, whose name must be unique in the collector.
		/// The thread pool must exist until it is removed or the collector is destroyed.

	void removeThreadPool(const Poco::ThreadPool& threadPool);
		/// Removes the samples of the given thread pool.

	// Collector
	void exportTo(Poco::Prometheus::Exporter& exporter) const override;
		/// Writes the metrics of all thread pools to the Exporter.

private:
	struct PoolMetric
	{
		std::unique_ptr<Poco::Prometheus::Metric> pMetric;
		std::function<int(const Poco::ThreadPool&)> value;
	};

	std::vector<PoolMetric> _metrics;
	std::vector<const Poco::ThreadPool*> _threadPools;
	mutable Poco::FastMutex _mutex;
};


} // namespace Poco::OSP::Prometheus


#endif // OSP_Prometheus_ThreadPoolCollector_INCLUDED
