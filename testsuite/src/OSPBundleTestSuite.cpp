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
#include "Poco/File.h"


std::string OSPBundleTestSuite::findBundlesPath()
{
	using Poco::Path;
	using Poco::File;

	std::string bundles("bundles");
	if (Poco::Environment::has("MACCHINA_BASE"))
	{
		Path p(Poco::Environment::get("MACCHINA_BASE"));
		p.makeDirectory();
		p.pushDirectory("platform");
		p.pushDirectory("OSP");
		p.pushDirectory("testsuite");
		p.pushDirectory("bundles");
		File f(p);
		if (f.exists())
			return p.toString();
	}
	std::string cwd(Path::current());
	Path cwdPath(cwd);
	Path p(cwdPath, bundles);
	while (p.depth() > 0)
	{
		File f(p);
		if (f.exists())
			return p.toString();
		p.popDirectory();
	}
	return std::string();
}


std::string OSPBundleTestSuite::findBundle(const std::string& name)
{
	std::string bundlesPath = findBundlesPath();
	if (bundlesPath.empty())
		return std::string();
	return Poco::Path(bundlesPath, name).toString();
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
