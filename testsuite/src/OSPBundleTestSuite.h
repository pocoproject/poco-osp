//
// OSPBundleTestSuite.h
//
// Definition of the OSPBundleTestSuite class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSPBundleTestSuite_INCLUDED
#define OSPBundleTestSuite_INCLUDED


#include "CppUnit/TestSuite.h"
#include <string>


class OSPBundleTestSuite
{
public:
	static CppUnit::Test* suite();

	static std::string findBundlesPath();
	static std::string findBundle(const std::string& name);
};


#endif // OSPBundleTestSuite_INCLUDED
