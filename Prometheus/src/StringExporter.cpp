//
// StringExporter.cpp
//
// Copyright (c) 2024, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "StringExporter.h"
#include "Poco/NumberFormatter.h"
#include <cmath>
#include <vector>
#include <ostream>


using namespace std::string_literals;


namespace Poco::OSP::Prometheus {


namespace
{
	[[nodiscard]] std::string escape(const std::string& str)
	{
		std::string result;
		for (char c: str)
		{
			switch (c)
			{
			case '\\':
				result += "\\\\";
				break;
			case '\n':
				result += "\\n";
				break;
			case '"':
				result += "\\\"";
				break;
			default:
				result += c;
			}
		}
		return result;
	}

	template <typename T>
	void writeSampleImpl(std::ostream& stream, const Poco::Prometheus::Metric& metric,
		const std::vector<std::string>& labelNames, const std::vector<std::string>& labelValues, T&& value,
		const Poco::Timestamp& /*timestamp*/)
	{
		poco_assert_dbg (labelNames.size() == labelValues.size());

		stream << metric.name();
		if (!labelNames.empty())
		{
			stream << '{';
			for (std::size_t i = 0; i < labelNames.size(); i++)
			{
				if (i > 0) stream << ',';
				stream << labelNames[i] << "=\"" << escape(labelValues[i]) << '"';
			}
			stream << '}';
		}
		stream << '=' << value << ' ';
	}
}


void StringExporter::writeHeader(const Poco::Prometheus::Metric&)
{
}


void StringExporter::writeSample(const Poco::Prometheus::Metric& metric, const std::vector<std::string>& labelNames, const std::vector<std::string>& labelValues, float value, const Poco::Timestamp& timestamp)
{
	writeSample(metric, labelNames, labelValues, static_cast<double>(value), timestamp);
}


void StringExporter::writeSample(const Poco::Prometheus::Metric& metric, const std::vector<std::string>& labelNames, const std::vector<std::string>& labelValues, double value, const Poco::Timestamp& timestamp)
{
	std::string valueString;
	if (std::isinf(value))
	{
		if (value > 0)
			valueString = "+Inf";
		else
			valueString = "-Inf";
	}
	else if (std::isnan(value))
	{
		valueString = "NaN";
	}
	else
	{
		valueString = Poco::NumberFormatter::format(value);
	}
	writeSampleImpl(_stream, metric, labelNames, labelValues, valueString, timestamp);
}


void StringExporter::writeSample(const Poco::Prometheus::Metric& metric, const std::vector<std::string>& labelNames, const std::vector<std::string>& labelValues, Poco::UInt32 value, const Poco::Timestamp& timestamp)
{
	writeSampleImpl(_stream, metric, labelNames, labelValues, value, timestamp);
}


void StringExporter::writeSample(const Poco::Prometheus::Metric& metric, const std::vector<std::string>& labelNames, const std::vector<std::string>& labelValues, Poco::Int32 value, const Poco::Timestamp& timestamp)
{
	writeSampleImpl(_stream, metric, labelNames, labelValues, value, timestamp);
}


void StringExporter::writeSample(const Poco::Prometheus::Metric& metric, const std::vector<std::string>& labelNames, const std::vector<std::string>& labelValues, Poco::UInt64 value, const Poco::Timestamp& timestamp)
{
	writeSampleImpl(_stream, metric, labelNames, labelValues, value, timestamp);
}


void StringExporter::writeSample(const Poco::Prometheus::Metric& metric, const std::vector<std::string>& labelNames, const std::vector<std::string>& labelValues, Poco::Int64 value, const Poco::Timestamp& timestamp)
{
	writeSampleImpl(_stream, metric, labelNames, labelValues, value, timestamp);
}


void StringExporter::writeSample(const Poco::Prometheus::Metric& metric, const std::vector<std::string>& labelNames, const std::vector<std::string>& labelValues, const std::string& value, const Poco::Timestamp& timestamp)
{
	writeSampleImpl(_stream, metric, labelNames, labelValues, value, timestamp);
}


} // namespace Poco::OSP::Prometheus
