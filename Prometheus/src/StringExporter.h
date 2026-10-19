//
// StringExporter.h
//
// Copyright (c) 2024, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_Prometheus_StringExporter_INCLUDED
#define OSP_Prometheus_StringExporter_INCLUDED


#include "Poco/Prometheus/Exporter.h"
#include "Poco/Prometheus/Metric.h"
#include <sstream>


namespace Poco {
namespace OSP {
namespace Prometheus {


class StringExporter: public Poco::Prometheus::Exporter
	/// Simple exporter writing all metrics to a string, comma-separated,
	/// in the form metric=value[, metric=value...].
	/// This is useful for writing metrics to a log.
{
public:
	StringExporter() = default;
		/// Creates the StringExporter for the given output stream.

	std::string toString() const;
		/// Returns a string containing all metrics.

	StringExporter(const StringExporter&) = delete;
	StringExporter& operator = (const StringExporter&) = delete;

	~StringExporter() = default;

	// Exporter
	void writeHeader(const Poco::Prometheus::Metric& metric) override;
	void writeSample(const Poco::Prometheus::Metric& metric, const std::vector<std::string>& labelNames, const std::vector<std::string>& labelValues, float value, const Poco::Timestamp& timestamp = 0) override;
	void writeSample(const Poco::Prometheus::Metric& metric, const std::vector<std::string>& labelNames, const std::vector<std::string>& labelValues, double value, const Poco::Timestamp& timestamp = 0) override;
	void writeSample(const Poco::Prometheus::Metric& metric, const std::vector<std::string>& labelNames, const std::vector<std::string>& labelValues, Poco::UInt32 value, const Poco::Timestamp& timestamp = 0) override;
	void writeSample(const Poco::Prometheus::Metric& metric, const std::vector<std::string>& labelNames, const std::vector<std::string>& labelValues, Poco::Int32 value, const Poco::Timestamp& timestamp = 0) override;
	void writeSample(const Poco::Prometheus::Metric& metric, const std::vector<std::string>& labelNames, const std::vector<std::string>& labelValues, Poco::UInt64 value, const Poco::Timestamp& timestamp = 0) override;
	void writeSample(const Poco::Prometheus::Metric& metric, const std::vector<std::string>& labelNames, const std::vector<std::string>& labelValues, Poco::Int64 value, const Poco::Timestamp& timestamp = 0) override;
	void writeSample(const Poco::Prometheus::Metric& metric, const std::vector<std::string>& labelNames, const std::vector<std::string>& labelValues, const std::string& value, const Poco::Timestamp& timestamp = 0) override;

private:
	std::ostringstream _stream;
};


//
// inlines
//
inline std::string StringExporter::toString() const
{
	return _stream.str();
}


} } } // namespace Poco::OSP::Prometheus


#endif // OSP_Prometheus_StringExporter_INCLUDED
