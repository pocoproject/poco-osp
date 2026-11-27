//
// MetricFamily.cpp
//
// Library: OSP/Prometheus
// Package: Prometheus
// Module:  MetricFamily
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "MetricFamily.h"
#include "Poco/Prometheus/Exporter.h"
#include "Poco/Prometheus/Metric.h"
#include <string>


namespace Poco::OSP::Prometheus {


MetricFamily::MetricFamily(Type type, const std::string& name, const std::string& help):
	Poco::Prometheus::Metric(type, name, nullptr)
{
	setHelp(help);
}


MetricFamily::~MetricFamily() = default;


void MetricFamily::exportTo(Poco::Prometheus::Exporter& /*exporter*/) const
{
}


} // namespace Poco::OSP::Prometheus
