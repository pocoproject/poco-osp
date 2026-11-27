//
// OSPBundleTestSuite.h
//
// Definition of the OSPBundleTestSuite class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSPBundleTestSuite_INCLUDED
#define OSPBundleTestSuite_INCLUDED


#include "CppUnit/TestSuite.h"
#include <string>


class OSPBundleTestSuite
{
public:
	static constexpr const char* TEST_BUNDLES_VARIABLE = "POCO_OSP_TEST_BUNDLES";
		/// The environment variable that holds the absolute path
		/// of the test bundles in the build directory.

	static CppUnit::Test* suite();

	[[nodiscard]] static std::string testBundlesPath();
		/// Returns the path of the test bundles, or an empty string
		/// if the environment variable is not set.

	[[nodiscard]] static std::string findBundle(const std::string& name);
		/// Returns the path of the test bundle with the given name.
};


#endif // OSPBundleTestSuite_INCLUDED
