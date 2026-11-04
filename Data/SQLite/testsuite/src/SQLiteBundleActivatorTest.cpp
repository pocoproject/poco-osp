//
// SQLiteBundleActivatorTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SQLiteBundleActivatorTest.h"
#include "TemporaryBundle.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/Data/SQLite/Connector.h"
#include "Poco/Data/Extraction.h"
#include "Poco/Data/Session.h"
#include "Poco/Data/Statement.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/ClassLoader.h"
#include "Poco/Path.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Types.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include <memory>
#include <string>


using Poco::OSP::PreferencesService;
using namespace std::string_literals;


SQLiteBundleActivatorTest::SQLiteBundleActivatorTest(const std::string& name): CppUnit::TestCase(name)
{
}


SQLiteBundleActivatorTest::~SQLiteBundleActivatorTest() = default;


void SQLiteBundleActivatorTest::testSoftHeapLimitIsSetInBytes()
{
	TemporaryBundle bundles(""s);
	Poco::TemporaryFile persistency;
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;
	pSettings->setInt("data.sqlite.softHeapLimit"s, 8000000);
	bundles.registry().registerService(PreferencesService::SERVICE_NAME, new PreferencesService(Poco::Path(persistency.path()), pSettings), Poco::OSP::Properties());
	Poco::ClassLoader<Poco::OSP::BundleActivator> loader;
	loader.loadLibrary(SQLITE_BUNDLE_LIBRARY);
	std::unique_ptr<Poco::OSP::BundleActivator> pActivator(loader.create("SQLiteBundleActivator"s));

	pActivator->start(bundles.context());

	Poco::Data::Session session("SQLite"s, ":memory:"s);
	Poco::Int64 limit = 0;
	session << "PRAGMA soft_heap_limit"s, Poco::Data::Keywords::into(limit), Poco::Data::Keywords::now;
	assertEqual (8000000LL, limit);
	pActivator->stop(bundles.context());
}


void SQLiteBundleActivatorTest::setUp()
{
}


void SQLiteBundleActivatorTest::tearDown()
{
	Poco::Data::SQLite::Connector::enableSoftHeapLimit(0);
}


CppUnit::Test* SQLiteBundleActivatorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SQLiteBundleActivatorTest");

	CppUnit_addTest(pSuite, SQLiteBundleActivatorTest, testSoftHeapLimitIsSetInBytes);

	return pSuite;
}
