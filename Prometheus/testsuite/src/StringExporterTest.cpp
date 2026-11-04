//
// StringExporterTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "StringExporterTest.h"
#include "StringExporter.h"
#include "Poco/Prometheus/CallbackMetric.h"
#include "Poco/Types.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include <limits>
#include <string>
#include <vector>


using Poco::OSP::Prometheus::StringExporter;
using namespace std::string_literals;


StringExporterTest::StringExporterTest(const std::string& name): CppUnit::TestCase(name)
{
}


StringExporterTest::~StringExporterTest() = default;


void StringExporterTest::testSamplesAreSeparatedBySpaces()
{
	Poco::Prometheus::CallbackIntGauge connections("connections"s, nullptr, []() { return Poco::Int64(3); });
	Poco::Prometheus::CallbackIntGauge threads("threads"s, nullptr, []() { return Poco::Int64(2); });
	StringExporter exporter;

	connections.exportTo(exporter);
	exporter.writeSample(threads, {"pool"s, "kind"s}, {"default"s, "a\"b"s}, Poco::Int64(2));

	assertEqual ("connections=3 threads{pool=\"default\",kind=\"a\\\"b\"}=2 "s, exporter.toString());
}


void StringExporterTest::testSpecialValues()
{
	Poco::Prometheus::CallbackGauge gauge("gauge"s, nullptr, []() { return 0.0; });
	StringExporter exporter;

	exporter.writeSample(gauge, {}, {}, std::numeric_limits<double>::infinity());
	exporter.writeSample(gauge, {}, {}, -std::numeric_limits<double>::infinity());
	exporter.writeSample(gauge, {}, {}, std::numeric_limits<double>::quiet_NaN());

	assertEqual ("gauge=+Inf gauge=-Inf gauge=NaN "s, exporter.toString());
}


void StringExporterTest::setUp()
{
}


void StringExporterTest::tearDown()
{
}


CppUnit::Test* StringExporterTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("StringExporterTest");

	CppUnit_addTest(pSuite, StringExporterTest, testSamplesAreSeparatedBySpaces);
	CppUnit_addTest(pSuite, StringExporterTest, testSpecialValues);

	return pSuite;
}
