//
// WebServerCollectorTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "WebServerCollectorTest.h"
#include "WebServerCollector.h"
#include "Poco/OSP/Web/WebServerService.h"
#include "Poco/Prometheus/Registry.h"
#include "Poco/Prometheus/TextExporter.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/String.h"
#include "Poco/Types.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include <sstream>
#include <string>
#include <utility>


using Poco::OSP::Prometheus::WebServerCollector;
using namespace std::string_literals;


namespace
{
	class WebServerStub: public Poco::OSP::Web::WebServerService
		/// A web server with the given address and fixed statistics.
	{
	public:
		WebServerStub() = delete;

		WebServerStub(std::string host, Poco::UInt16 port, bool secure):
			_host(std::move(host)),
			_port(port),
			_secure(secure)
		{
		}

		~WebServerStub() override = default;

		WebServerStub(const WebServerStub&) = delete;
		WebServerStub(WebServerStub&&) = delete;
		WebServerStub& operator = (const WebServerStub&) = delete;
		WebServerStub& operator = (WebServerStub&&) = delete;

		int currentThreads() const override { return 1; }
		Poco::Int64 totalConnections() const override { return 10; }
		int currentConnections() const override { return 2; }
		int maxConcurrentConnections() const override { return 3; }
		int queuedConnections() const override { return 0; }
		int refusedConnections() const override { return 0; }
		std::string host() const override { return _host; }
		Poco::UInt16 port() const override { return _port; }
		bool secure() const override { return _secure; }
		Poco::Net::HTTPServerParams::Ptr params() const override { return new Poco::Net::HTTPServerParams; }

	private:
		const std::string _host;
		const Poco::UInt16 _port;
		const bool _secure;
	};

	std::string exported(const WebServerCollector& collector)
	{
		std::ostringstream ostr;
		Poco::Prometheus::TextExporter exporter(ostr);
		collector.exportTo(exporter);
		return ostr.str();
	}

	int count(const std::string& text, const std::string& line)
		/// Returns the number of lines in text that start with line.
	{
		std::istringstream istr(text);
		int result = 0;
		std::string current;
		while (std::getline(istr, current))
		{
			if (Poco::startsWith(current, line)) ++result;
		}
		return result;
	}
}


WebServerCollectorTest::WebServerCollectorTest(const std::string& name): CppUnit::TestCase(name)
{
}


WebServerCollectorTest::~WebServerCollectorTest() = default;


void WebServerCollectorTest::testOneHeaderAndOneSamplePerServer()
{
	WebServerCollector collector;
	collector.addServer(new WebServerStub("0.0.0.0"s, 22080, false));
	collector.addServer(new WebServerStub("0.0.0.0"s, 22443, true));

	const std::string text = exported(collector);

	assertEqual (1, count(text, "# TYPE osp_web_current_connections "s));
	assertEqual (1, count(text, "# HELP osp_web_current_connections "s));
	assertEqual (1, count(text, "# TYPE osp_web_total_connections "s));
	assertEqual (1, count(text, "osp_web_current_connections{server=\"http://0.0.0.0:22080\"} 2"s));
	assertEqual (1, count(text, "osp_web_current_connections{server=\"https://0.0.0.0:22443\"} 2"s));
}


void WebServerCollectorTest::testRemovedServerHasNoSamples()
{
	WebServerCollector collector;
	WebServerCollector::WebServerPtr pServer = new WebServerStub("0.0.0.0"s, 22080, false);
	collector.addServer(pServer);
	collector.addServer(new WebServerStub("0.0.0.0"s, 22443, true));

	collector.removeServer(pServer);

	const std::string text = exported(collector);
	assertEqual (0, count(text, "osp_web_current_connections{server=\"http://0.0.0.0:22080\"}"s));
	assertEqual (1, count(text, "osp_web_current_connections{server=\"https://0.0.0.0:22443\"}"s));
}


void WebServerCollectorTest::setUp()
{
}


void WebServerCollectorTest::tearDown()
{
	// The collectors register themselves with the default registry,
	// which keeps a pointer to them.
	Poco::Prometheus::Registry::defaultRegistry().unregisterCollector("osp_web_server"s);
}


CppUnit::Test* WebServerCollectorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebServerCollectorTest");

	CppUnit_addTest(pSuite, WebServerCollectorTest, testOneHeaderAndOneSamplePerServer);
	CppUnit_addTest(pSuite, WebServerCollectorTest, testRemovedServerHasNoSamples);

	return pSuite;
}
