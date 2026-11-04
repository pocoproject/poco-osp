//
// OSPBundleTestSuite.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "OSPBundleTestSuite.h"
#include "BundleDirectoryTest.h"
#include "BundleFileTest.h"
#include "BundleManifestTest.h"
#include "BundleTest.h"
#include "BundleRepositoryTest.h"
#include "Poco/Environment.h"
#include "Poco/Path.h"


std::string OSPBundleTestSuite::findBundlesPath()
{
	return Poco::Environment::get(TEST_BUNDLES_VARIABLE, "");
}


std::string OSPBundleTestSuite::findBundle(const std::string& name)
{
	return Poco::Path(findBundlesPath(), name).toString();
}


CppUnit::Test* OSPBundleTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("OSPBundleTestSuite");

	pSuite->addTest(BundleDirectoryTest::suite());
	pSuite->addTest(BundleFileTest::suite());
	pSuite->addTest(BundleManifestTest::suite());
	pSuite->addTest(BundleTest::suite());
	pSuite->addTest(BundleRepositoryTest::suite());

	return pSuite;
}
