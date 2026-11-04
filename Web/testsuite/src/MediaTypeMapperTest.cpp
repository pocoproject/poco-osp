//
// MediaTypeMapperTest.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "MediaTypeMapperTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/Test.h"
#include "CppUnit/TestCase.h"
#include "Poco/OSP/Web/MediaTypeMapper.h"
#include "Poco/Bugcheck.h"
#include <atomic>
#include <sstream>
#include <string>
#include <thread>


using namespace Poco::OSP::Web;


MediaTypeMapperTest::MediaTypeMapperTest(const std::string& name): CppUnit::TestCase(name)
{
}


MediaTypeMapperTest::~MediaTypeMapperTest() = default;


void MediaTypeMapperTest::testLoad()
{
	std::string mimeTypes(
		"# This is a comment\n"
		"application/activemessage\n"
		"application/atom+xml\tatom\n"
		"application/pdf\tpdf\n"
		"image/jpeg \t jpeg  jpg \t jpe");
	std::istringstream istr(mimeTypes);
	
	MediaTypeMapper mapper;

	mapper.load(istr);
	assertTrue (mapper.find("atom") != mapper.end());
	assertTrue (mapper.find("pdf") != mapper.end());
	assertTrue (mapper.find("unknown") == mapper.end());
}


void MediaTypeMapperTest::testMapIgnoresCase()
{
	MediaTypeMapper mapper;
	mapper.add("PDF", "application/pdf");

	assertEqual (std::string("application/pdf"), mapper.map("pdf"));
	assertEqual (std::string("application/pdf"), mapper.map("Pdf"));
}


void MediaTypeMapperTest::testMapReturnsDefaultType()
{
	MediaTypeMapper mapper;
	assertEqual (std::string("application/octet-stream"), mapper.map("unknown"));
	assertEqual (std::string("application/octet-stream"), mapper.getDefault());

	mapper.setDefault("text/plain");

	assertEqual (std::string("text/plain"), mapper.map("unknown"));
}


void MediaTypeMapperTest::testAddReplacesMapping()
{
	MediaTypeMapper mapper;
	mapper.add("js", "application/x-javascript");

	mapper.add("js", "text/javascript");

	assertEqual (std::string("text/javascript"), mapper.map("js"));
}


void MediaTypeMapperTest::testMapWhileAdding()
{
	// Run under ThreadSanitizer: the service is shared, so add()
	// and map() can be called from different threads.
	MediaTypeMapper mapper;
	mapper.add("html", "text/html");
	std::atomic<bool> done(false);
	std::thread writer([&mapper, &done]()
		{
			for (int i = 0; i < 1000; ++i)
			{
				mapper.add("ext" + std::to_string(i), "application/x-test");
				mapper.add("html", i % 2 == 0 ? "text/html" : "application/xhtml+xml");
			}
			done = true;
		});

	bool known = true;
	while (!done)
	{
		known = known && mapper.map("html") != mapper.getDefault();
	}
	writer.join();

	assertTrue (known);
	assertEqual (std::string("application/x-test"), mapper.map("ext999"));
}


void MediaTypeMapperTest::setUp()
{
}


void MediaTypeMapperTest::tearDown()
{
}


CppUnit::Test* MediaTypeMapperTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("MediaTypeMapperTest");

	CppUnit_addTest(pSuite, MediaTypeMapperTest, testLoad);
	CppUnit_addTest(pSuite, MediaTypeMapperTest, testMapIgnoresCase);
	CppUnit_addTest(pSuite, MediaTypeMapperTest, testMapReturnsDefaultType);
	CppUnit_addTest(pSuite, MediaTypeMapperTest, testAddReplacesMapping);
	CppUnit_addTest(pSuite, MediaTypeMapperTest, testMapWhileAdding);

	return pSuite;
}
