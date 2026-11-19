//
// MetricFamily.h
//
// Library: OSP/Prometheus
// Package: Prometheus
// Module:  MetricFamily
//
// Definition of the MetricFamily class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Prometheus_MetricFamily_INCLUDED
#define OSP_Prometheus_MetricFamily_INCLUDED


#include "Poco/Prometheus/Exporter.h"
#include "Poco/Prometheus/Metric.h"
#include <string>


namespace Poco::OSP::Prometheus {


class MetricFamily: public Poco::Prometheus::Metric
	/// The name, type and help text of a metric whose samples a collector writes itself:
	/// one header, then one sample per object it collects from.
{
public:
	MetricFamily() = delete;

	MetricFamily(Type type, const std::string& name, const std::string& help);
		/// Creates the MetricFamily without registering it.

	~MetricFamily() override;
		/// Destroys the MetricFamily.

	MetricFamily(const MetricFamily&) = delete;
	MetricFamily(MetricFamily&&) = delete;
	MetricFamily& operator = (const MetricFamily&) = delete;
	MetricFamily& operator = (MetricFamily&&) = delete;

	void exportTo(Poco::Prometheus::Exporter& exporter) const override;
		/// Writes nothing: the collector writes the samples.
};


} // namespace Poco::OSP::Prometheus


#endif // OSP_Prometheus_MetricFamily_INCLUDED
