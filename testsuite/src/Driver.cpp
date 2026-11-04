//
// Driver.cpp
//
// Console-based test driver for POCO OSP Core.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "CppUnit/TestRunner.h"
#include "OSPTestSuite.h"
#include "OSPBundleTestSuite.h"
#include "Poco/File.h"
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>


int main(int ac, char** av)
{
	CppUnitInstallTerminateHandler();

	const std::string bundlesPath(OSPBundleTestSuite::testBundlesPath());
	const Poco::File bundlesDir(bundlesPath);
	if (bundlesPath.empty() || !bundlesDir.exists() || !bundlesDir.isDirectory())
	{
		std::cerr
			<< "The environment variable " << OSPBundleTestSuite::TEST_BUNDLES_VARIABLE
			<< " must name the directory of the test bundles (testsuite/bundles in the build directory); "
			<< "ctest sets it. Value: \"" << bundlesPath << "\"\n";
		return 1;
	}

	std::vector<std::string> args;
	args.reserve(static_cast<std::size_t>(ac));
	for (int i = 0; i < ac; ++i)
		args.push_back(std::string(av[i]));
	CppUnit::TestRunner runner;
	runner.addTest("OSPTestSuite", OSPTestSuite::suite());
	CppUnitPocoExceptionText (exc);
	return runner.run(args, exc) ? 0 : 1;
}
