//
// SampleBundlesTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SampleBundlesTest.h"
#include "TestApplication.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/Zip/ZipArchive.h"
#include "Poco/FileStream.h"
#include <string>


using namespace std::string_literals;


namespace
{
	const std::string LIBRARY_DIRECTORY("bin/"s);
}


SampleBundlesTest::SampleBundlesTest(const std::string& name): CppUnit::TestCase(name)
{
}


SampleBundlesTest::~SampleBundlesTest() = default;


void SampleBundlesTest::testDataOnlyBundlesContainNoLibrary()
{
	for (const auto& bundle: {"com.appinf.osp.samples.webpage"s, "com.appinf.osp.samples.extension"s})
	{
		Poco::FileInputStream istr(TestApplication::bundleFile(bundle));
		const Poco::Zip::ZipArchive archive(istr);
		bool hasExtensions = false;
		for (auto it = archive.fileInfoBegin(); it != archive.fileInfoEnd(); ++it)
		{
			assertTrue (it->first.compare(0, LIBRARY_DIRECTORY.size(), LIBRARY_DIRECTORY) != 0);
			hasExtensions = hasExtensions || it->first == "extensions.xml"s;
		}
		assertTrue (hasExtensions);
	}
}


CppUnit::Test* SampleBundlesTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SampleBundlesTest");

	CppUnit_addTest(pSuite, SampleBundlesTest, testDataOnlyBundlesContainNoLibrary);

	return pSuite;
}
